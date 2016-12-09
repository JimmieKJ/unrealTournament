// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DSP/Chorus.h"

namespace Audio
{
	FChorus::FChorus()
	{
	}

	FChorus::~FChorus()
	{
	}

	void FChorus::Init(int32 InSampleRate)
	{
		// Call super class init
		FModulatedDelay::Init(InSampleRate);

		// Set delay time values and modulation params for flangers
		MinDelayTimeMsec = 5.0f;
		MaxDelayTimeMsec = 30.0f;
		SetModulationFeedback(0.5f);
		SetModulationDelayOffsetMsec(15.0f);
		SetWetLevel(0.5f);
	}
}
