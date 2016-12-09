// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Audio
{
	/** 
	* White noise generator 
	* Flat spectrum
	*/
	class FWhiteNoise
	{
	public:
		/** Constructor with a default scale add parameter */
		FWhiteNoise(const float InScale = 1.0f, const float InAdd = 0.0f);

		void SetScaleAdd(const float InScale, const float InAdd);

		/** Generate next sample of white noise */
		float operator()();

	private:
		float Scale;
		float Add;
	};

	/** 
	* Pink noise generator
	* 1/Frequency noise spectrum
	*/
	class FPinkNoise
	{
	public:
		/** Constructor. */
		FPinkNoise(const float InScale = 1.0f, const float InAdd = 0.0f);

		/** Sets the output scale and add parameter. */
		void SetScaleAdd(const float InScale, const float InAdd);

		/** Generate next sample of pink noise. */
		float operator()();

	private:

		void InitFilter();

		FWhiteNoise Noise;

		float A[4];
		float B[4];
		float X[4];
		float Y[4];
	};


}
