// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSPlatformMemory.cpp: IOS platform memory functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "MallocBinned.h"
#include "MallocAnsi.h"

#include <sys/mman.h>

void FIOSPlatformMemory::Init()
{
	FGenericPlatformMemory::Init();

	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	UE_LOG(LogInit, Log, TEXT("Memory total: Physical=%.1fGB (%dGB approx) Pagefile=%.1fGB Virtual=%.1fGB"), 
		float(MemoryConstants.TotalPhysical/1024.0/1024.0/1024.0),
		MemoryConstants.TotalPhysicalGB, 
		float((MemoryConstants.TotalVirtual-MemoryConstants.TotalPhysical)/1024.0/1024.0/1024.0), 
		float(MemoryConstants.TotalVirtual/1024.0/1024.0/1024.0) );
}

FPlatformMemoryStats FIOSPlatformMemory::GetStats()
{
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();

	FPlatformMemoryStats MemoryStats;

	// Gather system-wide memory stats.
	vm_statistics Stats;
	mach_msg_type_number_t StatsSize = sizeof(Stats);
	host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&Stats, &StatsSize);
	uint64_t FreeMem = Stats.free_count * MemoryConstants.PageSize;
	uint64_t SysUsedMem = (Stats.active_count + Stats.inactive_count + Stats.wire_count) * MemoryConstants.PageSize;

	// Gather process memory stats.
	task_basic_info Info;
	mach_msg_type_number_t InfoSize = sizeof(Info);
	task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&Info, &InfoSize);

	MemoryStats.AvailablePhysical = FreeMem;
	MemoryStats.AvailableVirtual = 0;
	MemoryStats.UsedPhysical = Info.resident_size;
	MemoryStats.UsedVirtual = Info.virtual_size;

	return MemoryStats;
}

const FPlatformMemoryConstants& FIOSPlatformMemory::GetConstants()
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
		vm_statistics Stats;
		mach_msg_type_number_t StatsSize = sizeof(Stats);
		host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&Stats, &StatsSize);
		uint64_t FreeMem = Stats.free_count * PageSize;
		uint64_t UsedMem = (Stats.active_count + Stats.inactive_count + Stats.wire_count) * PageSize;
		uint64_t TotalPhys = FreeMem + UsedMem;
		uint64_t TotalPageFile = SwapUsage.xsu_total;
		uint64_t TotalVirtual = TotalPhys + TotalPageFile;
	
		MemoryConstants.TotalPhysical = TotalPhys;
		MemoryConstants.TotalVirtual = TotalVirtual;
		MemoryConstants.PageSize = (uint32)PageSize;

		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
	}

	return MemoryConstants;	
}

FMalloc* FIOSPlatformMemory::BaseAllocator()
{
	// get free memory
	vm_size_t PageSize;
	host_page_size(mach_host_self(), &PageSize);

	vm_statistics Stats;
	mach_msg_type_number_t StatsSize = sizeof(Stats);
	host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&Stats, &StatsSize);
	// 1 << FMath::CeilLogTwo(MemoryConstants.TotalPhysical) should really be FMath::RoundUpToPowerOfTwo,
	// but that overflows to 0 when MemoryConstants.TotalPhysical is close to 4GB, since CeilLogTwo returns 32
	// this then causes the MemoryLimit to be 0 and crashing the app
	uint64 MemoryLimit = FMath::Min<uint64>( uint64(1) << FMath::CeilLogTwo(Stats.free_count * PageSize), 0x100000000);

	//return new FMallocAnsi();
	return new FMallocBinned(PageSize, MemoryLimit);
}

bool FIOSPlatformMemory::PageProtect(void* const Ptr, const SIZE_T Size, const bool bCanRead, const bool bCanWrite)
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
	return mprotect(Ptr, Size, ProtectMode) == 0;
}

void* FIOSPlatformMemory::BinnedAllocFromOS( SIZE_T Size )
{
	return mmap(nullptr, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

void FIOSPlatformMemory::BinnedFreeToOS( void* Ptr, SIZE_T Size )
{
	if (munmap(Ptr, Size) != 0)
	{
		const int ErrNo = errno;
		UE_LOG(LogHAL, Fatal, TEXT("munmap(addr=%p, len=%llu) failed with errno = %d (%s)"), Ptr, Size,
			   ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
	}
}




