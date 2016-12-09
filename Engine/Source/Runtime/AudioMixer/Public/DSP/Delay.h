// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Dsp.h"
#include "DSP/WaveTableOsc.h"

namespace Audio
{

	/** Represents a circulating delay line. */
	class FDelay
	{
	public:

		/** Constructor. */
		FDelay();

		/** Destructor. */
		virtual ~FDelay();

		/** Initializes the delay line with the given number of input samples. */
		void Init(const int32 InSampleRate, const int32 InDelaySizeMsec = 2000.0f);

		/** Resets the delay line to re-use for audio processing. */
		void Reset();

		/** Sets the feedback of the delay line. */
		void SetFeedback(const float InFeedback);

		/** Sets the output gain of the delay line in dB */
		void SetOutputGainDB(const float InOutputGainDB);

		/** Sets the output gain of the delay line in linear gain. */
		void SetOutputGain(const float InOutputGain);

		/** Sets the delay length (of read index) by given fractional mSec. */
		void SetDelayMsec(const float InDelayMsec);

		/** Reads from the delay line at the current read index without incrementing the read index. */
		float Read() const;

		/** Reads the value of the delay line at the given tap out point in milliseconds. */
		float TapOut(const float TapOutMsec) const;

		/** Writes the audio sample into the delay line and increments read and write indices. */
		void TapIn(const float InSample);

		/** Writes the input sample and returns the sample at the output of the delay line. */
		virtual float operator()(const float InSample);

	protected:
		/** The delay line audio buffer. */
		TArray<float> AudioBuffer;

		/** Feedback of the delay line. */
		float Feedback;

		/** Represents this delay lines current fractional sample delay. */
		float DelaySamples;

		/** The output gain of the delay. */
		float OutputGain;

		/** Where the delay line is going to read from next. */
		int32 ReadIndex;

		/** Where teh delay line is going write to next. */
		int32 WriteIndex;

		/** The sample rate of the delay line. */
		int32 SampleRate;
	};

	/** A delay line which can be modulated by an LFO. */
	class FModulatedDelay
	{
	public:
		/** Constructor */
		FModulatedDelay();

		/** Destructor */
		virtual ~FModulatedDelay();

		/** Initialize the modulated delay with the given sample rate. */
		virtual void Init(int32 InSampleRate);

		/** Set the modulated delay to use the saw LFO */
		void SetLFO(const EWaveTable::Type InLFOType);

		/** Sets whether or not this modulated delay's LFOs are in quadrature phase. */
		void SetQuadraturePhase(const bool bInIsInQuadPhase) { bIsInQuadPhase = bInIsInQuadPhase; }

		/** Set the modulation feedback (between -1.0f and 1.0f) */
		virtual void SetModulationFeedback(const float InModulationFeedback);

		/** Sets the modulated delay modulation min and max delay times. */
		void SetMinAndMaxDelayMSec(const float InMinDelayTimeMsec, const float InMaxDelayTimeMsec);

		/** Sets the delay offset time. */
		void SetModulationDelayOffsetMsec(const float InDelayOffsetMsec);

		/** Sets the modulation frequency of the LFOs. */
		void SetModulationFrequency(const float InModulationFrequency);

		/** Sets the amount of wet level in the modulated delay. */
		void SetWetLevel(const float InWetLevel);

		/** Generate new sample from the given input sample. */
		float operator()(const float InSample);

	protected:

		/** Wave tables used for LFOs. */
		FSineWaveTable SinLFO;
		FSawWaveTable SawLFO;
		FTriangleWaveTable TriLFO;
		FSquareWaveTable SquareLFO;

		/** Ptr to the currently used wave tables */
		FWaveTableOsc* CurrentWaveTable;

		/** A delay line */
		FDelay Delay;

		/** The modulation depth */
		float ModulationDepth;

		/** The modulation frequency. */
		float ModulationFrequency;

		/** The min delay time for the flanger. */
		float MinDelayTimeMsec;

		/** The max delay time for the flanger. */
		float MaxDelayTimeMsec;

		/** An additional delay offset to use. */
		float DelayOffsetMsec;

		/** The modulation feedback. */
		float ModulationFeedback;

		/** The amount of wet level in the modulated delay line. */
		float WetLevel;

		/** Whether or not the flanger is operating in quad phase. */
		bool bIsInQuadPhase;
	};

}
