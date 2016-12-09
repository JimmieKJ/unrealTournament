// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Delay.h"

namespace Audio
{
	/** 
	* FFlanger
	* A simple modulated delay line.
	*/
	class FFlanger : public FModulatedDelay
	{
	public:
		/** Constructor */
		FFlanger();

		/** Destructor */
		virtual ~FFlanger();

		/** Initialize the flanger with the given sample rate. */
		void Init(int32 InSampleRate) override;
	};

	class FStereoFlanger
	{
	public:
		FStereoFlanger();
		~FStereoFlanger();

		/** Initialize the stereo flanger with the given sample rate. */
		void Init(int32 InSampleRate);

		/** Set the flanger to use the saw LFO */
		void SetLFO(const EWaveTable::Type InLFOType);

		/** Set the modulation feedback (between -1.0f and 1.0f) */
		void SetModulationFeedback(const float InModulationFeedback);

		/** Sets the flanger modulation min and max delay times. */
		void SetModulationMinAndMaxDelayMSec(const float InMinDelayTimeMsec, const float InMaxDelayTimeMsec);

		/** Sets the modulation frequency of the LFOs. */
		void SetModulationFrequency(const float InModulationFrequency);

		void operator()(const float InLeft, const float InRight, float* OutLeft, float* OutRight);

	private:
		/** Stereo flanger, one for each channel. */
		FFlanger Flanger[2];
	};
}
