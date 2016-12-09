// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Containers/LockFreeList.h"
#include "HAL/PlatformProcess.h"
#include "HAL/IConsoleManager.h"


DEFINE_LOG_CATEGORY(LogLockFreeList);


#if USE_LOCKFREELIST_128

/*-----------------------------------------------------------------------------
	FLockFreeVoidPointerListBase128
-----------------------------------------------------------------------------*/

FLockFreeVoidPointerListBase128::FLinkAllocator& FLockFreeVoidPointerListBase128::FLinkAllocator::Get()
{
	static FLinkAllocator TheAllocator;
	return TheAllocator;
}

FLockFreeVoidPointerListBase128::FLinkAllocator::FLinkAllocator()
	: SpecialClosedLink( FLargePointer( new FLink(), FLockFreeListConstants::SpecialClosedLink ) ) 
{
	if( !FPlatformAtomics::CanUseCompareExchange128() )
	{
		// This is a fatal error.
		FPlatformMisc::MessageBoxExt( EAppMsgType::Ok, TEXT( "CPU does not support Compare and Exchange 128-bit operation. Unreal Engine 4 will exit now." ), TEXT( "Unsupported processor" ) );
		FPlatformMisc::RequestExit( true );
		UE_LOG( LogHAL, Fatal, TEXT( "CPU does not support Compare and Exchange 128-bit operation" ) );
	}
}
	

FLockFreeVoidPointerListBase128::FLinkAllocator::~FLinkAllocator()
{
 	// - Deliberately leak to allow LockFree to be used during shutdown
#if	0
 	check(SpecialClosedLink.Pointer); // double free?
 	check(!NumUsedLinks.GetValue());
 	while( FLink* ToDelete = FLink::Unlink(FreeLinks) )
 	{
 		delete ToDelete;
 		NumFreeLinks.Decrement();
 	}
 	check(!NumFreeLinks.GetValue());
 	delete SpecialClosedLink.Pointer;
 	SpecialClosedLink.Pointer = 0;
#endif // 0
}

#else

/*-----------------------------------------------------------------------------
	FLockFreeVoidPointerListBase
-----------------------------------------------------------------------------*/

FLockFreeVoidPointerListBase::FLinkAllocator& FLockFreeVoidPointerListBase::FLinkAllocator::Get()
{
	static FLinkAllocator TheAllocator;
	return TheAllocator;
}

FLockFreeVoidPointerListBase::FLinkAllocator::FLinkAllocator()
	: FreeLinks(NULL)
	, SpecialClosedLink(new FLink())
{
	ClosedLink()->LockCount.Increment();
}

FLockFreeVoidPointerListBase::FLinkAllocator::~FLinkAllocator()
{
	// - Deliberately leak to allow LockFree to be used during shutdown
#if	0
	check(SpecialClosedLink); // double free?
	check(!NumUsedLinks.GetValue());
	while (FLink* ToDelete = FLink::Unlink(&FreeLinks))
	{
		delete ToDelete;
		NumFreeLinks.Decrement();
	}
	check(!NumFreeLinks.GetValue());
	delete SpecialClosedLink;
	SpecialClosedLink = 0;
#endif // 0
}

FLockFreePointerQueueBaseLinkAllocator& FLockFreePointerQueueBaseLinkAllocator::Get()
{
	static FLockFreePointerQueueBaseLinkAllocator Singleton;
	return Singleton;
}

//static FThreadSafeCounter Sleeps;
//static FThreadSafeCounter BigSleeps;

void LockFreeCriticalSpin(int32& SpinCount)
{
	SpinCount++;
	if (SpinCount > 256)
	{
		FPlatformProcess::Sleep(0.00001f);
		//BigSleeps.Increment();
	}
	else if (SpinCount > 32)
	{
		FPlatformProcess::Sleep(0.0f);
		//if (Sleeps.Increment() % 10000 == 9999)
		//{
		//	UE_LOG(LogTemp, Warning, TEXT("sleep0 %d sleep 10us %d"), Sleeps.GetValue() + 1, BigSleeps.GetValue());
		//}
	}
	else if (SpinCount > 8)
	{
		FPlatformMisc::MemoryBarrier();
	}
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

static FThreadSafeCounter TestStall;

void DoTestCriticalStall()
{
	if (TestStall.Increment() % 9713 == 9712)
	{
		FPlatformProcess::Sleep(0.000001f);
	}
}

int32 GTestCriticalStalls = 0;
static FAutoConsoleVariableRef CVarTestCriticalLockFree(
	TEXT("TaskGraph.TestCriticalLockFree"),
	GTestCriticalStalls,
	TEXT("If > 0, then we sleep periodically at critical points in the lock free lists. Threads must not starve...this will encourage them to starve at the right place to find livelocks."),
	ECVF_Cheat
	);

#endif

#endif //USE_LOCKFREELIST_128

