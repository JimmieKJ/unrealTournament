// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DSP/Reverb.h"

namespace Audio
{
	const float FFreeVerb::FixedGain = 0.015f;
	const float FFreeVerb::ScaleWet = 3.0f;
	const float FFreeVerb::ScaleDry = 2.0f;
	const float FFreeVerb::ScaleDamp = 0.4f;
	const float FFreeVerb::ScaleRoom = 0.28f;
	const float FFreeVerb::OffsetRoom = 0.7;

	FFreeVerb::FFreeVerb(const float InSampleRate)
	{
		// Hard-coded delay lines tuned to 44.1 kHz sample rate
		CombFilterDelayLengths.AddDefaulted(NumCombFilters);

		const float OneOverSamplesPerMs = 1000.0f / InSampleRate;

		CombFilterDelayLengths[0] = OneOverSamplesPerMs * 1557.0f;
		CombFilterDelayLengths[1] = OneOverSamplesPerMs * 1617.0f;
		CombFilterDelayLengths[2] = OneOverSamplesPerMs * 1491.0f;
		CombFilterDelayLengths[3] = OneOverSamplesPerMs * 1422.0f;
		CombFilterDelayLengths[4] = OneOverSamplesPerMs * 1277.0f;
		CombFilterDelayLengths[5] = OneOverSamplesPerMs * 1356.0f;
		CombFilterDelayLengths[6] = OneOverSamplesPerMs * 1188.0f;
		CombFilterDelayLengths[7] = OneOverSamplesPerMs * 1116.0f;

		AllpassFilterDelayLengths.AddDefaulted(NumAllPassFilters);
		AllpassFilterDelayLengths[0] = OneOverSamplesPerMs * 225.0f;
		AllpassFilterDelayLengths[1] = OneOverSamplesPerMs * 556.0f;
		AllpassFilterDelayLengths[2] = OneOverSamplesPerMs * 441.0f;
		AllpassFilterDelayLengths[3] = OneOverSamplesPerMs * 341.0f;

		// Scale these delay line lengths according to the actual sample rate
		float SampleRateScale = InSampleRate / 44100.0f;
		for (int32 i = 0; i < NumCombFilters; ++i)
		{
			CombFilterDelayLengths[i] = FMath::FloorToInt(SampleRateScale * CombFilterDelayLengths[i]);
		}

		for (int32 i = 0; i < NumAllPassFilters; ++i)
		{
			AllpassFilterDelayLengths[i] = FMath::FloorToInt(SampleRateScale * AllpassFilterDelayLengths[i]);
		}

		CombDelayLeft.AddDefaulted(NumCombFilters);
		CombDelayRight.AddDefaulted(NumCombFilters);
		for (int32 i = 0; i < NumCombFilters; ++i)
		{
			CombDelayLeft[i].Init(InSampleRate, CombFilterDelayLengths[i]);
			CombDelayLeft[i].SetDelayMsec(CombFilterDelayLengths[i]);
			CombDelayRight[i].Init(InSampleRate, CombFilterDelayLengths[i]);
			CombDelayRight[i].SetDelayMsec(CombFilterDelayLengths[i]);
		}

		AllPassDelayLeft.AddDefaulted(NumAllPassFilters);
		AllPassDelayRight.AddDefaulted(NumAllPassFilters);
		for (int32 i = 0; i < NumAllPassFilters; ++i)
		{
			AllPassDelayRight[i].Init(InSampleRate, CombFilterDelayLengths[i]);
			AllPassDelayRight[i].SetDelayMsec(CombFilterDelayLengths[i]);
			AllPassDelayLeft[i].Init(InSampleRate, CombFilterDelayLengths[i]);
			AllPassDelayLeft[i].SetDelayMsec(CombFilterDelayLengths[i]);
		}

		SetWetLevel(0.75f);

		// Feedback attenuation in low pass feedback comb filter
		RoomSizeSetting = (0.75f * ScaleRoom) + OffsetRoom;
		DampSetting = -0.25f * ScaleDamp;

		Width = 1.0f;
		bIsFrozen = false;

		CombLowPassLeft.AddDefaulted(NumCombFilters);
		CombLowPassRight.AddDefaulted(NumCombFilters);

		UpdateParameters();

		AllPassCoefficientG = 0.5f;
	}

	void FFreeVerb::SetWetLevel(const float InWetLevel)
	{
		WetLevel = InWetLevel;
		UpdateParameters();
	}

	void FFreeVerb::SetRoomSize(const float InRoomSize)
	{
		RoomSizeSetting = (InRoomSize * ScaleRoom) + OffsetRoom;
		UpdateParameters();
	}

	float FFreeVerb::GetRoomSize() const
	{
		return (RoomSizeSetting - OffsetRoom) / ScaleRoom;
	}

	void FFreeVerb::SetDamping(const float InDamping)
	{
		DampSetting = InDamping * ScaleDamp;
		UpdateParameters();
	}

	float FFreeVerb::GetDamping() const
	{
		return DampSetting / ScaleDamp;
	}

	void FFreeVerb::SetStereoWidth(float InWidth)
	{
		Width = InWidth;
		UpdateParameters();
	}

	float FFreeVerb::GetStereoWidth() const
	{
		return Width;
	}

	void FFreeVerb::SetFreeze(const bool bInIsFrozen)
	{
		bIsFrozen = bInIsFrozen;
		UpdateParameters();
	}

	bool FFreeVerb::IsFrozen() const
	{
		return bIsFrozen;
	}

	void FFreeVerb::Clear()
	{
		for (int32 i = 0; i < NumCombFilters; ++i)
		{
			CombDelayLeft[i].Reset();
			CombDelayRight[i].Reset();
		}

		for (int32 i = 0; i < NumAllPassFilters; ++i)
		{
			AllPassDelayLeft[i].Reset();
			AllPassDelayRight[i].Reset();
		}
	}

	void FFreeVerb::UpdateParameters()
	{
		float ActualWet = ScaleWet * WetLevel;
		Dry = ScaleDry * (1.0f - Wet1);

		ActualWet /= (ActualWet + Dry);
		Dry /= (ActualWet + Dry);

		Wet1 = ActualWet * (0.5f * Width + 0.5f);
		Wet2 = ActualWet * (-0.5f * Width + 0.5f);

		if (bIsFrozen)
		{
			RoomSize = 1.0f;
			Damp = 0.0f;
			Gain = 0.0f;
		}
		else
		{
			RoomSize = RoomSizeSetting;
			Damp = DampSetting;
			Gain = FixedGain;
		}

		// Set the low pass filters
		for (int i = 0; i < NumCombFilters; ++i)
		{
			CombLowPassLeft[i].SetCoefficients(1.0f - Damp, -Damp);
			CombLowPassRight[i].SetCoefficients(1.0f - Damp, -Damp);
		}
	}

	void FFreeVerb::operator()(const TArray<float>& InAudio, TArray<float>& OutAudio)
	{
		// Sum left and right channels to a mono channel
		const float InputSum = (InAudio[0] + InAudio[1]) * Gain;

		float OutputLeft = 0.0f;
		float OutputRight = 0.0f;

		// Process the bank of parallel lowpass-feedback comb filters
		for (int32 i = 0; i < NumCombFilters; ++i)
		{
			// Get the comb filter's next sample value
			float DelayNextOut = CombDelayLeft[i].Read();

			// Feed it through the LPF, scale with the room size add to the input
			float Yn = InputSum + (RoomSize * CombLowPassLeft[i](DelayNextOut));

			// Add the sample to the comb filter delay line
			CombDelayLeft[i](Yn);

			// Add the result of this comb filter to the left channel output
			OutputLeft += Yn;

			DelayNextOut = CombDelayRight[i].Read();

			Yn = InputSum + (RoomSize * CombLowPassRight[i](DelayNextOut));

			CombDelayRight[i](Yn);

			OutputRight += Yn;
		}

		// Now process the series of all pass filters
		for (int32 i = 0; i < NumAllPassFilters; ++i)
		{
			// Compute left channel output
			float NextAllpassOutput = AllPassDelayLeft[i].Read();
			float AllpassInput = OutputLeft + AllPassCoefficientG * NextAllpassOutput;
			AllPassDelayLeft[i](AllpassInput);
			OutputLeft = -AllpassInput + (1.0f + AllPassCoefficientG) * NextAllpassOutput;

			// Compute right channel output
			NextAllpassOutput = AllPassDelayRight[i].Read();
			AllpassInput = OutputRight + (AllPassCoefficientG * NextAllpassOutput);
			AllPassDelayRight[i](AllpassInput);
			OutputRight = -AllpassInput + (1.0f + AllPassCoefficientG) * NextAllpassOutput;
		}

		// Now blend the left/right channels to the final output
		OutAudio[0] = OutputLeft * Wet1 + OutputRight * Wet2 + InAudio[0] * Dry;
		OutAudio[1] = OutputRight * Wet1 + OutputLeft * Wet2 + InAudio[1] * Dry;
	}

}
