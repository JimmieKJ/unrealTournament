// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Audio
{

	/**
	* FParamInterpolator
	* Simple parameter interpolator which interpolates to target values
	* in the given the required number of update ticks.
	*/
	class FParamInterpolator
	{
	public:

		FParamInterpolator();
		~FParamInterpolator();

		/** Initialize the value. */
		void InitValue(const float InValue);

		/** Set the target value to interpolate to in the given number of update ticks. */
		void SetValue(const float InValue, const uint32 InNumInterpolationTicks);

		/** Returns the current value without updating the tick. */
		float GetValue() const;

		/** Functor operator to generate new output value. */
		virtual float operator()();

	protected:
		float StartValue;
		float EndValue;
		float CurrentTick;
		float NumTicksToNewValue;
	};

}
