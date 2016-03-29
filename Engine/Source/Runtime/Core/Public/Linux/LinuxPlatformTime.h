// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	LinuxPlatformTime.h: Linux platform Time functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformTime.h"
#include "Linux/LinuxSystemIncludes.h"

/**
 * Linux implementation of the Time OS functions
 */
struct CORE_API FLinuxTime : public FGenericPlatformTime
{
	static FORCEINLINE double Seconds()
	{
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
		return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
	}

	static FORCEINLINE uint32 Cycles()
	{
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
		return static_cast<uint32>(static_cast<uint64>(ts.tv_sec) * 1000000ULL + static_cast<uint64>(ts.tv_nsec) / 1000ULL);
	}

	static FCPUTime GetCPUTime();	

	/**
	 * Benchmarks clock_gettime(), possibly switches to something else is too slow.
	 * Linux-specific.
	 */
	static void CalibrateClock();
};

typedef FLinuxTime FPlatformTime;
