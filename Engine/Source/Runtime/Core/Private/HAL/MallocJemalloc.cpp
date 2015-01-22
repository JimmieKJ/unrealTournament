// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "MallocJemalloc.h"

// Only use for supported platforms
#if PLATFORM_SUPPORTS_JEMALLOC

#define JEMALLOC_NO_DEMANGLE	// use stable function names prefixed with je_
#include "jemalloc.h"

#ifndef JEMALLOC_VERSION
#error JEMALLOC_VERSION not defined!
#endif

/** Value we fill a memory block with after it is free, in UE_BUILD_DEBUG and UE_BUILD_DEVELOPMENT **/
#define DEBUG_FILL_FREED (0xdd)

/** Value we fill a new memory block with, in UE_BUILD_DEBUG and UE_BUILD_DEVELOPMENT **/
#define DEBUG_FILL_NEW (0xcd)

void* FMallocJemalloc::Malloc( SIZE_T Size, uint32 Alignment )
{
	MEM_TIME(MemTime -= FPlatformTime::Seconds());

	void* Free = NULL;
	if( Alignment != DEFAULT_ALIGNMENT )
	{
		Alignment = FMath::Max(Size >= 16 ? (uint32)16 : (uint32)8, Alignment);

		// use aligned_alloc when allocating exact multiplies of an alignment
		// use the fact that Alignment is power of 2 and avoid %, but check it
		checkSlow(0 == (Alignment & (Alignment - 1)));
		if ((Size & (Alignment - 1)) == 0)
		{
			Free = je_aligned_alloc(Alignment, Size);
		}
		else if (0 != je_posix_memalign(&Free, Alignment, Size))
		{
			Free = NULL;	// explicitly nullify
		};
	}
	else
	{
		Free = je_malloc( Size );
	}

	if( !Free )
	{
		OutOfMemory();
	}

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	else if (Size)
	{
		FMemory::Memset(Free, DEBUG_FILL_NEW, Size); 
	}
#endif

	MEM_TIME(MemTime += FPlatformTime::Seconds());
	return Free;	
}

void* FMallocJemalloc::Realloc( void* Ptr, SIZE_T NewSize, uint32 Alignment )
{
	check(Alignment == DEFAULT_ALIGNMENT && "Alignment with realloc is not supported with FMallocJemalloc");
	MEM_TIME(MemTime -= FPlatformTime::Seconds());

	SIZE_T OldSize = 0;
	if (Ptr)
	{
		OldSize = je_malloc_usable_size(Ptr);
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		if (NewSize < OldSize)
		{
			FMemory::Memset((uint8*)Ptr + NewSize, DEBUG_FILL_FREED, OldSize - NewSize); 
		}
#endif
	}
	void* NewPtr = NULL;
	if (Alignment != DEFAULT_ALIGNMENT)
	{
		NewPtr = Malloc(NewSize, Alignment);
		if (Ptr)
		{
			FMemory::Memcpy(NewPtr, Ptr, OldSize);
			Free(Ptr);
		}
	}
	else
	{
		NewPtr = je_realloc(Ptr, NewSize);
	}
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	if (NewPtr && NewSize > OldSize)
	{
		FMemory::Memset((uint8*)NewPtr + OldSize, DEBUG_FILL_NEW, NewSize - OldSize); 
	}
#endif

	MEM_TIME(MemTime += FPlatformTime::Seconds());
	return NewPtr;
}
	
void FMallocJemalloc::Free( void* Ptr )
{
	if( !Ptr )
	{
		return;
	}

	MEM_TIME(MemTime -= FPlatformTime::Seconds());

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	FMemory::Memset(Ptr, DEBUG_FILL_FREED, je_malloc_usable_size(Ptr)); 
#endif
	je_free(Ptr);

	MEM_TIME(MemTime += FPlatformTime::Seconds());
}

namespace
{
	void JemallocStatsPrintCallback(void *UserData, const char *String)
	{
		FOutputDevice* Ar = reinterpret_cast< FOutputDevice* >(UserData);

		check(Ar);
		if (Ar)
		{
			Ar->Logf(ANSI_TO_TCHAR(String));
		}
	}
}

void FMallocJemalloc::DumpAllocatorStats( FOutputDevice& Ar ) 
{
	MEM_TIME(Ar.Logf( TEXT("Seconds     % 5.3f"), MemTime ));
	je_malloc_stats_print(JemallocStatsPrintCallback, &Ar, NULL);
}

bool FMallocJemalloc::GetAllocationSize(void *Original, SIZE_T &SizeOut)
{
	SizeOut = je_malloc_usable_size(Original);
	return true;
}

#undef DEBUG_FILL_FREED
#undef DEBUG_FILL_NEW

#endif // PLATFORM_SUPPORTS_JEMALLOC
