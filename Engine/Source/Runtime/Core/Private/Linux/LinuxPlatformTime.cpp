// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ApplePlatformTime.mm: Apple implementations of time functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include <sys/resource.h>

namespace
{
	FORCEINLINE double TimeValToMicroSec(timeval & tv)
	{
		return static_cast<double>(tv.tv_sec) * 1e6 + static_cast<double>(tv.tv_usec);
	}
}

FCPUTime FLinuxTime::GetCPUTime()
{
	// minimum delay between checks to minimize overhead (and also match Windows version)
	const double MinDelayBetweenChecksMicroSec = 25 * 1e3;
	
	// last time we checked the timer
	static double PreviousUpdateTimeNanoSec = 0.0;
	// last user + system time
	static double PreviousSystemAndUserProcessTimeMicroSec = 0.0;

	// last CPU utilization
	static float CurrentCpuUtilization = 0.0f;
	// last CPU utilization (per core)
	static float CurrentCpuUtilizationNormalized = 0.0f;

	struct timespec ts;
	if (0 == clock_gettime(CLOCK_MONOTONIC, &ts))
	{
		const double CurrentTimeNanoSec = static_cast<double>(ts.tv_sec) * 1e9 + static_cast<double>(ts.tv_nsec);

		// see if we need to update the values
		double TimeSinceLastUpdateMicroSec = ( CurrentTimeNanoSec - PreviousUpdateTimeNanoSec ) / 1e3;
		if (TimeSinceLastUpdateMicroSec >= MinDelayBetweenChecksMicroSec)
		{
			struct rusage Usage;
			if (0 == getrusage(RUSAGE_SELF, &Usage))
			{				
				const double CurrentSystemAndUserProcessTimeMicroSec = TimeValToMicroSec(Usage.ru_utime) + TimeValToMicroSec(Usage.ru_stime); // holds all usages on all cores
				const double CpuTimeDuringPeriodMicroSec = CurrentSystemAndUserProcessTimeMicroSec - PreviousSystemAndUserProcessTimeMicroSec;

				double CurrentCpuUtilizationHighPrec = CpuTimeDuringPeriodMicroSec / TimeSinceLastUpdateMicroSec * 100.0;

				// recalculate the values
				CurrentCpuUtilizationNormalized = static_cast< float >( CurrentCpuUtilizationHighPrec / static_cast< double >( FPlatformMisc::NumberOfCoresIncludingHyperthreads() ) );
				CurrentCpuUtilization = static_cast< float >( CurrentCpuUtilizationHighPrec );

				// update previous
				PreviousSystemAndUserProcessTimeMicroSec = CurrentSystemAndUserProcessTimeMicroSec;
				PreviousUpdateTimeNanoSec = CurrentTimeNanoSec;
			}
		}
	}

	return FCPUTime(CurrentCpuUtilizationNormalized, CurrentCpuUtilization);
}