// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#include "MallocTBB.h"
#include "MallocAnsi.h"
#include "MallocStomp.h"
#include "GenericPlatformMemoryPoolStats.h"
#include "MemoryMisc.h"

#if ENABLE_WIN_ALLOC_TRACKING
#include <crtdbg.h>
#endif // ENABLE_WIN_ALLOC_TRACKING

#if !FORCE_ANSI_ALLOCATOR
#include "MallocBinnedRedirect.h"
#endif

#include "AllowWindowsPlatformTypes.h"
#include <Psapi.h>
#pragma comment(lib, "psapi.lib")

DECLARE_MEMORY_STAT(TEXT("Windows Specific Memory Stat"),	STAT_WindowsSpecificMemoryStat, STATGROUP_MemoryPlatform);

#if ENABLE_WIN_ALLOC_TRACKING
// This allows tracking of allocations that don't happen within the engine's wrappers.
// You will probably want to set conditional breakpoints here to capture specific allocations
// which aren't related to static initialization, they will happen on the CRT anyway.
int WindowsAllocHook(int nAllocType, void *pvData,
				  size_t nSize, int nBlockUse, long lRequest,
				  const unsigned char * szFileName, int nLine )
{
	return true;
}
#endif // ENABLE_WIN_ALLOC_TRACKING

#include "GenericPlatformMemoryPoolStats.h"


void FWindowsPlatformMemory::Init()
{
	FGenericPlatformMemory::Init();

#if PLATFORM_32BITS
	const int64 GB(1024*1024*1024);
	SET_MEMORY_STAT(MCR_Physical, 2*GB); //2Gb of physical memory on win32
#endif

	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
#if PLATFORM_32BITS	
	UE_LOG(LogMemory, Log, TEXT("Memory total: Physical=%.1fGB (%dGB approx) Virtual=%.1fGB"), 
		float(MemoryConstants.TotalPhysical/1024.0/1024.0/1024.0),
		MemoryConstants.TotalPhysicalGB, 
		float(MemoryConstants.TotalVirtual/1024.0/1024.0/1024.0) );
#else
	// Logging virtual memory size for 64bits is pointless.
	UE_LOG(LogMemory, Log, TEXT("Memory total: Physical=%.1fGB (%dGB approx)"), 
		float(MemoryConstants.TotalPhysical/1024.0/1024.0/1024.0),
		MemoryConstants.TotalPhysicalGB );
#endif //PLATFORM_32BITS

	DumpStats( *GLog );
}

FMalloc* FWindowsPlatformMemory::BaseAllocator()
{
#if ENABLE_WIN_ALLOC_TRACKING
	// This allows tracking of allocations that don't happen within the engine's wrappers.
	// This actually won't be compiled unless bDebugBuildsActuallyUseDebugCRT is set in the
	// build configuration for UBT.
	_CrtSetAllocHook(WindowsAllocHook);
#endif // ENABLE_WIN_ALLOC_TRACKING

#if FORCE_ANSI_ALLOCATOR
	return new FMallocAnsi();
#elif USE_MALLOC_STOMP
	// Allocator that helps track down memory stomps.
	return new FMallocStomp();
#elif (WITH_EDITORONLY_DATA || IS_PROGRAM) && TBB_ALLOCATOR_ALLOWED
	return new FMallocTBB();
#else
	return new FMallocBinnedRedirect((uint32)(GetConstants().PageSize&MAX_uint32), (uint64)MAX_uint32+1);
#endif
}

FPlatformMemoryStats FWindowsPlatformMemory::GetStats()
{
	/**
	 *	GlobalMemoryStatusEx 
	 *	MEMORYSTATUSEX 
	 *		ullTotalPhys
	 *		ullAvailPhys
	 *		ullTotalVirtual
	 *		ullAvailVirtual
	 *		
	 *	GetProcessMemoryInfo
	 *	PROCESS_MEMORY_COUNTERS
	 *		WorkingSetSize
	 *		UsedVirtual
	 *		PeakUsedVirtual
	 *		
	 *	GetPerformanceInfo
	 *		PPERFORMANCE_INFORMATION 
	 *		PageSize
	 */

	// This method is slow, do not call it too often.
	// #TODO Should be executed only on the background thread.

	FPlatformMemoryStats MemoryStats;

	// Gather platform memory stats.
	MEMORYSTATUSEX MemoryStatusEx;
	FPlatformMemory::Memzero( &MemoryStatusEx, sizeof( MemoryStatusEx ) );
	MemoryStatusEx.dwLength = sizeof( MemoryStatusEx );
	::GlobalMemoryStatusEx( &MemoryStatusEx );

	PROCESS_MEMORY_COUNTERS ProcessMemoryCounters;
	FPlatformMemory::Memzero( &ProcessMemoryCounters, sizeof( ProcessMemoryCounters ) );
	::GetProcessMemoryInfo( ::GetCurrentProcess(), &ProcessMemoryCounters, sizeof(ProcessMemoryCounters) );

	MemoryStats.AvailablePhysical = MemoryStatusEx.ullAvailPhys;
	MemoryStats.AvailableVirtual = MemoryStatusEx.ullAvailVirtual;
	
	MemoryStats.UsedPhysical = ProcessMemoryCounters.WorkingSetSize;
	MemoryStats.PeakUsedPhysical = ProcessMemoryCounters.PeakWorkingSetSize;
	MemoryStats.UsedVirtual = ProcessMemoryCounters.PagefileUsage;
	MemoryStats.PeakUsedVirtual = ProcessMemoryCounters.PeakPagefileUsage;

	return MemoryStats;
}

void FWindowsPlatformMemory::GetStatsForMallocProfiler( FGenericMemoryStats& out_Stats )
{
#if	STATS
	FGenericPlatformMemory::GetStatsForMallocProfiler( out_Stats );

	FPlatformMemoryStats Stats = GetStats();

	// Windows specific stats.
	out_Stats.Add( GET_STATDESCRIPTION( STAT_WindowsSpecificMemoryStat ), Stats.WindowsSpecificMemoryStat );
#endif // STATS
}

const FPlatformMemoryConstants& FWindowsPlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if( MemoryConstants.TotalPhysical == 0 )
	{
		// Gather platform memory constants.
		MEMORYSTATUSEX MemoryStatusEx;
		FPlatformMemory::Memzero( &MemoryStatusEx, sizeof( MemoryStatusEx ) );
		MemoryStatusEx.dwLength = sizeof( MemoryStatusEx );
		::GlobalMemoryStatusEx( &MemoryStatusEx );

		SYSTEM_INFO SystemInfo;
		FPlatformMemory::Memzero( &SystemInfo, sizeof( SystemInfo ) );
		::GetSystemInfo(&SystemInfo);

		MemoryConstants.TotalPhysical = MemoryStatusEx.ullTotalPhys;
		MemoryConstants.TotalVirtual = MemoryStatusEx.ullTotalVirtual;
		MemoryConstants.PageSize = SystemInfo.dwAllocationGranularity;	// Use this so we get larger 64KiB pages, instead of 4KiB

		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
	}

	return MemoryConstants;	
}

bool FWindowsPlatformMemory::PageProtect(void* const Ptr, const SIZE_T Size, const bool bCanRead, const bool bCanWrite)
{
	DWORD flOldProtect;
	uint32 ProtectMode = 0;
	if (bCanRead && bCanWrite)
	{
		ProtectMode = PAGE_READWRITE;
	}
	else if (bCanWrite)
	{
		ProtectMode = PAGE_READWRITE;
	}
	else if (bCanRead)
	{
		ProtectMode = PAGE_READONLY;
	}
	else
	{
		ProtectMode = PAGE_NOACCESS;
	}
	return VirtualProtect(Ptr, Size, ProtectMode, &flOldProtect) != 0;
}
void* FWindowsPlatformMemory::BinnedAllocFromOS( SIZE_T Size )
{
	return VirtualAlloc( NULL, Size, MEM_COMMIT, PAGE_READWRITE );
}

void FWindowsPlatformMemory::BinnedFreeToOS( void* Ptr, SIZE_T Size )
{
	CA_SUPPRESS(6001)
	// Windows maintains the size of allocation internally, so Size is unused
	verify(VirtualFree( Ptr, 0, MEM_RELEASE ) != 0);
}

FPlatformMemory::FSharedMemoryRegion* FWindowsPlatformMemory::MapNamedSharedMemoryRegion(const FString& InName, bool bCreate, uint32 AccessMode, SIZE_T Size)
{
	FString Name(TEXT("Global\\"));
	Name += InName;

	DWORD OpenMappingAccess = FILE_MAP_READ;
	check(AccessMode != 0);
	if (AccessMode == FPlatformMemory::ESharedMemoryAccess::Write)
	{
		OpenMappingAccess = FILE_MAP_WRITE;
	}
	else if (AccessMode == (FPlatformMemory::ESharedMemoryAccess::Write | FPlatformMemory::ESharedMemoryAccess::Read))
	{
		OpenMappingAccess = FILE_MAP_ALL_ACCESS;
	}

	HANDLE Mapping = NULL;
	if (bCreate)
	{
		DWORD CreateMappingAccess = PAGE_READONLY;
		check(AccessMode != 0);
		if (AccessMode == FPlatformMemory::ESharedMemoryAccess::Write)
		{
			CreateMappingAccess = PAGE_WRITECOPY;
		}
		else if (AccessMode == (FPlatformMemory::ESharedMemoryAccess::Write | FPlatformMemory::ESharedMemoryAccess::Read))
		{
			CreateMappingAccess = PAGE_READWRITE;
		}

		DWORD MaxSizeHigh = 
#if PLATFORM_64BITS
			(Size >> 32);
#else
			0;
#endif // PLATFORM_64BITS

		DWORD MaxSizeLow = Size
#if PLATFORM_64BITS
			& 0xFFFFFFFF
#endif // PLATFORM_64BITS
			;

		Mapping = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, CreateMappingAccess, MaxSizeHigh, MaxSizeLow, *Name);

		if (Mapping == NULL)
		{
			DWORD ErrNo = GetLastError();
			UE_LOG(LogHAL, Warning, TEXT("CreateFileMapping(file=INVALID_HANDLE_VALUE, security=NULL, protect=0x%x, MaxSizeHigh=%d, MaxSizeLow=%d, name='%s') failed with GetLastError() = %d"), 
				CreateMappingAccess, MaxSizeHigh, MaxSizeLow, *Name,
				ErrNo
				);
		}
	}
	else
	{
		Mapping = OpenFileMapping(OpenMappingAccess, FALSE, *Name);

		if (Mapping == NULL)
		{
			DWORD ErrNo = GetLastError();
			UE_LOG(LogHAL, Warning, TEXT("OpenFileMapping(access=0x%x, inherit=false, name='%s') failed with GetLastError() = %d"), 
				OpenMappingAccess, *Name,
				ErrNo
				);
		}
	}

	if (Mapping == NULL)
	{
		return NULL;
	}

	void* Ptr = MapViewOfFile(Mapping, OpenMappingAccess, 0, 0, Size);
	if (Ptr == NULL)
	{
		DWORD ErrNo = GetLastError();
		UE_LOG(LogHAL, Warning, TEXT("MapViewOfFile(mapping=0x%x, access=0x%x, OffsetHigh=0, OffsetLow=0, NumBytes=%u) failed with GetLastError() = %d"), 
			Mapping, OpenMappingAccess, Size,
			ErrNo
			);

		CloseHandle(Mapping);
		return NULL;
	}

	return new FWindowsSharedMemoryRegion(Name, AccessMode, Ptr, Size, Mapping);
}

bool FWindowsPlatformMemory::UnmapNamedSharedMemoryRegion(FSharedMemoryRegion * MemoryRegion)
{
	bool bAllSucceeded = true;

	if (MemoryRegion)
	{
		FWindowsSharedMemoryRegion * WindowsRegion = static_cast< FWindowsSharedMemoryRegion* >( MemoryRegion );

		if (!UnmapViewOfFile(WindowsRegion->GetAddress()))
		{
			bAllSucceeded = false;

			int ErrNo = GetLastError();
			UE_LOG(LogHAL, Warning, TEXT("UnmapViewOfFile(address=%p) failed with GetLastError() = %d"), 
				WindowsRegion->GetAddress(),
				ErrNo
				);
		}

		if (!CloseHandle(WindowsRegion->GetMapping()))
		{
			bAllSucceeded = false;

			int ErrNo = GetLastError();
			UE_LOG(LogHAL, Warning, TEXT("CloseHandle(handle=0x%x) failed with GetLastError() = %d"), 
				WindowsRegion->GetMapping(),
				ErrNo
				);
		}

		// delete the region
		delete WindowsRegion;
	}

	return bAllSucceeded;
}

void FWindowsPlatformMemory::InternalUpdateStats( const FPlatformMemoryStats& MemoryStats )
{
	// Windows specific stats.
	SET_MEMORY_STAT( STAT_WindowsSpecificMemoryStat, MemoryStats.WindowsSpecificMemoryStat );
}

#include "HideWindowsPlatformTypes.h"
