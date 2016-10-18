// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
		MCR_StreamingPool, // amount of texture pool available for streaming.
		MCR_UsedStreamingPool, // amount of texture pool used for streaming.
		MCR_GPUDefragPool, // presized pool of memory that can be defragmented.
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

	//~ Begin FGenericPlatformMemory Interface
	static void Init();
	static bool SupportBackupMemoryPool()
	{
		return true;
	}
	static class FMalloc* BaseAllocator();
	static FPlatformMemoryStats GetStats();
	static void GetStatsForMallocProfiler( FGenericMemoryStats& out_Stats );
	static const FPlatformMemoryConstants& GetConstants();
	static bool PageProtect(void* const Ptr, const SIZE_T Size, const bool bCanRead, const bool bCanWrite);
	static void* BinnedAllocFromOS( SIZE_T Size );
	static void BinnedFreeToOS( void* Ptr, SIZE_T Size );
	static FSharedMemoryRegion* MapNamedSharedMemoryRegion(const FString& InName, bool bCreate, uint32 AccessMode, SIZE_T Size);
	static bool UnmapNamedSharedMemoryRegion(FSharedMemoryRegion * MemoryRegion);
protected:
	friend struct FGenericStatsUpdater;

	static void InternalUpdateStats( const FPlatformMemoryStats& MemoryStats );
	//~ End FGenericPlatformMemory Interface
};


typedef FWindowsPlatformMemory FPlatformMemory;
