// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformMemory.h"
#include "Windows/WindowsSystemIncludes.h"


/**
 *	Windows implementation of the FGenericPlatformMemoryStats.
 *	At this moment it's just the same as the FGenericPlatformMemoryStats.
 *	Can be extended as shown in the following example.
 */
struct FPlatformMemoryStats
	: public FGenericPlatformMemoryStats
{
	/** Default constructor, clears all variables. */
	FPlatformMemoryStats()
		: FGenericPlatformMemoryStats()
		, WindowsSpecificMemoryStat(0)
	{ }

	/** Memory stat specific only for Windows. */
	SIZE_T WindowsSpecificMemoryStat;
};


/**
* Windows implementation of the memory OS functions
**/
struct CORE_API FWindowsPlatformMemory
	: public FGenericPlatformMemory
{
	enum EMemoryCounterRegion
	{
		MCR_Invalid, // not memory
		MCR_Physical, // main system memory
		MCR_GPU, // memory directly a GPU (graphics card, etc)
		MCR_GPUSystem, // system memory directly accessible by a GPU
		MCR_TexturePool, // presized texture pools
		MCR_SamplePlatformSpecifcMemoryRegion, 
		MCR_MAX
	};

	/**
	 * Windows representation of a shared memory region
	 */
	struct FWindowsSharedMemoryRegion : public FSharedMemoryRegion
	{
		/** Returns the handle to file mapping object. */
		HANDLE GetMapping() const { return Mapping; }

		FWindowsSharedMemoryRegion(const FString& InName, uint32 InAccessMode, void* InAddress, SIZE_T InSize, HANDLE InMapping)
			:	FSharedMemoryRegion(InName, InAccessMode, InAddress, InSize)
			,	Mapping(InMapping)
		{}

	protected:

		/** Handle of a file mapping object */
		HANDLE				Mapping;
	};

	// Begin FGenericPlatformMemory interface
	static void Init();
	static class FMalloc* BaseAllocator();
	static FPlatformMemoryStats GetStats();
	static void GetStatsForMallocProfiler( FGenericMemoryStats& out_Stats );
	static const FPlatformMemoryConstants& GetConstants();
	static void UpdateStats();
	static void* BinnedAllocFromOS( SIZE_T Size );
	static void BinnedFreeToOS( void* Ptr );
	static FSharedMemoryRegion* MapNamedSharedMemoryRegion(const FString& InName, bool bCreate, uint32 AccessMode, SIZE_T Size);
	static bool UnmapNamedSharedMemoryRegion(FSharedMemoryRegion * MemoryRegion);
	// End FGenericPlatformMemory interface
};


typedef FWindowsPlatformMemory FPlatformMemory;
