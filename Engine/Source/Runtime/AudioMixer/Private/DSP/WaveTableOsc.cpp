// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DSP/WaveTableOsc.h"
#include "AudioMixer.h"

namespace Audio
{

	FWaveTableOsc::FWaveTableOsc()
		: SampleRate(0)
		, ReadIndex(0.0f)
		, QuadPhaseReadIndex(0.0f)
		, ReadDelta(0.0f)
	{
	}

	FWaveTableOsc::~FWaveTableOsc()
	{
	}

	void FWaveTableOsc::Init(int32 InSampleRate, int32 TableSize)
	{
		AUDIO_MIXER_CHECK(InSampleRate > 0);

		SampleRate = InSampleRate;
		
		Generate(TableSize);
	}

	void FWaveTableOsc::SetFrequency(const float InFrequencyHz)
	{
		AUDIO_MIXER_CHECK(InFrequencyHz > 0);
		ReadDelta = (InFrequencyHz / (float)SampleRate) * (float)WaveTable.Num();
	}

	void FWaveTableOsc::SetPolarity(const bool bInIsUnipolar)
	{
		bIsUnipolar = bInIsUnipolar;
	}

	void FWaveTableOsc::operator()(float* OutSample, float* OutQuadSample)
	{
		const int32 CurrIndex = (int32)ReadIndex;
		const int32 NextIndex = (CurrIndex + 1) % WaveTable.Num();

		const int32 CurrQuadIndex = (int32)QuadPhaseReadIndex;
		const int32 NextQuadIndex = (CurrQuadIndex + 1) % WaveTable.Num();

		const float Alpha = ReadIndex - CurrIndex;
		const float QuadAlpha = QuadPhaseReadIndex - CurrQuadIndex;

		*OutSample = FMath::Lerp(WaveTable[CurrIndex], WaveTable[NextIndex], Alpha);
		*OutQuadSample = FMath::Lerp(WaveTable[CurrQuadIndex], WaveTable[NextQuadIndex], Alpha);

		if (bIsUnipolar)
		{
			// Scale from [-1.0, 1.0] to [0.0 1.0]
			*OutSample = 0.5f * (*OutSample + 1.0f);
			*OutQuadSample = 0.5f * (*OutQuadSample + 1.0f);
		}

		ReadIndex += ReadDelta;
		QuadPhaseReadIndex += ReadIndex;

		// Wrap the read indices to within the size of the table
		while (ReadIndex >= (float)WaveTable.Num())
		{
			ReadIndex -= (float)WaveTable.Num();
		}

		while (QuadPhaseReadIndex >= (float)WaveTable.Num())
		{
			QuadPhaseReadIndex -= (float)WaveTable.Num();
		}
	}

	/*
	* FSineWaveTable
	*/
	void FSineWaveTable::Generate(const int32 InTableSize)
	{
		WaveTable.Reset(InTableSize);

		for (int32 i = 0; i < InTableSize; ++i)
		{
			WaveTable.Add(FMath::Sin(2.0f * PI * (float)i / InTableSize));
		}
	}

	/*
	* FSawWaveTable
	*/
	void FSawWaveTable::Generate(const int32 InTableSize)
	{
		WaveTable.Reset(InTableSize);

		/*      
				 /|   /|
		        / |  / |
		       /  | /  |
		      /   |/   |
		*/

		// Using y = mx + b slope/intercept form
		const int32 HalfPoint = 0.5f * InTableSize;
		const float Slope = 1.0f / HalfPoint;

		for (int32 i = 0; i < InTableSize; ++i)
		{
			// Rise if less than half point
			if (i < HalfPoint)
			{
				WaveTable.Add(Slope * i);
			}
			else
			{
				WaveTable.Add(Slope * (i - HalfPoint - 1) - 1.0f);
			}
		}
	}

	/*
	* FBandLimitedSawWaveTable
	*/
	void FBandLimitedSawWaveTable::Generate(const int32 InTableSize)
	{
		WaveTable.Reset(InTableSize);
		WaveTable.AddDefaulted(InTableSize);

		float MaxValue = 0.0f;

		// Generate according to sum of sines, Fourier theorem
		for (int32 i = 0; i < InTableSize; ++i)
		{
			const float TwoPiFraction = 2.0f * PI * (float)i / InTableSize;

			for (int32 k = 1; k <= 6; ++k)
			{
				const float SinusoidComponent = FMath::Sin(TwoPiFraction * k);
				const float Sign = FMath::Pow(-1.0f, (float)(k + 1));
				const float Amplitude = (1.0f / k);
				WaveTable[i] += Sign * Amplitude * SinusoidComponent;
			}

			if (WaveTable[i] > MaxValue)
			{
				MaxValue = WaveTable[i];
			}
		}

		// Normalize
		for (int32 i = 0; i < InTableSize; ++i)
		{
			WaveTable[i] /= MaxValue;
		}
	}

	/*
	* FTriangleWaveTable
	*/
	void FTriangleWaveTable::Generate(const int32 InTableSize)
	{
		WaveTable.Reset(InTableSize);

		/*
		          /\
		         /  \    
		             \  /
		              \/
		*/

		const int32 QuarterPoint = 0.25f * InTableSize;
		const int32 ThirdQuarterPoint = 0.75f * InTableSize;

		const float RisingSlope = 1.0f / QuarterPoint;
		const float FallingSlope = -2.0f / (0.5f * InTableSize);

		for (int32 i = 0; i < InTableSize; ++i)
		{
			// Rising first edge
			if (i < QuarterPoint)
			{
				WaveTable.Add(RisingSlope * i);
			}
			// Falling middle part
			else if (i < ThirdQuarterPoint)
			{
				WaveTable.Add(FallingSlope * (i - QuarterPoint) + 1.0f);
			}
			// Rising last part
			else
			{
				WaveTable.Add(RisingSlope * (i - ThirdQuarterPoint) - 1.0f);
			}
		}
	}

	/*
	* FBandLimitedTriangleWaveTable
	*/
	void FBandLimitedTriangleWaveTable::Generate(const int32 InTableSize)
	{
		WaveTable.Reset(InTableSize);
		WaveTable.AddDefaulted(InTableSize);

		float MaxValue = 0.0f;

		// Generate according to sum of sines, Fourier theorem
		// triangle wave only has odd-harmonics
		for (int32 i = 0; i < InTableSize; ++i)
		{
			const float TwoPiFraction = 2.0f * PI * (float)i / InTableSize;

			for (int32 k = 0; k <= 3; ++k)
			{
				const float SinusoidComponent = FMath::Sin(TwoPiFraction * (2.0f * k + 1.0f));
				const float Sign = FMath::Pow(-1.0f, (float)k);
				const float Amplitude = (1.0f / FMath::Pow((float)(2 * k + 1), 2.0f));

				WaveTable[i] += Sign * Amplitude * SinusoidComponent;
			}

			if (WaveTable[i] > MaxValue)
			{
				MaxValue = WaveTable[i];
			}
		}

		// Normalize
		for (int32 i = 0; i < InTableSize; ++i)
		{
			WaveTable[i] /= MaxValue;
		}
	}

	/*
	* FBandLimitedSquareWaveTable
	*/
	void FSquareWaveTable::Generate(const int32 InTableSize)
	{
		WaveTable.Reset(InTableSize);
		const int32 HalfPoint = 0.5f * InTableSize;

		for (int i = 0; i < InTableSize; ++i)
		{
			if (i < HalfPoint)
			{
				WaveTable.Add(1.0f);
			}
			else
			{
				WaveTable.Add(-1.0f);
			}
		}
	}

	void FBandLimitedSquareWaveTable::Generate(const int32 InTableSize)
	{
		WaveTable.Reset(InTableSize);
		WaveTable.AddDefaulted(InTableSize);

		float MaxValue = 0.0f;

		// Generate according to sum of sines, Fourier theorem
		for (int32 i = 0; i < InTableSize; ++i)
		{
			const float TwoPiFraction = 2.0f * PI * (float)i / InTableSize;

			for (int32 k = 1; k <= 5; ++k)
			{
				const float SinusoidComponent = FMath::Sin(TwoPiFraction * (2.0f * k - 1.0f));
				const float Amplitude = 1.0f / (2.0f * k - 1.0f);

				WaveTable[i] += Amplitude * SinusoidComponent;
			}

			if (WaveTable[i] > MaxValue)
			{
				MaxValue = WaveTable[i];
			}
		}

		// Normalize
		for (int32 i = 0; i < InTableSize; ++i)
		{
			WaveTable[i] /= MaxValue;
		}
	}

}
