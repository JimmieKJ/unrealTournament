// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#if STATS
/**
 * Utility class to capture time passed in seconds, adding delta time to passed
 * in variable.
 */
class FScopeSecondsCounter
{
public:
	/** Ctor, capturing start time. */
	FScopeSecondsCounter( double& InSeconds )
	:	StartTime(FPlatformTime::Seconds())
	,	Seconds(InSeconds)
	{}
	/** Dtor, updating seconds with time delta. */
	~FScopeSecondsCounter()
	{
		Seconds += FPlatformTime::Seconds() - StartTime;
	}
private:
	/** Start time, captured in ctor. */
	double StartTime;
	/** Time variable to update. */
	double& Seconds;
};

/** Macro for updating a seconds counter without creating new scope. */
#define SCOPE_SECONDS_COUNTER(Seconds) \
	FScopeSecondsCounter SecondsCount_##Seconds(Seconds);

#else
#define SCOPE_SECONDS_COUNTER(Seconds)
#endif 

/** Key contains total time, value contains total count. */
typedef TKeyValuePair<double, uint32> FTotalTimeAndCount;

/**
 *	Utility class to log time passed in seconds, adding cumulative stats to passed in variable. 
 */
struct FScopeLogTime
{
	enum EScopeLogTimeUnits
	{
		ScopeLog_Milliseconds,
		ScopeLog_Seconds
	};

	/**
	 * Initialization constructor.
	 *
	 * @param InName - String that will be displayed in the log
	 * @param InGlobal - Pointer to the variable that holds the cumulative stats
	 *
	 */
	CORE_API FScopeLogTime( const WIDECHAR* InName, FTotalTimeAndCount* InCumulative = nullptr, EScopeLogTimeUnits InUnits = ScopeLog_Milliseconds );
	CORE_API FScopeLogTime( const ANSICHAR* InName, FTotalTimeAndCount* InCumulative = nullptr, EScopeLogTimeUnits InUnits = ScopeLog_Milliseconds );

	/** Destructor. */
	CORE_API ~FScopeLogTime();

protected:
	double GetDisplayScopedTime(double InScopedTime) const;
	FString GetDisplayUnitsString() const;


	const double StartTime;
	const FString Name;
	FTotalTimeAndCount* Cumulative;
	EScopeLogTimeUnits Units;
};

#define SCOPE_LOG_TIME(Name,CumulativePtr) \
	FScopeLogTime PREPROCESSOR_JOIN(ScopeLogTime,__LINE__)(Name,CumulativePtr);

#define SCOPE_LOG_TIME_IN_SECONDS(Name,CumulativePtr) \
	FScopeLogTime PREPROCESSOR_JOIN(ScopeLogTime,__LINE__)(Name, CumulativePtr, FScopeLogTime::ScopeLog_Seconds);

#define SCOPE_LOG_TIME_FUNC() \
	FScopeLogTime PREPROCESSOR_JOIN(ScopeLogTime,__LINE__)(__FUNCTION__);

#define SCOPE_LOG_TIME_FUNC_WITH_GLOBAL(CumulativePtr) \
	FScopeLogTime PREPROCESSOR_JOIN(ScopeLogTime,__LINE__)(__FUNCTION__,CumulativePtr);
