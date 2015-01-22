// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#if STATS

#include "StatsData.h"
#include "StatsMallocProfilerProxy.h"

#define DECLARE_MEMORY_PTR_STAT(CounterName,StatId,GroupId)\
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_Ptr, false, false, FPlatformMemory::MCR_Invalid); \
	static DEFINE_STAT(StatId)

#define DECLARE_MEMORY_SIZE_STAT(CounterName,StatId,GroupId)\
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_int64, false, false, FPlatformMemory::MCR_Invalid); \
	static DEFINE_STAT(StatId)

/** Fake stat group and memory stats. */
DECLARE_STATS_GROUP(TEXT("Memory Profiler"), STATGROUP_MemoryProfiler, STATCAT_Advanced);

DECLARE_MEMORY_PTR_STAT(TEXT("Memory Free Ptr"),			STAT_Memory_FreePtr,		STATGROUP_MemoryProfiler);
DECLARE_MEMORY_PTR_STAT(TEXT("Memory Alloc Ptr"),			STAT_Memory_AllocPtr,		STATGROUP_MemoryProfiler);
DECLARE_MEMORY_SIZE_STAT(TEXT("Memory Alloc Size"),			STAT_Memory_AllocSize,		STATGROUP_MemoryProfiler);

/** Stats for memory usage by the profiler. */
DECLARE_DWORD_COUNTER_STAT(TEXT("Profiler AllocPtr Calls"),	STAT_Memory_AllocPtr_Calls,	STATGROUP_StatSystem);
DECLARE_DWORD_COUNTER_STAT(TEXT("Profiler FreePtr Calls"),	STAT_Memory_FreePtr_Calls,	STATGROUP_StatSystem);
DECLARE_MEMORY_STAT( TEXT( "Profiler AllocPtr" ),			STAT_Memory_AllocPtr_Mem,	STATGROUP_StatSystem );
DECLARE_MEMORY_STAT( TEXT( "Profiler FreePtr" ),			STAT_Memory_FreePtr_Mem,	STATGROUP_StatSystem );

FStatsMallocProfilerProxy::FStatsMallocProfilerProxy( FMalloc* InMalloc ) 
	: UsedMalloc( InMalloc )
	, bEnabled(false)
{
}

FStatsMallocProfilerProxy* FStatsMallocProfilerProxy::Get()
{
	static FStatsMallocProfilerProxy* Instance = nullptr;
	if( Instance == nullptr )
	{
		Instance = new FStatsMallocProfilerProxy( GMalloc );
	}
	return Instance;
}


void FStatsMallocProfilerProxy::SetState( bool bEnable )
{
	bEnabled = bEnable;
	FPlatformMisc::MemoryBarrier();
	if( bEnable )
	{
		StatsMasterEnableAdd();
	}
	else
	{
		StatsMasterEnableSubtract();
	}
}


void FStatsMallocProfilerProxy::InitializeStatsMetadata()
{
	UsedMalloc->InitializeStatsMetadata();

	// Initialize the memory messages metadata.
	// Malloc profiler proxy needs to be disabled otherwise it will hit infinite recursion in DoSetup.
	// Needs to be changed if we want to support boot time memory profiling.
	const FName NameAllocPtr = GET_STATFNAME(STAT_Memory_AllocPtr);
	const FName NameFreePtr = GET_STATFNAME(STAT_Memory_FreePtr);
	const FName NameAllocSize = GET_STATFNAME(STAT_Memory_AllocSize);

	GET_STATFNAME(STAT_Memory_AllocPtr_Calls);
	GET_STATFNAME(STAT_Memory_FreePtr_Calls);

	GET_STATFNAME(STAT_Memory_AllocPtr_Mem);
	GET_STATFNAME(STAT_Memory_FreePtr_Mem);

	//StatPtr_STAT_Memory_AllocPtr;
	//StatPtr_STAT_Memory_FreePtr;
	//StatPtr_STAT_Memory_AllocSize;
}


void FStatsMallocProfilerProxy::TrackAlloc( void* Ptr, int64 Size )
{
	if( bEnabled )
	{
		if( Size > 0 )
		{
			FThreadStats* ThreadStats = FThreadStats::GetThreadStats();

			if( ThreadStats->MemoryMessageScope == 0 )
			{
				ThreadStats->AddMemoryMessage( GET_STATFNAME(STAT_Memory_AllocPtr), (uint64)(UPTRINT)Ptr | (uint64)EMemoryOperation::Alloc );	// 16 bytes
				ThreadStats->AddMemoryMessage( GET_STATFNAME(STAT_Memory_AllocSize), Size );					// 32 bytes total
				AllocPtrCalls.Increment();
			}
		}	
	}
}

void FStatsMallocProfilerProxy::TrackFree( void* Ptr )
{
	if( bEnabled )
	{
		if( Ptr != nullptr )
		{
			FThreadStats* ThreadStats = FThreadStats::GetThreadStats();

			if( ThreadStats->MemoryMessageScope == 0 )
			{
				ThreadStats->AddMemoryMessage( GET_STATFNAME(STAT_Memory_FreePtr), (uint64)(UPTRINT)Ptr | (uint64)EMemoryOperation::Free );	// 16 bytes total
				FreePtrCalls.Increment();
			}
		}	
	}
}

void* FStatsMallocProfilerProxy::Malloc( SIZE_T Size, uint32 Alignment )
{
	void* Ptr = UsedMalloc->Malloc( Size, Alignment );
	// We lose the Size's precision, but don't worry about it.
	TrackAlloc( Ptr, (int64)Size );
	return Ptr;
}

void* FStatsMallocProfilerProxy::Realloc( void* OldPtr, SIZE_T NewSize, uint32 Alignment )
{
	TrackFree( OldPtr );
	void* NewPtr = UsedMalloc->Realloc( OldPtr, NewSize, Alignment );
	TrackAlloc( NewPtr, (int64)NewSize );
	return NewPtr;
}

void FStatsMallocProfilerProxy::Free( void* Ptr )
{
	TrackFree( Ptr );
	UsedMalloc->Free( Ptr );
}

void FStatsMallocProfilerProxy::UpdateStats()
{
	UsedMalloc->UpdateStats();

	if( bEnabled )
	{
		const int32 NumAllocPtrCalls = AllocPtrCalls.GetValue();
		const int32 NumFreePtrCalls = FreePtrCalls.GetValue();

		SET_DWORD_STAT( STAT_Memory_AllocPtr_Calls, NumAllocPtrCalls );
		SET_DWORD_STAT( STAT_Memory_FreePtr_Calls, NumFreePtrCalls );

		// AllocPtr, AllocSize
		SET_MEMORY_STAT( STAT_Memory_AllocPtr_Mem, NumAllocPtrCalls*(sizeof(FStatMessage)*2) );
		// FreePtr
		SET_MEMORY_STAT( STAT_Memory_FreePtr_Mem, NumFreePtrCalls*(sizeof(FStatMessage)*1) );

		AllocPtrCalls.Reset();
		FreePtrCalls.Reset();
	}
}

#endif //STATS