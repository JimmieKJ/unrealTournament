// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WindowsPlatformTime.cpp: Windows implementations of time functions
=============================================================================*/

#include "CorePrivatePCH.h"

#if PLATFORM_HTML5_WIN32
#include <time.h>

void FHTML5PlatformTime::SystemTime( int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec )
{
	time_t ltime;
	time(&ltime);
	struct tm lt;

	_localtime64_s(&lt, &ltime);

	Year		= lt.tm_year + 1900;
	Month		= lt.tm_mon + 1;
	DayOfWeek	= lt.tm_wday;
	Day			= lt.tm_mday;
	Hour		= lt.tm_hour;
	Min			= lt.tm_min;
	Sec			= lt.tm_sec;
	MSec		= 0;
}

void FHTML5PlatformTime::UtcTime( int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec )
{
	time_t ltime;
	time(&ltime);
	struct tm gmt;

	_gmtime64_s(&gmt, &ltime);

	Year		= gmt.tm_year + 1900;
	Month		= gmt.tm_mon + 1;
	DayOfWeek	= gmt.tm_wday;
	Day			= gmt.tm_mday;
	Hour		= gmt.tm_hour;
	Min			= gmt.tm_min;
	Sec			= gmt.tm_sec;
	MSec		= 0;
}
#else 
double FHTML5PlatformTime::emscripten_t0 = 0; 
#endif 
