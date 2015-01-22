// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	HTML5Time.h: HTML5 platform Time functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformTime.h"
#include "HTML5/HTML5SystemIncludes.h"

#if PLATFORM_HTML5_WIN32
#include <time.h>
#include <sys/timeb.h>

// PLATFORM_HTML5_WIN32 

/**
* Windows implementation of the Time OS functions
**/

struct CORE_API FHTML5PlatformTime : public FGenericPlatformTime
{
	static double InitTiming()
	{
		return Seconds();
	}

	static FORCEINLINE double Seconds()
	{
		struct _timeb tb;
		_ftime(&tb);
		return tb.time + tb.millitm / 1000.0;
	}

	static FORCEINLINE uint32 Cycles()
	{
		struct _timeb tb;
		_ftime(&tb);
		return tb.time * 1000.0 + tb.millitm;
	}

	static void SystemTime( int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec );

	static void UtcTime( int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec );
};

typedef FHTML5PlatformTime FPlatformTime;
#else 

// PLATFORM_HTML5.

#include <time.h>
#include <emscripten.h> 



struct CORE_API FHTML5PlatformTime : public FGenericPlatformTime
{
	static double emscripten_t0; 

	static double InitTiming()
	{
		emscripten_t0 = emscripten_get_now(); 
		SecondsPerCycle = 1.0f / 1000000.0f;
		return FHTML5PlatformTime::Seconds();
	}

	// for HTML5 - this returns the time since startup. 
	static FORCEINLINE double Seconds()
	{
		double t = emscripten_get_now();
		return (t - emscripten_t0) / 1000.0;
	}

	static FORCEINLINE uint32 Cycles()
	{
		return (uint32)(Seconds() * 1000000);
	}

};

typedef FHTML5PlatformTime FPlatformTime;
#endif
