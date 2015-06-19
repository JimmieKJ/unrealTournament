// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#include "MallocTBB.h"
#include "MallocAnsi.h"
#include "GenericPlatformMemoryPoolStats.h"
#include "MemoryMisc.h"

#if !FORCE_ANSI_ALLOCATOR
#include "MallocBinned.h"
#endif

#include "AllowWindowsPlatformTypes.h"
#include <Psapi.h>
#pragma comment(lib, "psapi.lib")

DECLARE_MEMORY_STAT(TEXT("Windows Specific Memory Stat"),	STAT_WindowsSpecificMemoryStat, STATGROUP_MemoryPlatform);

/** Enable this to track down windows allocations not wrapped by our wrappers
int WindowsAllocHook(int nAllocType, void *pvData,
				  size_t nSize, int nBlockUse, long lRequest,
				  const unsigned char * szFileName, int nLine )
{
	if ((nAllocType == _HOOK_ALLOC || nAllocType == _HOOK_REALLOC) && nSize > 2048)
	{
		static int i = 0;
		i++;
	}
	return true;
}
*/

#include "GenericPlatformMemoryPoolStats.h"


void FWindowsPlatformMemory::Init()
{
	FGenericPlatformMemory::SetupMemoryPools();

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

	UpdateStats();
	DumpStats( *GLog );
}

FMalloc* FWindowsPlatformMemory::BaseAllocator()
{
#if FORCE_ANSI_ALLOCATOR
	return new FMallocAnsi();
#elif (WITH_EDITORONLY_DATA || IS_PROGRAM) && TBB_ALLOCATOR_ALLOWED
	return new FMallocTBB();
#else
	return new FMallocBinned((uint32)(GetConstants().PageSize&MAX_uint32), (uint64)MAX_uint32+1);
#endif

//	_CrtSetAllocHook(WindowsAllocHook); // Enable to track down windows allocs not handled by our wrapper
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

	FPlatformMemoryStats MemoryStats;

	// Gather platform memory stats.
	MEMORYSTATUSEX MemoryStatusEx = {0};
	MemoryStatusEx.dwLength = sizeof( MemoryStatusEx );
	::GlobalMemoryStatusEx( &MemoryStatusEx );

	PROCESS_MEMORY_COUNTERS ProcessMemoryCounters = {0};
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
	out_Stats.Add(TEXT("Windows Specific Memory Stat"), Stats.WindowsSpecificMemoryStat );
#endif // STATS
}

const FPlatformMemoryConstants& FWindowsPlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if( MemoryConstants.TotalPhysical == 0 )
	{
		// Gather platform memory constants.
		MEMORYSTATUSEX MemoryStatusEx = {0};
		MemoryStatusEx.dwLength = sizeof( MemoryStatusEx );
		::GlobalMemoryStatusEx( &MemoryStatusEx );

		PERFORMANCE_INFORMATION PerformanceInformation = {0};
		::GetPerformanceInfo( &PerformanceInformation, sizeof(PerformanceInformation) );

		MemoryConstants.TotalPhysical = MemoryStatusEx.ullTotalPhys;
		MemoryConstants.TotalVirtual = MemoryStatusEx.ullTotalVirtual;
		MemoryConstants.PageSize = PerformanceInformation.PageSize;

		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
	}

	return MemoryConstants;	
}

void FWindowsPlatformMemory::UpdateStats()
{
#if STATS
	if (FThreadStats::IsCollectingData(GET_STATID(STAT_TotalPhysical)))
	{
		FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
		SET_MEMORY_STAT(STAT_TotalPhysical,MemoryStats.TotalPhysical);
		SET_MEMORY_STAT(STAT_TotalVirtual,MemoryStats.TotalVirtual);
		SET_MEMORY_STAT(STAT_PageSize,MemoryStats.PageSize);
		SET_MEMORY_STAT(STAT_TotalPhysicalGB,MemoryStats.TotalPhysicalGB);

		SET_MEMORY_STAT(STAT_AvailablePhysical,MemoryStats.AvailablePhysical);
		SET_MEMORY_STAT(STAT_AvailableVirtual,MemoryStats.AvailableVirtual);
		SET_MEMORY_STAT(STAT_UsedPhysical,MemoryStats.UsedPhysical);
		SET_MEMORY_STAT(STAT_PeakUsedPhysical,MemoryStats.PeakUsedPhysical);
		SET_MEMORY_STAT(STAT_UsedVirtual,MemoryStats.UsedVirtual);
		SET_MEMORY_STAT(STAT_PeakUsedVirtual,MemoryStats.PeakUsedVirtual);

		// Windows specific stats.
		SET_MEMORY_STAT(STAT_WindowsSpecificMemoryStat,MemoryStats.WindowsSpecificMemoryStat);
	}
#endif
}

void* FWindowsPlatformMemory::BinnedAllocFromOS( SIZE_T Size )
{
	return VirtualAlloc( NULL, Size, MEM_COMMIT, PAGE_READWRITE );
}

void FWindowsPlatformMemory::BinnedFreeToOS( void* Ptr )
{
	CA_SUPPRESS(6001)
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
#include "HideWindowsPlatformTypes.h"
