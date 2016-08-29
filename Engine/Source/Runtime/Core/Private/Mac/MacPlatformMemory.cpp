// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MacPlatformMemory.cpp: Mac platform memory functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "MallocTBB.h"
#include "MallocAnsi.h"
#include "MallocBinned.h"
#include "MallocStomp.h"

#include <sys/param.h>
#include <sys/mount.h>

void FMacPlatformMemory::Init()
{
	FGenericPlatformMemory::Init();

	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	UE_LOG(LogInit, Log, TEXT("Memory total: Physical=%.1fGB (%dGB approx) Pagefile=%.1fGB Virtual=%.1fGB"), 
		float(MemoryConstants.TotalPhysical/1024.0/1024.0/1024.0),
		MemoryConstants.TotalPhysicalGB, 
		float((MemoryConstants.TotalVirtual-MemoryConstants.TotalPhysical)/1024.0/1024.0/1024.0), 
		float(MemoryConstants.TotalVirtual/1024.0/1024.0/1024.0) );
}

FMalloc* FMacPlatformMemory::BaseAllocator()
{
	bool bIsMavericks = false;

	char OSRelease[PATH_MAX] = {};
	size_t OSReleaseBufferSize = PATH_MAX;
	if (sysctlbyname("kern.osrelease", OSRelease, &OSReleaseBufferSize, NULL, 0) == 0)
	{
		int32 OSVersionMajor = 0;
		if (sscanf(OSRelease, "%d", &OSVersionMajor) == 1)
		{
			bIsMavericks = OSVersionMajor <= 13;
		}
	}

	if(getenv("UE4_FORCE_MALLOC_ANSI") != nullptr || bIsMavericks)
	{
		return new FMallocAnsi();
	}
	else
	{
#if FORCE_ANSI_ALLOCATOR || IS_PROGRAM
		return new FMallocAnsi();
#elif USE_MALLOC_STOMP
		return new FMallocStomp();
#elif (WITH_EDITORONLY_DATA || IS_PROGRAM) && TBB_ALLOCATOR_ALLOWED
		return new FMallocTBB();
#else
		return new FMallocBinned((uint32)(GetConstants().PageSize&MAX_uint32), 0x100000000);
#endif
	}
}

FPlatformMemoryStats FMacPlatformMemory::GetStats()
{
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();

	FPlatformMemoryStats MemoryStats;

	// Gather platform memory stats.
	vm_statistics Stats;
	mach_msg_type_number_t StatsSize = sizeof(Stats);
	host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&Stats, &StatsSize);
	uint64_t FreeMem = Stats.free_count * MemoryConstants.PageSize;
	MemoryStats.AvailablePhysical = FreeMem;
	
	// Get swap file info
	xsw_usage SwapUsage;
	SIZE_T Size = sizeof(SwapUsage);
	sysctlbyname("vm.swapusage", &SwapUsage, &Size, NULL, 0);
	MemoryStats.AvailableVirtual = FreeMem + SwapUsage.xsu_avail;

	// Just get memory information for the process and report the working set instead
	mach_task_basic_info_data_t TaskInfo;
	mach_msg_type_number_t TaskInfoCount = MACH_TASK_BASIC_INFO_COUNT;
	task_info( mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&TaskInfo, &TaskInfoCount );
	MemoryStats.UsedPhysical = TaskInfo.resident_size;
	if(MemoryStats.UsedPhysical > MemoryStats.PeakUsedPhysical)
	{
		MemoryStats.PeakUsedPhysical = MemoryStats.UsedPhysical;
	}
	MemoryStats.UsedVirtual = TaskInfo.virtual_size;
	if(MemoryStats.UsedVirtual > MemoryStats.PeakUsedVirtual)
	{
		MemoryStats.PeakUsedVirtual = MemoryStats.UsedVirtual;
	}
	

	return MemoryStats;
}

const FPlatformMemoryConstants& FMacPlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if( MemoryConstants.TotalPhysical == 0 )
	{
		// Gather platform memory constants.

		// Get page size.
		vm_size_t PageSize;
		host_page_size(mach_host_self(), &PageSize);

		// Get swap file info
		xsw_usage SwapUsage;
		SIZE_T Size = sizeof(SwapUsage);
		sysctlbyname("vm.swapusage", &SwapUsage, &Size, NULL, 0);

		// Get memory.
		int64 AvailablePhysical = 0;
		int Mib[] = {CTL_HW, HW_MEMSIZE};
		size_t Length = sizeof(int64);
		sysctl(Mib, 2, &AvailablePhysical, &Length, NULL, 0);
		
		MemoryConstants.TotalPhysical = AvailablePhysical;
		MemoryConstants.TotalVirtual = AvailablePhysical + SwapUsage.xsu_total;
		MemoryConstants.PageSize = (uint32)PageSize;

		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
	}

	return MemoryConstants;	
}

bool FMacPlatformMemory::PageProtect(void* const Ptr, const SIZE_T Size, const bool bCanRead, const bool bCanWrite)
{
	int32 ProtectMode;
	if (bCanRead && bCanWrite)
	{
		ProtectMode = PROT_READ | PROT_WRITE;
	}
	else if (bCanRead)
	{
		ProtectMode = PROT_READ;
	}
	else if (bCanWrite)
	{
		ProtectMode = PROT_WRITE;
	}
	else
	{
		ProtectMode = PROT_NONE;
	}
	return mprotect(Ptr, Size, static_cast<int32>(ProtectMode)) == 0;
}

void* FMacPlatformMemory::BinnedAllocFromOS( SIZE_T Size )
{
	return mmap(nullptr, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

void FMacPlatformMemory::BinnedFreeToOS( void* Ptr, SIZE_T Size )
{
	if (munmap(Ptr, Size) != 0)
	{
		const int ErrNo = errno;
		UE_LOG(LogHAL, Fatal, TEXT("munmap(addr=%p, len=%llu) failed with errno = %d (%s)"), Ptr, Size,
			   ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
	}
}
