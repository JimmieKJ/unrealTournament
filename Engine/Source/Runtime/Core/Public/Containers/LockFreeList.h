// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogLockFreeList, Log, All);

// what level of checking to perform...normally checkLockFreePointerList but could be ensure or check
#define checkLockFreePointerList checkSlow /*check*/

#define MONITOR_LINK_ALLOCATION (0)

#if	MONITOR_LINK_ALLOCATION==1
	typedef FThreadSafeCounter	FLockFreeListCounter;
#else
	typedef FNoopCounter		FLockFreeListCounter;
#endif // MONITOR_LINK_ALLOCATION==1

struct CORE_API FLockFreeListStats
{
	/** Number of lock free list operations like Push, Pop, PopAll etc. */
	static FThreadSafeCounter NumOperations;
	/** Cycles spent in lock free list operations like Push, Pop, PopAll etc. */
	static FThreadSafeCounter Cycles;
};

#define MONITOR_LOCKFREELIST_PERFORMANCE (0)

/** Helper struct used to test the performance of the lock free list operations. */
struct FThreadSafePerfCounter
{
	const uint32 StartCycles;

	FThreadSafePerfCounter()
		: StartCycles( FPlatformTime::Cycles() )
	{}

	~FThreadSafePerfCounter()
	{
		const uint32 EndCycles = FPlatformTime::Cycles();
		const int32 Delta = int32( (uint32)EndCycles - (uint32)StartCycles );
		FLockFreeListStats::Cycles.Add( Delta );
		FLockFreeListStats::NumOperations.Increment();
	}
};


#if	MONITOR_LOCKFREELIST_PERFORMANCE==1
	typedef FThreadSafePerfCounter	FLockFreeListPerfCounter;
#else
	typedef FNoopStruct				FLockFreeListPerfCounter;
#endif // MONITOR_LOCKFREELIST_PERFORMANCE==1

/** Enumerates lock free list alignments. */
namespace ELockFreeAligment
{
	enum
	{
		LF64bit = 8,
		LF128bit = 16,
	};
}

/** Whether to use the lock free list based on 128-bit atomics. */
#define USE_LOCKFREELIST_128 (0/*PLATFORM_HAS_128BIT_ATOMICS*/)

#if USE_LOCKFREELIST_128==1
#include "LockFreeVoidPointerListBase128.h"
#else
#include "LockFreeVoidPointerListBase.h"
#endif

#include "LockFreeListImpl.h"

