// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DSP/ParamInterpolator.h"

namespace Audio
{
	/**
	* FParamInterpolator implementation
	*/

	FParamInterpolator::FParamInterpolator()
		: StartValue(0.0f)
		, EndValue(0.0f)
		, CurrentTick(1.0f)
		, NumTicksToNewValue(1.0f)
	{
	}

	FParamInterpolator::~FParamInterpolator()
	{
	}

	void FParamInterpolator::InitValue(const float InValue)
	{
		StartValue = InValue;
		EndValue = InValue;
		CurrentTick = 1.0f;
		NumTicksToNewValue = 1.0f;
	}

	void FParamInterpolator::SetValue(const float InValue, const uint32 InNumInterpolationTicks)
	{
		EndValue = InValue;
		if (InNumInterpolationTicks == 0)
		{
			NumTicksToNewValue = 1.0f;
			CurrentTick = 1.0f;
		}
		else
		{
			NumTicksToNewValue = (float)InNumInterpolationTicks;
			CurrentTick = 0.0f;
		}
		check(NumTicksToNewValue > 0.0f);
	}

	float FParamInterpolator::GetValue() const
	{
		float Alpha = FMath::Min(1.0f, CurrentTick / NumTicksToNewValue);
		float Result = FMath::Lerp(StartValue, EndValue, Alpha);
		return Result;
	}

	float FParamInterpolator::operator()()
	{
		if (CurrentTick >= NumTicksToNewValue)
		{
			return EndValue;
		}

		float Result = GetValue();
		CurrentTick += 1.0f;
		return Result;
	}
}
