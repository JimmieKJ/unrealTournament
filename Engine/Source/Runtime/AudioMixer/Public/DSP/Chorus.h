// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Delay.h"

namespace Audio
{
	/** 
	* FChorus
	* Takes mono samples in and returns flanged mono samples out.
	*/
	class FChorus : public FModulatedDelay
	{
	public:
		/** Constructor */
		FChorus();

		/** Destructor */
		virtual ~FChorus();

		/** Initialize the flanger with the given sample rate. */
		void Init(int32 InSampleRate) override;
	};
}
