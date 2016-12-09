// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Utility macro to clamp a sample value to 0 if in range outside of full precision. */

namespace Audio
{
	FORCEINLINE float UnderflowClamp(const float InValue)
	{
		if (InValue > 0.0f && InValue < FLT_MIN)
		{
			return 0.0f;
		}
		if (InValue < 0.0f && InValue > -FLT_MIN)
		{
			return 0.0f;
		}
		return InValue;
	}

}

