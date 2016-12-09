// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DSP/Delay.h"
#include "AudioMixer.h"

namespace Audio
{
	FDelay::FDelay()
		: Feedback(0.0f)
		, DelaySamples(0.0f)
		, OutputGain(0.0f)
		, ReadIndex(0)
		, WriteIndex(0)
		, SampleRate(0)
	{
	}

	FDelay::~FDelay()
	{
	}

	void FDelay::Init(const int32 InSampleRate, const int32 InDelaySizeMsec)
	{
		AUDIO_MIXER_CHECK(InSampleRate != 0);
		SampleRate = InSampleRate;

		// Get the number of samples based on sample rate
		const int32 NumSamples = InSampleRate * ((float)InDelaySizeMsec / 1000.0f);
		AudioBuffer.Reset();
		AudioBuffer.AddDefaulted(NumSamples);

		// Assume the current delay is the entire buffer size
		DelaySamples = NumSamples;
	}

	void FDelay::Reset()
	{
		AUDIO_MIXER_CHECK(SampleRate != 0);

		const int32 BufferSize = AudioBuffer.Num();
		AudioBuffer.Reset();
		AudioBuffer.AddDefaulted(BufferSize);

		// Reset the write index
		WriteIndex = 0;

		// Set the read index based on the current delay in samples
		ReadIndex = -(int32)DelaySamples;

		while (ReadIndex < 0)
		{
			ReadIndex += BufferSize;
		}
	}

	void FDelay::SetFeedback(const float InFeedback)
	{
		AUDIO_MIXER_CHECK(InFeedback >= -1.0f && InFeedback <= 1.0f);
		Feedback = InFeedback;
	}

	void FDelay::SetOutputGainDB(const float InOutputGainDB)
	{
		AUDIO_MIXER_CHECK(InOutputGainDB <= 0.0f);

		OutputGain = FMath::Pow(10.0f, InOutputGainDB / 20.0f);
	}

	void FDelay::SetOutputGain(const float InOutputGain)
	{
		AUDIO_MIXER_CHECK(InOutputGain >= 0.0f && InOutputGain <= 1.0f);

		OutputGain = InOutputGain;
	}

	void FDelay::SetDelayMsec(const float InDelayMsec)
	{
		AUDIO_MIXER_CHECK(SampleRate != 0);

		// Set the current delay in terms of fractional samples
		DelaySamples = InDelayMsec * ((float)SampleRate / 1000.0f);

		// Update the read index based on the current write index
		// Don't touch the current write index, this will allow modulated delay lines.
		ReadIndex = WriteIndex - (int32)DelaySamples;

		// Make sure to wrap the read index to within the bounds of the delay line
		while (ReadIndex < 0)
		{
			ReadIndex += AudioBuffer.Num();
		}
	}

	float FDelay::Read() const
	{
		// Get the previous read index, wrap to buffer size
		const int32 ReadIndexPrev = (ReadIndex == 0) ? AudioBuffer.Num() - 1 : ReadIndex - 1;

		// Get the output at the current read index
		const float Yn = AudioBuffer[ReadIndex];

		// Get sample value at previous read index
		const float YnPrev = AudioBuffer[ReadIndexPrev];

		// Get the lerp alpha for interpolating sample reading
		const float Alpha = DelaySamples - (int32)DelaySamples;

		// Lerp from current read to prev read output
		return FMath::Lerp(Yn, YnPrev, Alpha);
	}

	float FDelay::TapOut(const float TapOutMsec) const
	{
		const float TapDelaySamples = TapOutMsec * ((float)SampleRate / 1000.0f);
		int32 TapReadIndex = WriteIndex - (int32)TapDelaySamples;
		while (TapReadIndex < 0)
		{
			TapReadIndex += AudioBuffer.Num();
		}

		// Get the previous read index, wrap to buffer size
		const int32 TapReadIndexPrev = (TapReadIndex == 0) ? AudioBuffer.Num() - 1 : TapReadIndex - 1;

		// Get current and previous tap read sample values
		const float Yn = AudioBuffer[ReadIndex];
		const float YnPrev = AudioBuffer[TapReadIndexPrev];

		// Get lerp alpha
		const float Alpha = TapDelaySamples - (int32)TapDelaySamples;

		// Lerp from current read to prev read indices
		return FMath::Lerp(Yn, YnPrev, Alpha);
	}

	void FDelay::TapIn(const float InSample)
	{
		AudioBuffer[WriteIndex++] = InSample;

		if (WriteIndex >= AudioBuffer.Num())
		{
			WriteIndex = 0;
		}

		++ReadIndex;

		if (ReadIndex >= AudioBuffer.Num())
		{
			ReadIndex = 0;
		}
	}

	float FDelay::operator()(const float InSample)
	{
		// If there is no delay, then copy input to output
		const float Yn = (DelaySamples == 0.0f) ? InSample : Read();

		// Write in the new sample into the delay line with possible feedback
		TapIn(InSample + Feedback * Yn);

		// Return the output applied with the gain
		return OutputGain * Yn;
	}

	/**
	* FModulatedDelay
	*/
	FModulatedDelay::FModulatedDelay()
		: CurrentWaveTable(&SinLFO)
		, ModulationDepth(0.0f)
		, ModulationFrequency(0.0f)
		, MinDelayTimeMsec(0.0f)
		, MaxDelayTimeMsec(0.0f)
		, DelayOffsetMsec(0.0f)
		, ModulationFeedback(0.0f)
		, WetLevel(1.0f)
		, bIsInQuadPhase(false)
	{
	}

	FModulatedDelay::~FModulatedDelay()
	{
	}

	void FModulatedDelay::Init(int32 InSampleRate)
	{
		SawLFO.Init(InSampleRate);
		SinLFO.Init(InSampleRate);
		Delay.Init(InSampleRate);
	}

	void FModulatedDelay::SetLFO(const EWaveTable::Type InLFOType)
	{
		switch (InLFOType)
		{
		default:
		case EWaveTable::SineWaveTable:
			CurrentWaveTable = &SinLFO;
			break;
		case EWaveTable::SawWaveTable:
			CurrentWaveTable = &SawLFO;
			break;
		case EWaveTable::TriangleWaveTable:
			CurrentWaveTable = &TriLFO;
			break;
		case EWaveTable::SquareWaveTable:
			CurrentWaveTable = &SquareLFO;
			break;
		}
	}

	void FModulatedDelay::SetModulationFeedback(const float InModulationFeedback)
	{
		Delay.SetFeedback(InModulationFeedback);
	}

	void FModulatedDelay::SetWetLevel(const float InWetLevel)
	{
		AUDIO_MIXER_CHECK(InWetLevel >= 0.0f && InWetLevel <= 1.0f);
		WetLevel = InWetLevel;
	}

	void FModulatedDelay::SetMinAndMaxDelayMSec(const float InMinDelayTimeMsec, const float InMaxDelayTimeMsec)
	{
		AUDIO_MIXER_CHECK(InMinDelayTimeMsec >= 0.0f && InMaxDelayTimeMsec <= 1.0f);
		AUDIO_MIXER_CHECK(InMinDelayTimeMsec < InMaxDelayTimeMsec);

		MinDelayTimeMsec = InMinDelayTimeMsec;
		MaxDelayTimeMsec = InMaxDelayTimeMsec;
	}

	void FModulatedDelay::SetModulationDelayOffsetMsec(const float InDelayOffsetMsec)
	{
		AUDIO_MIXER_CHECK(InDelayOffsetMsec >= 0.0f);
		DelayOffsetMsec = InDelayOffsetMsec;
	}

	void FModulatedDelay::SetModulationFrequency(const float InModulationFrequency)
	{
		SawLFO.SetFrequency(ModulationFrequency);
		SinLFO.SetFrequency(ModulationFrequency);
	}

	float FModulatedDelay::operator()(const float InSample)
	{
		// Get the current value of our LFO wavetable
		float Yn = 0.0f;			// Normal phase output
		float Yqn = 0.0f;			// Quad phase output

		(*CurrentWaveTable)(&Yn, &Yqn);

		// Use modulation depth and min/max delay time range to compute a new delay time 
		const float LFOSample = bIsInQuadPhase ? Yqn : Yn;
		const float NewDelayMsec = ModulationDepth * LFOSample * (MaxDelayTimeMsec - MinDelayTimeMsec) + MinDelayTimeMsec + DelayOffsetMsec;

		// Set the new delay time on our delay line
		Delay.SetDelayMsec(NewDelayMsec);

		// Feed the input sample through the now modulated delay line 
		const float OutDelay = Delay(InSample);
		const float OutputSample = OutDelay * WetLevel + InSample * (1.0f - WetLevel);

		return OutputSample;
	}

}
