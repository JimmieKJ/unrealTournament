// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#if USE_MALLOC_PROFILER && WITH_ENGINE && IS_MONOLITHIC
	#include "Runtime/Engine/Public/MallocProfilerEx.h"
#endif

/*-----------------------------------------------------------------------------
	Memory functions.
-----------------------------------------------------------------------------*/

#include "MallocDebug.h"
#include "MallocProfiler.h"
#include "MallocThreadSafeProxy.h"
#include "PlatformMallocCrash.h"


/** Helper function called on first allocation to create and initialize GMalloc */
void GCreateMalloc()
{
	GMalloc = FPlatformMemory::BaseAllocator();
	// Setup malloc crash as soon as possible.
	FPlatformMallocCrash::Get( GMalloc );

// so now check to see if we are using a Mem Profiler which wraps the GMalloc
#if USE_MALLOC_PROFILER
	#if WITH_ENGINE && IS_MONOLITHIC
		GMallocProfiler = new FMallocProfilerEx( GMalloc );
	#else
		GMallocProfiler = new FMallocProfiler( GMalloc );
	#endif
	GMallocProfiler->BeginProfiling();
	GMalloc = GMallocProfiler;
#endif

	// if the allocator is already thread safe, there is no need for the thread safe proxy
	if (!GMalloc->IsInternallyThreadSafe())
	{
		GMalloc = new FMallocThreadSafeProxy( GMalloc );
	}
}
#if STATS
	#define MALLOC_GT_HOOKS 1
#else
	#define MALLOC_GT_HOOKS 0
#endif

#if MALLOC_GT_HOOKS

// This code is used to find memory allocations, you set up the lambda in the section of the code you are interested in and add a breakpoint to your lambda to see who is allocating memory

//An example:
//static TFunction<void(int32)> AnimHook(
//	[](int32 Index)
//{
//	TickAnimationMallocStats[Index]++;
//	if (++AllCount % 337 == 0)
//	{
//		BreakMe();
//	}
//}
//);
//extern CORE_API TFunction<void(int32)>* GGameThreadMallocHook;
//TGuardValue<TFunction<void(int32)>*> RestoreHook(GGameThreadMallocHook, &AnimHook);


CORE_API TFunction<void(int32)>* GGameThreadMallocHook = nullptr;

FORCEINLINE static void DoGamethreadHook(int32 Index)
{
	if (GIsRunning && GGameThreadMallocHook // otherwise our hook might not be initialized yet
		&& IsInGameThread() ) 
	{
		(*GGameThreadMallocHook)(Index);
	}
}
#else
FORCEINLINE static void DoGamethreadHook(int32 Index)
{
}
#endif

void* FMemory::Malloc( SIZE_T Count, uint32 Alignment ) 
{ 
	if( !GMalloc )
	{
		GCreateMalloc();
		CA_ASSUME( GMalloc != NULL );	// Don't want to assert, but suppress static analysis warnings about potentially NULL GMalloc
	}
	DoGamethreadHook(0);
	return GMalloc->Malloc( Count, Alignment );
}

void* FMemory::Realloc( void* Original, SIZE_T Count, uint32 Alignment ) 
{ 
	if( !GMalloc )
	{
		GCreateMalloc();	
		CA_ASSUME( GMalloc != NULL );	// Don't want to assert, but suppress static analysis warnings about potentially NULL GMalloc
	}
	DoGamethreadHook(1);
	return GMalloc->Realloc( Original, Count, Alignment );
}	

void FMemory::Free( void* Original )
{
	if( !GMalloc )
	{
		GCreateMalloc();
		CA_ASSUME( GMalloc != NULL );	// Don't want to assert, but suppress static analysis warnings about potentially NULL GMalloc
	}
	DoGamethreadHook(2);
	return GMalloc->Free( Original );
}

SIZE_T FMemory::GetAllocSize( void* Original )
{
	if( !GMalloc )
	{
		GCreateMalloc();	
		CA_ASSUME( GMalloc != NULL );	// Don't want to assert, but suppress static analysis warnings about potentially NULL GMalloc
	}
	
	SIZE_T Size = 0;
	return GMalloc->GetAllocationSize( Original, Size ) ? Size : 0;
}

void FMemory::TestMemory()
{
#if !UE_BUILD_SHIPPING
	if( !GMalloc )
	{
		GCreateMalloc();	
		CA_ASSUME( GMalloc != NULL );	// Don't want to assert, but suppress static analysis warnings about potentially NULL GMalloc
	}

	// track the pointers to free next call to the function
	static TArray<void*> LeakedPointers;
	TArray<void*> SavedLeakedPointers = LeakedPointers;


	// note that at the worst case, there will be NumFreedAllocations + 2 * NumLeakedAllocations allocations alive
	static const int NumFreedAllocations = 1000;
	static const int NumLeakedAllocations = 100;
	static const int MaxAllocationSize = 128 * 1024;

	TArray<void*> FreedPointers;
	// allocate pointers that will be freed later
	for (int32 Index = 0; Index < NumFreedAllocations; Index++)
	{
		FreedPointers.Add(FMemory::Malloc(FMath::RandHelper(MaxAllocationSize)));
	}


	// allocate pointers that will be leaked until the next call
	LeakedPointers.Empty();
	for (int32 Index = 0; Index < NumLeakedAllocations; Index++)
	{
		LeakedPointers.Add(FMemory::Malloc(FMath::RandHelper(MaxAllocationSize)));
	}

	// free the leaked pointers from _last_ call to this function
	for (int32 Index = 0; Index < SavedLeakedPointers.Num(); Index++)
	{
		FMemory::Free(SavedLeakedPointers[Index]);
	}

	// free the non-leaked pointers from this call to this function
	for (int32 Index = 0; Index < FreedPointers.Num(); Index++)
	{
		FMemory::Free(FreedPointers[Index]);
	}
#endif
}
