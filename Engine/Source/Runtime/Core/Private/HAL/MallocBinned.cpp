// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MallocBinned.cpp: Binned memory allocator
=============================================================================*/

#include "CorePrivatePCH.h"

#include "MallocBinned.h"
#include "MemoryMisc.h"

/** Malloc binned allocator specific stats. */
DEFINE_STAT(STAT_Binned_OsCurrent);
DEFINE_STAT(STAT_Binned_OsPeak);
DEFINE_STAT(STAT_Binned_WasteCurrent);
DEFINE_STAT(STAT_Binned_WastePeak);
DEFINE_STAT(STAT_Binned_UsedCurrent);
DEFINE_STAT(STAT_Binned_UsedPeak);
DEFINE_STAT(STAT_Binned_CurrentAllocs);
DEFINE_STAT(STAT_Binned_TotalAllocs);
DEFINE_STAT(STAT_Binned_SlackCurrent);

void FMallocBinned::GetAllocatorStats( FGenericMemoryStats& out_Stats )
{
	FMalloc::GetAllocatorStats( out_Stats );

#if	STATS
	SIZE_T	LocalOsCurrent = 0;
	SIZE_T	LocalOsPeak = 0;
	SIZE_T	LocalWasteCurrent = 0;
	SIZE_T	LocalWastePeak = 0;
	SIZE_T	LocalUsedCurrent = 0;
	SIZE_T	LocalUsedPeak = 0;
	SIZE_T	LocalCurrentAllocs = 0;
	SIZE_T	LocalTotalAllocs = 0;
	SIZE_T	LocalSlackCurrent = 0;

	{
#ifdef USE_INTERNAL_LOCKS
		FScopeLock ScopedLock( &AccessGuard );
#endif

		UpdateSlackStat();

		// Copy memory stats.
		LocalOsCurrent = OsCurrent;
		LocalOsPeak = OsPeak;
		LocalWasteCurrent = WasteCurrent;
		LocalWastePeak = WastePeak;
		LocalUsedCurrent = UsedCurrent;
		LocalUsedPeak = UsedPeak;
		LocalCurrentAllocs = CurrentAllocs;
		LocalTotalAllocs = TotalAllocs;
		LocalSlackCurrent = SlackCurrent;
	}

	// Malloc binned stats.
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_OsCurrent ), LocalOsCurrent );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_OsPeak ), LocalOsPeak );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_WasteCurrent ), LocalWasteCurrent );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_WastePeak ), LocalWastePeak );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_UsedCurrent ), LocalUsedCurrent );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_UsedPeak ), LocalUsedPeak );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_CurrentAllocs ), LocalCurrentAllocs );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_TotalAllocs ), LocalTotalAllocs );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_Binned_SlackCurrent ), LocalSlackCurrent );
#endif // STATS
}

void FMallocBinned::InitializeStatsMetadata()
{
	FMalloc::InitializeStatsMetadata();

	// Initialize stats metadata here instead of UpdateStats.
	// Mostly to avoid dead-lock when stats malloc profiler is enabled.
	GET_STATFNAME(STAT_Binned_OsCurrent);
	GET_STATFNAME(STAT_Binned_OsPeak);
	GET_STATFNAME(STAT_Binned_WasteCurrent);
	GET_STATFNAME(STAT_Binned_WastePeak);
	GET_STATFNAME(STAT_Binned_UsedCurrent);
	GET_STATFNAME(STAT_Binned_UsedPeak);
	GET_STATFNAME(STAT_Binned_CurrentAllocs);
	GET_STATFNAME(STAT_Binned_TotalAllocs);
	GET_STATFNAME(STAT_Binned_SlackCurrent);
}
