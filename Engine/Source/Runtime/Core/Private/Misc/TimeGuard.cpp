// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "TimeGuard.h"

DEFINE_LOG_CATEGORY_STATIC(LogTimeGuard, Log, All);

#if DO_TIMEGUARD

TMap<const TCHAR*, FLightweightTimeGuard::FGuardInfo>  FLightweightTimeGuard::HitchData;
bool FLightweightTimeGuard::bEnabled;
double FLightweightTimeGuard::LastHitchTime;
float FLightweightTimeGuard::FrameTimeThresholdMS = 1 / 30.0;
FCriticalSection FLightweightTimeGuard::ReportMutex;

void FLightweightTimeGuard::SetEnabled(bool InEnable)
{
	bEnabled = InEnable;
}

void	FLightweightTimeGuard::SetFrameTimeThresholdMS(float InTimeMS)
{
	FrameTimeThresholdMS = InTimeMS;
}

void FLightweightTimeGuard::ClearData()
{
	FScopeLock lock(&ReportMutex);
	HitchData.Empty();
	// don't capture any hitches immediately
	LastHitchTime = FPlatformTime::Seconds();
}

void  FLightweightTimeGuard::GetData(TMap<const TCHAR*, FGuardInfo>& Dest)
{
	FScopeLock lock(&ReportMutex);
	Dest = HitchData;
}

void FLightweightTimeGuard::ReportHitch(const TCHAR* InName, const float TimeMS)
{
	const double	kHitchDebounceTime = 2;

	// Don't report hitches that occur in the same 5sec window. This will also stop
	// the outer scope of nested checks reporting
	const double TimeNow = FPlatformTime::Seconds();
	if (TimeNow < LastHitchTime + kHitchDebounceTime)
	{
		return;
	}

	FScopeLock lock(&ReportMutex);

	FGuardInfo& Data = HitchData.FindOrAdd(InName);

	if (Data.Count == 0)
	{
		Data.FirstTime = FDateTime::UtcNow();
	}

	Data.Count++;
	Data.Total += TimeMS;
	Data.Min = FMath::Min(Data.Min, TimeMS);
	Data.Max = FMath::Max(Data.Max, TimeMS);
	Data.LastTime = FDateTime::UtcNow();

	LastHitchTime = TimeNow;

	UE_LOG(LogTimeGuard, Warning, TEXT("Detected Hitch of %0.2fms in %s"), TimeMS, InName);
}

#endif // DO_TIMEGUARD