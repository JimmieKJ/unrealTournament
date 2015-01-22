// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	LinuxPlatformMath.h: Linux platform Math functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformMath.h"
#include "Linux/LinuxSystemIncludes.h"

/**
* Linux implementation of the Math OS functions
**/
struct FLinuxPlatformMath : public FGenericPlatformMath
{
#if PLATFORM_ENABLE_VECTORINTRINSICS
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
		// Note: the x2 is to workaround the rounding-to-nearest-even-number issue when the fraction is .5
		return (float)CeilToInt(F);
	}

	static FORCEINLINE bool IsNaN( float A ) { return isnan(A) != 0; }
	static FORCEINLINE bool IsFinite( float A ) { return isfinite(A); }

	//
	// MSM: Fast float inverse square root using SSE.
	// Accurate to within 1 LSB.
	//
	static FORCEINLINE float InvSqrt( float F )
	{
		static const __m128 fThree = _mm_set_ss( 3.0f );
		static const __m128 fOneHalf = _mm_set_ss( 0.5f );
		__m128 Y0, X0, Temp;
		float temp;

		Y0 = _mm_set_ss( F );
		X0 = _mm_rsqrt_ss( Y0 );	// 1/sqrt estimate (12 bits)

		// Newton-Raphson iteration (X1 = 0.5*X0*(3-(Y*X0)*X0))
		Temp = _mm_mul_ss( _mm_mul_ss(Y0, X0), X0 );	// (Y*X0)*X0
		Temp = _mm_sub_ss( fThree, Temp );				// (3-(Y*X0)*X0)
		Temp = _mm_mul_ss( X0, Temp );					// X0*(3-(Y*X0)*X0)
		Temp = _mm_mul_ss( fOneHalf, Temp );			// 0.5*X0*(3-(Y*X0)*X0)
		_mm_store_ss( &temp, Temp );

		return temp;
	}
#endif

};

typedef FLinuxPlatformMath FPlatformMath;

