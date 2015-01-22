// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"

#include "MainLoopTiming.h"
#include "LaunchEngineLoop.h"


FMainLoopTiming::FMainLoopTiming(float IdealTickRate, EMainLoopOptions::Type Options)
	: IdealFrameTime(1.f / IdealTickRate)
	, ActualDeltaTime(0)
	, LastTime(FPlatformTime::Seconds())
	, bTickSlate(Options & EMainLoopOptions::UsingSlate)
{
}

void FMainLoopTiming::Tick()
{
	// Tick app logic
	FTicker::GetCoreTicker().Tick(ActualDeltaTime);

#if !CRASH_REPORT_UNATTENDED_ONLY
	// Tick SlateApplication
	if (bTickSlate)
	{
		FSlateApplication::Get().PumpMessages();
		FSlateApplication::Get().Tick();
	}
#endif // !CRASH_REPORT_UNATTENDED_ONLY

	static const float TickRate = 30.f;
	static const float TickInterval = 1.f / TickRate;

	// Sleep Throttling
	// Copied from Community Portal - should be shared
	FPlatformProcess::Sleep(FMath::Max<float>(0, IdealFrameTime - (FPlatformTime::Seconds() - LastTime)));

	// Calculate deltas
	const double AppTime = FPlatformTime::Seconds();
	ActualDeltaTime = AppTime - LastTime;
	LastTime = AppTime;	
}
