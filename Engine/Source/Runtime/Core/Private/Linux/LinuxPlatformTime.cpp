// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

	FORCEINLINE uint64 TimeSpecToMicroSec(timespec &ts)
	{
		return static_cast<uint64>(ts.tv_sec) * 1000000000ULL + static_cast<uint64>(ts.tv_nsec);
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
	if (0 == clock_gettime(CLOCK_MONOTONIC_RAW, &ts))
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

namespace
{
	/**
	 * Returns number of time we can call the clock per second.
	 */
	uint64 CallsPerSecondBenchmark(clockid_t BenchClockId, const TCHAR * BenchClockIdName)
	{
		const uint64 kBenchmarkPeriodMicroSec = 1000000000ULL / 10;	// 0.1s

		struct timespec ts;
		clock_gettime(BenchClockId, &ts);
		uint64 StartTimestamp = TimeSpecToMicroSec(ts);
		uint64 EndTimeStamp = StartTimestamp;

		uint64 NumCalls = 1;	// account for starting timestamp
		uint64 NumZeroDeltas = 0;
		const uint64 kHardLimitOnZeroDeltas = (1 << 26);	// arbitrary, but high enough so we don't hit it on fast coarse clocks
		do
		{
			clock_gettime(BenchClockId, &ts);

			uint64 NewEndTimeStamp = TimeSpecToMicroSec(ts);
			++NumCalls;

			if (NewEndTimeStamp < EndTimeStamp)
			{
				UE_LOG(LogLinux, Warning, TEXT("Clock_id %d (%s) is unusable, can go backwards."), BenchClockId, BenchClockIdName);
				return 0;
			}
			else if (NewEndTimeStamp == EndTimeStamp)
			{
				++NumZeroDeltas;

				// do not lock up if the clock is broken (e.g. stays in place)
				if (NumZeroDeltas > kHardLimitOnZeroDeltas)
				{
					UE_LOG(LogLinux, Warning, TEXT("Clock_id %d (%s) is unusable, too many (%llu) zero deltas"), BenchClockId, BenchClockIdName, NumZeroDeltas);
					return 0;
				}
			}

			EndTimeStamp = NewEndTimeStamp;
		} 
		while (EndTimeStamp - StartTimestamp < kBenchmarkPeriodMicroSec);	

		double TimesPerSecond = 1e9 / static_cast<double>(EndTimeStamp - StartTimestamp);

		uint64 RealNumCalls = static_cast<uint64> (TimesPerSecond * static_cast<double>(NumCalls));

		UE_LOG(LogLinux, Log, TEXT(" - %s (id=%d) can sustain %llu (%lluK, %lluM) calls per second %s"), BenchClockIdName, BenchClockId, 
			RealNumCalls, (RealNumCalls + 500) / 1000, (RealNumCalls + 500000) / 1000000, 
			NumZeroDeltas ? *FString::Printf(TEXT("with %f%% zero deltas"), 100.0 * static_cast<double>(NumZeroDeltas) / static_cast<double>(NumCalls)) : TEXT("without zero deltas"));

		return RealNumCalls;
	}

}

void FLinuxTime::CalibrateClock()
{
	UE_LOG(LogLinux, Log, TEXT("Benchmarking clocks"));

	static uint64 ClockRates[] =
	{
		CallsPerSecondBenchmark(CLOCK_REALTIME, TEXT("CLOCK_REALTIME")),
		CallsPerSecondBenchmark(CLOCK_MONOTONIC, TEXT("CLOCK_MONOTONIC")),
		CallsPerSecondBenchmark(CLOCK_MONOTONIC_RAW, TEXT("CLOCK_MONOTONIC_RAW")),
		CallsPerSecondBenchmark(CLOCK_MONOTONIC_COARSE, TEXT("CLOCK_MONOTONIC_COARSE"))
	};

	// Warn if our current clock source cannot be called at least 2M times a second (<50k a frame) as this may affect tight loops
	if (ClockRates[2] < 2000000)
	{
		UE_LOG(LogLinux, Warning, TEXT("Current clock source (CLOCK_MONOTONIC_RAW) is too slow on this machine, performance may be affected."));
	}

	// @TODO: switch to whatever is faster and does not have zero deltas?
}
