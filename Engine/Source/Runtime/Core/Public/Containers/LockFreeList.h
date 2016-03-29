// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogLockFreeList, Log, All);

// what level of checking to perform...normally checkLockFreePointerList but could be ensure or check
#define checkLockFreePointerList checkSlow
//#define checkLockFreePointerList(x) ((x)||((*(char*)3) = 0))

#define MONITOR_LINK_ALLOCATION (0)

#if	MONITOR_LINK_ALLOCATION==1
	typedef FThreadSafeCounter	FLockFreeListCounter;
#else
	typedef FNoopCounter		FLockFreeListCounter;
#endif // MONITOR_LINK_ALLOCATION==1

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

