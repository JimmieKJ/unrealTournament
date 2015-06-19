// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "LockFreeList.h"


DEFINE_LOG_CATEGORY(LogLockFreeList);

FThreadSafeCounter FLockFreeListStats::NumOperations;
FThreadSafeCounter FLockFreeListStats::Cycles;


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

#endif //USE_LOCKFREELIST_128

