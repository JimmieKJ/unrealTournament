// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Build.h"

#ifndef DO_TIMEGUARD
	// By default we are enabled based on STATS, but DO_TIMEGUARD can be set in XXX.Target.cs if so desired%
	#define DO_TIMEGUARD STATS
#endif

#if DO_TIMEGUARD

class FLightweightTimeGuard
{

public:

	struct FGuardInfo
	{
		int			Count;
		float		Min;
		float		Max;
		float		Total;
		FDateTime	FirstTime;
		FDateTime	LastTime;

		FGuardInfo() :
			Count(0)
			, Min(FLT_MAX)
			, Max(-FLT_MAX)
			, Total(0)
		{}
	};

public:
	FORCEINLINE FLightweightTimeGuard(TCHAR const* InName, const float InTargetMS = 0.0)
		: Name(nullptr)
	{
		Name = (bEnabled && IsInGameThread()) ? InName : nullptr;

		if (Name)
		{
			TargetTimeMS = (InTargetMS > 0) ? InTargetMS : FrameTimeThresholdMS;
			StartTime = FPlatformTime::Seconds();
		}
	}

	/**
	* Updates the stat with the time spent
	*/
	FORCEINLINE ~FLightweightTimeGuard()
	{
		if (Name)
		{
			double delta = (FPlatformTime::Seconds() - StartTime) * 1000;

			if (delta > TargetTimeMS)
			{
				ReportHitch(Name, delta);
			}
		}
	}

	// Can be used externally to extract / clear data
	static CORE_API void						ClearData();
	static CORE_API void						SetEnabled(bool InEnable);
	static CORE_API void						SetFrameTimeThresholdMS(float InTimeMS);
	static CORE_API void						GetData(TMap<const TCHAR*, FGuardInfo>& Dest);

protected:

	// Used for reporting
	static CORE_API void						ReportHitch(const TCHAR* InName, float TimeMS);
	static TMap<const TCHAR*, FGuardInfo>		HitchData;
	static FCriticalSection						ReportMutex;
	static CORE_API double						LastHitchTime;
	static CORE_API bool						bEnabled;
	static CORE_API float						FrameTimeThresholdMS;

protected:

	// Per-stat tracking
	TCHAR const*	Name;
	float			TargetTimeMS;
	double			StartTime;

};

#define SCOPE_TIME_GUARD(name) \
	FLightweightTimeGuard TimeGuard_##Stat(name);

#define SCOPE_TIME_GUARD_MS(name, timeMs) \
	FLightweightTimeGuard TimeGuard_##Stat(name, timeMs);


#else

#define SCOPE_TIME_GUARD(name)
#define SCOPE_TIME_GUARD_MS(name, timeMs)

#endif // DO_TIMEGUARD
