// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
* Here are a number of profiling helper functions so we do not have to duplicate a lot of the glue
* code everywhere.  And we can have consistent naming for all our files.
*
*/

// Core includes.
#include "CorePrivatePCH.h"
#include "LoadTimeTracker.h"

#if ENABLE_LOADTIME_TRACKING

void FLoadTimeTracker::ReportScopeTime(double ScopeTime, const FName ScopeLabel)
{
	check(IsInGameThread());
	TArray<double>& LoadTimes = TimeInfo.FindOrAdd(ScopeLabel);
	LoadTimes.Add(ScopeTime);
}


void FLoadTimeTracker::DumpLoadTimes()
{
	double TotalTime = 0.0;
	UE_LOG(LogLoad, Log, TEXT("------------- Load times -------------"));
	for(auto Itr = TimeInfo.CreateIterator(); Itr; ++Itr)
	{
		const FString KeyName = Itr.Key().ToString();
		const TArray<double>& LoadTimes = Itr.Value();
		if(LoadTimes.Num() == 1)
		{
			TotalTime += Itr.Value()[0];
			UE_LOG(LogLoad, Log, TEXT("%s: %f"), *KeyName, Itr.Value()[0]);
		}
		else
		{
			double InnerTotal = 0.0;
			for(int Index = 0; Index < LoadTimes.Num(); ++Index)
			{
				InnerTotal += Itr.Value()[Index];
				UE_LOG(LogLoad, Log, TEXT("%s[%d]: %f"), *KeyName, Index, LoadTimes[Index]);
			}

			UE_LOG(LogLoad, Log, TEXT("    Sub-Total: %f"), InnerTotal);

			TotalTime += InnerTotal;
		}
		
	}
	UE_LOG(LogLoad, Log, TEXT("------------- ---------- -------------"));
	UE_LOG(LogLoad, Log, TEXT("Total Load times: %f"), TotalTime);
}

void FLoadTimeTracker::ResetLoadTimes()
{
	static bool bActuallyReset = !FParse::Param(FCommandLine::Get(), TEXT("-NoLoadTrackClear"));
	if(bActuallyReset)
	{
		TimeInfo.Reset();
	}
}

static FAutoConsoleCommand LoadTimerDumpCmd(
	TEXT("LoadTimes.DumpTracking"),
	TEXT("Dump high level load times being tracked"),
	FConsoleCommandDelegate::CreateStatic(&FLoadTimeTracker::DumpLoadTimesStatic)
	);

#endif

