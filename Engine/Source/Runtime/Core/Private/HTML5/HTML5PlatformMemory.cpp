// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5PlatformMemory.cpp: HTML5 platform memory functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "MallocBinned.h"
#include "MallocAnsi.h"

void FHTML5PlatformMemory::Init()
{
	FGenericPlatformMemory::Init();

	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	UE_LOG(LogInit, Log, TEXT("Memory total: Physical=%.1fGB (%dGB approx) "), 
		(double)MemoryConstants.TotalPhysical / 1024.0f / 1024.0f / 1024.0f,
		MemoryConstants.TotalPhysicalGB);
}

const FPlatformMemoryConstants& FHTML5PlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if( MemoryConstants.TotalPhysical == 0 )
	{
		// Gather platform memory stats.
#if PLATFORM_HTML5_WIN32
		uint64 GTotalMemoryAvailable = 512 * 1024 * 1024;
#else
		uint64 GTotalMemoryAvailable =  EM_ASM_INT_V({ return Module.TOTAL_MEMORY; }); 
#endif

		MemoryConstants.TotalPhysical = GTotalMemoryAvailable;
		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
	}

	return MemoryConstants;	
}

FPlatformMemoryStats FHTML5PlatformMemory::GetStats()
{   
    // @todo 
	FPlatformMemoryStats MemoryStats;
	return MemoryStats;
}


FMalloc* FHTML5PlatformMemory::BaseAllocator()
{
#if !PLATFORM_HTML5_WIN32 
	return new FMallocAnsi();
#else 
	return new FMallocBinned(32 * 1024, 1 << 30 );
#endif 
}

void* FHTML5PlatformMemory::BinnedAllocFromOS( SIZE_T Size )
{ 
#if PLATFORM_HTML5_WIN32 
	return _aligned_malloc( Size, 32 * 1024  );
#else  
	return FMemory::Malloc(Size, 16);
#endif 
}

void FHTML5PlatformMemory::BinnedFreeToOS( void* Ptr, SIZE_T Size )
{
#if PLATFORM_HTML5_WIN32 
	_aligned_free ( Ptr );
#else
	FMemory::Free(Ptr);
#endif 
}

