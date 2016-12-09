// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Delay.h"

namespace Audio
{
	/** 
	* An All Pass Filter (APF) delay line. 
	* https://en.wikipedia.org/wiki/All-pass_filter
	* APFs can be created by cascading a feedback and a feedforward combfilter with equal delays.
	*/
	class FAllPassFilter : public FDelay
	{
	public:
		FAllPassFilter();
		~FAllPassFilter();

		/** Set the APF Gain coefficient. */
		void SetAPFGain(const float InGain);

		/** Override the operator for APF */
		float operator()(const float InSample) override;

	protected:
		/** APF Feedback/Feedforward gain coefficient. */
		float APF_G;
	};


}
