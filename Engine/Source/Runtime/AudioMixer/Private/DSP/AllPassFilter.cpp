// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DSP/AllPassFilter.h"

namespace Audio
{
	FAllPassFilter::FAllPassFilter()
	{
	}

	FAllPassFilter::~FAllPassFilter()
	{
	}

	void FAllPassFilter::SetAPFGain(const float InGain)
	{
		APF_G = InGain;
	}

	float FAllPassFilter::operator()(const float InSample)
	{
		// If read and write index are identical then pass input to output
		if (ReadIndex == WriteIndex)
		{
			TapIn(InSample);
			return InSample;
		}

		// Using difference equations:
		// w(n) = x(n) + g * w(n - D)
		// y(n) = -g * w(n) + w(n - D)

		// w(n - D) is the sample value at the current read index
		const float Wn_D = Read();
		const float Wn = InSample + APF_G * Wn_D;
		const float Yn = -APF_G * Wn + Wn_D;

		// Write Wn (not Yn) into the delay line
		TapIn(Wn);

		return UnderflowClamp(Yn);
	}

}
