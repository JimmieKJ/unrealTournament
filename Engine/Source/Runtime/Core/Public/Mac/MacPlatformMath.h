// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	MacPlatformMath.h: Mac platform Math functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformMath.h"
#include "Mac/MacSystemIncludes.h"
#include "Math/UnrealPlatformMathSSE.h"

/**
* Mac implementation of the Math OS functions
**/
struct FMacPlatformMath : public FGenericPlatformMath
{
	static FORCEINLINE int32 TruncToInt(float F)
	{
		return _mm_cvtt_ss2si(_mm_set_ss(F));
	}

	static FORCEINLINE float TruncToFloat(float F)
	{
		return (float)TruncToInt(F); // same as generic implementation, but this will call the faster trunc
	}

	static FORCEINLINE int32 RoundToInt(float F)
	{
		// Note: the x2 is to workaround the rounding-to-nearest-even-number issue when the fraction is .5
		return _mm_cvt_ss2si(_mm_set_ss(F + F + 0.5f)) >> 1;
	}

	static FORCEINLINE float RoundToFloat(float F)
	{
		return (float)RoundToInt(F);
	}

	static FORCEINLINE int32 FloorToInt(float F)
	{
		return _mm_cvt_ss2si(_mm_set_ss(F + F - 0.5f)) >> 1;
	}

	static FORCEINLINE float FloorToFloat(float F)
	{
		return (float)FloorToInt(F);
	}

	static FORCEINLINE int32 CeilToInt(float F)
	{
		// Note: the x2 is to workaround the rounding-to-nearest-even-number issue when the fraction is .5
		return -(_mm_cvt_ss2si(_mm_set_ss(-0.5f - (F + F))) >> 1);
	}

	static FORCEINLINE float CeilToFloat(float F)
	{
		return (float)CeilToInt(F);
	}

	static FORCEINLINE bool IsNaN( float A ) { return isnan(A) != 0; }
	static FORCEINLINE bool IsFinite( float A ) { return isfinite(A); }

#if PLATFORM_ENABLE_VECTORINTRINSICS
	static FORCEINLINE float InvSqrt(float F)
	{
		return UnrealPlatformMathSSE::InvSqrt(F);
	}

	static FORCEINLINE float InvSqrtEst( float F )
	{
		return UnrealPlatformMathSSE::InvSqrtEst(F);
	}
#endif

};

typedef FMacPlatformMath FPlatformMath;

