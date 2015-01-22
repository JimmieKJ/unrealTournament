// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	GenericPlatformMemory.h: Generic platform memory classes
==============================================================================================*/

#pragma once
#include "HAL/Platform.h"
#include "HAL/PlatformCodeAnalysis.h"
#include <wchar.h>
#include <string.h>

class FMalloc;
class FOutputDevice;
class FString;

/** Holds generic memory stats, internally implemented as a map. */
struct FGenericMemoryStats;

/** 
 * Struct used to hold common memory constants for all platforms.
 * These values don't change over the entire life of the executable.
 */
struct FGenericPlatformMemoryConstants
{
	/** The amount of actual physical memory, in bytes. */
	SIZE_T TotalPhysical;

	/** The amount of virtual memory, in bytes. */
	SIZE_T TotalVirtual;

	/** The size of a page, in bytes. */
	SIZE_T PageSize;

	/** Approximate physical RAM in GB; 1 on everything except PC. Used for "course tuning", like FPlatformMisc::NumberOfCores(). */
	uint32 TotalPhysicalGB;

	/** Default constructor, clears all variables. */
	FGenericPlatformMemoryConstants()
		: TotalPhysical( 0 )
		, TotalVirtual( 0 )
		, PageSize( 0 )
		, TotalPhysicalGB( 1 )
	{}

	/** Copy constructor, used by the generic platform memory stats. */
	FGenericPlatformMemoryConstants( const FGenericPlatformMemoryConstants& Other )
		: TotalPhysical( Other.TotalPhysical )
		, TotalVirtual( Other.TotalVirtual )
		, PageSize( Other.PageSize )
		, TotalPhysicalGB( Other.TotalPhysicalGB )
	{}
};

typedef FGenericPlatformMemoryConstants FPlatformMemoryConstants;

/** 
 * Struct used to hold common memory stats for all platforms.
 * These values may change over the entire life of the executable.
 */
struct FGenericPlatformMemoryStats : public FPlatformMemoryConstants
{
	/** The amount of physical memory currently available, in bytes. */
	SIZE_T AvailablePhysical;

	/** The amount of virtual memory currently available, in bytes. */
	SIZE_T AvailableVirtual;

	/** The amount of physical memory used by the process, in bytes. */
	SIZE_T UsedPhysical;

	/** The peak amount of physical memory used by the process, in bytes. */
	SIZE_T PeakUsedPhysical;

	/** Total amount of virtual memory used by the process. */
	SIZE_T UsedVirtual;

	/** The peak amount of virtual memory used by the process. */
	SIZE_T PeakUsedVirtual;
	
	/** Default constructor, clears all variables. */
	FGenericPlatformMemoryStats();
};

struct FPlatformMemoryStats;

/**
 * FMemory_Alloca/alloca implementation. This can't be a function, even FORCEINLINE'd because there's no guarantee that 
 * the memory returned in a function will stick around for the caller to use.
 */
#if PLATFORM_USES_MICROSOFT_LIBC_FUNCTIONS
#define __FMemory_Alloca_Func _alloca
#else
#define __FMemory_Alloca_Func alloca
#endif

#define FMemory_Alloca(Size )((Size==0) ? 0 : (void*)(((PTRINT)__FMemory_Alloca_Func(Size + 15) + 15) & ~15))

/** Generic implementation for most platforms, these tend to be unused and unimplemented. */
struct CORE_API FGenericPlatformMemory
{
	/** Set to true if we encounters out of memory. */
	static bool bIsOOM;

	/**
	 * Various memory regions that can be used with memory stats. The exact meaning of
	 * the enums are relatively platform-dependent, although the general ones (Physical, GPU)
	 * are straightforward. A platform can add more of these, and it won't affect other 
	 * platforms, other than a minuscule amount of memory for the StatManager to track the
	 * max available memory for each region (uses an array FPlatformMemory::MCR_MAX big)
	 */
	enum EMemoryCounterRegion
	{
		MCR_Invalid, // not memory
		MCR_Physical, // main system memory
		MCR_GPU, // memory directly a GPU (graphics card, etc)
		MCR_GPUSystem, // system memory directly accessible by a GPU
		MCR_TexturePool, // presized texture pools
		MCR_MAX
	};

	/**
	 * Flags used for shared memory creation/open
	 */
	enum ESharedMemoryAccess
	{
		Read	=		(1 << 1),
		Write	=		(1 << 2)
	};

	/**
	 * Generic representation of a shared memory region
	 */
	struct FSharedMemoryRegion
	{
		/** Returns the name of the region */
		const TCHAR *	GetName() const			{ return Name; }

		/** Returns the beginning of the region in process address space */
		void *			GetAddress()			{ return Address; }

		/** Returns the beginning of the region in process address space */
		const void *	GetAddress() const		{ return Address; }

		/** Returns size of the region in bytes */
		SIZE_T			GetSize() const			{ return Size; }
	
		
		FSharedMemoryRegion(const FString& InName, uint32 InAccessMode, void* InAddress, SIZE_T InSize);

	protected:

		enum Limits
		{
			MaxSharedMemoryName		=	128
		};

		/** Name of the region */
		TCHAR			Name[MaxSharedMemoryName];

		/** Access mode for the region */
		uint32			AccessMode;

		/** The actual buffer */
		void *			Address;

		/** Size of the buffer */
		SIZE_T			Size;
	};

	/** Initializes platform memory specific constants. */
	static void Init();
	
	static CA_NO_RETURN void OnOutOfMemory(uint64 Size, uint32 Alignment);

	/** Initializes the memory pools, should be called by the init function. */
	static void SetupMemoryPools();

	/**
	 * @return the default allocator.
	 */
	static FMalloc* BaseAllocator();

	/**
	 * @return platform specific current memory statistics.
	 */
	static FPlatformMemoryStats GetStats();

	/**
	 * Writes all platform specific current memory statistics in the format usable by the malloc profiler.
	 */
	static void GetStatsForMallocProfiler( FGenericMemoryStats& out_Stats );

	/**
	 * @return platform specific memory constants.
	 */
	static const FPlatformMemoryConstants& GetConstants();
	
	/**
	 * @return approximate physical RAM in GB.
	 */
	static uint32 GetPhysicalGBRam();

	/** Called once per frame, gathers and sets all platform memory statistics into the corresponding stats. */
	static void UpdateStats();

	/**
	 * Allocates pages from the OS.
	 *
	 * @param Size Size to allocate, not necessarily aligned
	 *
	 * @return OS allocated pointer for use by binned allocator
	 */
	static void* BinnedAllocFromOS( SIZE_T Size );
	
	/**
	 * Returns pages allocated by BinnedAllocFromOS to the OS.
	 *
	 * @param A pointer previously returned from BinnedAllocFromOS
	 */
	static void BinnedFreeToOS( void* Ptr );

	/** Dumps basic platform memory statistics into the specified output device. */
	static void DumpStats( FOutputDevice& Ar );

	/** Dumps basic platform memory statistics and allocator specific statistics into the specified output device. */
	static void DumpPlatformAndAllocatorStats( FOutputDevice& Ar );

	/** @name Memory functions */

	/** Copies count bytes of characters from Src to Dest. If some regions of the source
	 * area and the destination overlap, memmove ensures that the original source bytes
	 * in the overlapping region are copied before being overwritten.  NOTE: make sure
	 * that the destination buffer is the same size or larger than the source buffer!
	 */
	static FORCEINLINE void* Memmove( void* Dest, const void* Src, SIZE_T Count )
	{
		return memmove( Dest, Src, Count );
	}

	static FORCEINLINE int32 Memcmp( const void* Buf1, const void* Buf2, SIZE_T Count )
	{
		return memcmp( Buf1, Buf2, Count );
	}

	static FORCEINLINE void* Memset(void* Dest, uint8 Char, SIZE_T Count)
	{
		return memset( Dest, Char, Count );
	}

	static FORCEINLINE void* Memzero(void* Dest, SIZE_T Count)
	{
		return memset( Dest, 0, Count );
	}

	static FORCEINLINE void* Memcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return memcpy( Dest, Src, Count );
	}

	/** Memcpy optimized for big blocks. */
	static FORCEINLINE void* BigBlockMemcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return memcpy(Dest, Src, Count);
	}

	/** On some platforms memcpy optimized for big blocks that avoid L2 cache pollution are available */
	static FORCEINLINE void* StreamingMemcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return memcpy( Dest, Src, Count );
	}

	static void Memswap( void* Ptr1, void* Ptr2, SIZE_T Size );

	/**
	 * Maps a named shared memory region into process address space (creates or opens it)
	 *
	 * @param Name unique name of the shared memory region (should not contain [back]slashes to remain cross-platform).
	 * @param bCreate whether we're creating it or just opening existing (created by some other process).
	 * @param AccessMode mode which we will be accessing it (use values from ESharedMemoryAccess)
	 * @param Size size of the buffer (should be >0. Also, the real size is subject to platform limitations and may be increased to match page size)
	 *
	 * @return pointer to FSharedMemoryRegion (or its descendants) if successful, NULL if not.
	 */
	static FSharedMemoryRegion* MapNamedSharedMemoryRegion(const FString& Name, bool bCreate, uint32 AccessMode, SIZE_T Size);

	/**
	 * Unmaps a name shared memory region
	 *
	 * @param MemoryRegion an object that encapsulates a shared memory region (will be destroyed even if function fails!)
	 *
	 * @return true if successful
	 */
	static bool UnmapNamedSharedMemoryRegion(FSharedMemoryRegion * MemoryRegion);
};
