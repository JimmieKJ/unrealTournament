// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
* Declarations for LoadTimer which helps get load times for various parts of the game.
*/

#pragma once

#define ENABLE_LOADTIME_TRACKING 1

#include "ScopedTimers.h"

#if ENABLE_LOADTIME_TRACKING

/** High level load time tracker utility (such as initial engine startup or game specific timings) */
class CORE_API FLoadTimeTracker
{
public:
	static FLoadTimeTracker& Get()
	{
		static FLoadTimeTracker Singleton;
		return Singleton;
	}

	/** Adds a scoped time for a given label.  Records each instance individually */
	void ReportScopeTime(double ScopeTime, const FName ScopeLabel);

	/** Prints out total time and individual times */
	void DumpLoadTimes();


	static void DumpLoadTimesStatic()
	{
		Get().DumpLoadTimes();
	}

	const TMap<FName, TArray<double>>& GetData() const
	{
		return TimeInfo;
	}

	void ResetLoadTimes();

private:
	TMap<FName, TArray<double>> TimeInfo;

private:
	FLoadTimeTracker()
	{
	}
};

/** Scoped duration timer that automatically reports to FLoadTimeTracker */
class CORE_API FScopedLoadTimer : public FDurationTimer
{
public:
	explicit FScopedLoadTimer(const FName InLabel) 
		: FDurationTimer(TimerData)
		, TimerData(0)
		, Label(InLabel)
	{
		Start();
	}

	/** Dtor, updating seconds with time delta. */
	~FScopedLoadTimer()
	{
		Stop();
		FLoadTimeTracker::Get().ReportScopeTime(GetAccumulatedTime(), Label);
	}
private:
	double TimerData;
	const FName Label;
};


#define SCOPED_LOADTIMER(TimerName) FScopedLoadTimer TimerName##_ScopedTimer_ (FName(TEXT(#TimerName)))
#define ACCUM_LOADTIME(TimerName, Time) FLoadTimeTracker::Get().ReportScopeTime(Time, FName(TimerName));
#define SET_LOADTIME(TimerName, Time) FLoadTimeTracker::Get().SetTime(Time, FName(TimerName));
#else
#define SCOPED_LOADTIMER(TimerName)
#define ACCUM_LOADTIME(TimerName, Time)
#define SET_LOADTIME(TimerName, Time)
#endif