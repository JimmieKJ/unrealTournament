// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

// Platform specific vector intrinsics include.
#if WITH_DIRECTXMATH
#define SIMD_ALIGNMENT (16)
#include "UnrealMathDirectX.h"
#elif PLATFORM_ENABLE_VECTORINTRINSICS
#define SIMD_ALIGNMENT (16)
#include "UnrealMathSSE.h"
#elif PLATFORM_ENABLE_VECTORINTRINSICS_NEON
#define SIMD_ALIGNMENT (16)
#include "UnrealMathNeon.h"
#else
#define SIMD_ALIGNMENT (4)
#include "UnrealMathFPU.h"
#endif

// 'Cross-platform' vector intrinsics (built on the platform-specific ones defined above)
#include "UnrealMathVectorCommon.h"

/** Vector that represents (1/255,1/255,1/255,1/255) */
extern CORE_API const VectorRegister VECTOR_INV_255;

/**
* Below this weight threshold, animations won't be blended in.
*/
#define ZERO_ANIMWEIGHT_THRESH (0.00001f)

namespace GlobalVectorConstants
{
	static const VectorRegister AnimWeightThreshold = MakeVectorRegister(ZERO_ANIMWEIGHT_THRESH, ZERO_ANIMWEIGHT_THRESH, ZERO_ANIMWEIGHT_THRESH, ZERO_ANIMWEIGHT_THRESH);
	static const VectorRegister RotationSignificantThreshold = MakeVectorRegister(1.0f - DELTA*DELTA, 1.0f - DELTA*DELTA, 1.0f - DELTA*DELTA, 1.0f - DELTA*DELTA);
}
