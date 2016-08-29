// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "Ticker.h"
#include "Async.h"
#include "MallocAnsi.h"
#include "GenericPlatformMemoryPoolStats.h"
#include "MemoryMisc.h"


DEFINE_STAT(MCR_Physical);
DEFINE_STAT(MCR_GPU);
DEFINE_STAT(MCR_TexturePool);
DEFINE_STAT(MCR_StreamingPool);
DEFINE_STAT(MCR_UsedStreamingPool);

DEFINE_STAT(STAT_TotalPhysical);
DEFINE_STAT(STAT_TotalVirtual);
DEFINE_STAT(STAT_PageSize);
DEFINE_STAT(STAT_TotalPhysicalGB);

DEFINE_STAT(STAT_AvailablePhysical);
DEFINE_STAT(STAT_AvailableVirtual);
DEFINE_STAT(STAT_UsedPhysical);
DEFINE_STAT(STAT_PeakUsedPhysical);
DEFINE_STAT(STAT_UsedVirtual);
DEFINE_STAT(STAT_PeakUsedVirtual);

/** Helper class used to update platform memory stats. */
struct FGenericStatsUpdater
{
	/** Called once per second, enqueues stats update. */
	static bool EnqueueUpdateStats( float /*InDeltaTime*/ )
	{
		AsyncTask( ENamedThreads::AnyBackgroundThreadNormalTask, []()
		{
			DoUpdateStats();
		} );
		return true; // Tick again
	}

	/** Gathers and sets all platform memory statistics into the corresponding stats. */
	static void DoUpdateStats()
	{
		// This is slow, so do it on the task graph.
		FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
		SET_MEMORY_STAT( STAT_TotalPhysical, MemoryStats.TotalPhysical );
		SET_MEMORY_STAT( STAT_TotalVirtual, MemoryStats.TotalVirtual );
		SET_MEMORY_STAT( STAT_PageSize, MemoryStats.PageSize );
		SET_MEMORY_STAT( STAT_TotalPhysicalGB, MemoryStats.TotalPhysicalGB );

		SET_MEMORY_STAT( STAT_AvailablePhysical, MemoryStats.AvailablePhysical );
		SET_MEMORY_STAT( STAT_AvailableVirtual, MemoryStats.AvailableVirtual );
		SET_MEMORY_STAT( STAT_UsedPhysical, MemoryStats.UsedPhysical );
		SET_MEMORY_STAT( STAT_PeakUsedPhysical, MemoryStats.PeakUsedPhysical );
		SET_MEMORY_STAT( STAT_UsedVirtual, MemoryStats.UsedVirtual );
		SET_MEMORY_STAT( STAT_PeakUsedVirtual, MemoryStats.PeakUsedVirtual );

		// Platform specific stats.
		FPlatformMemory::InternalUpdateStats( MemoryStats );
	}
};

FGenericPlatformMemoryStats::FGenericPlatformMemoryStats()
	: FGenericPlatformMemoryConstants( FPlatformMemory::GetConstants() )
	, AvailablePhysical( 0 )
	, AvailableVirtual( 0 )
	, UsedPhysical( 0 )
	, PeakUsedPhysical( 0 )
	, UsedVirtual( 0 )
	, PeakUsedVirtual( 0 )
{}

bool FGenericPlatformMemory::bIsOOM = false;
uint64 FGenericPlatformMemory::OOMAllocationSize = 0;
uint32 FGenericPlatformMemory::OOMAllocationAlignment = 0;

/**
 * Value determined by series of tests on Fortnite with limited process memory.
 * 26MB sufficed to report all test crashes, using 32MB to have some slack.
 * If this pool is too large, use the following values to determine proper size:
 * 2MB pool allowed to report 78% of crashes.
 * 6MB pool allowed to report 90% of crashes.
 */
uint32 FGenericPlatformMemory::BackupOOMMemoryPoolSize = 32 * 1024 * 1024;
void* FGenericPlatformMemory::BackupOOMMemoryPool = nullptr;

void FGenericPlatformMemory::SetupMemoryPools()
{
	SET_MEMORY_STAT(MCR_Physical, 0); // "unlimited" physical memory, we still need to make this call to set the short name, etc
	SET_MEMORY_STAT(MCR_GPU, 0); // "unlimited" GPU memory, we still need to make this call to set the short name, etc
	SET_MEMORY_STAT(MCR_TexturePool, 0); // "unlimited" Texture memory, we still need to make this call to set the short name, etc
	SET_MEMORY_STAT(MCR_StreamingPool, 0);
	SET_MEMORY_STAT(MCR_UsedStreamingPool, 0);

	BackupOOMMemoryPool = FPlatformMemory::BinnedAllocFromOS(BackupOOMMemoryPoolSize);
}

void FGenericPlatformMemory::Init()
{
	if (FPlatformMemory::SupportBackupMemoryPool())
	{
		SetupMemoryPools();
	}

#if	STATS
	// Stats are updated only once per second.
	const float PollingInterval = 1.0f;
	FTicker::GetCoreTicker().AddTicker( FTickerDelegate::CreateStatic( &FGenericStatsUpdater::EnqueueUpdateStats ), PollingInterval );

	// Update for the first time.
	FGenericStatsUpdater::DoUpdateStats();
#endif // STATS
}

void FGenericPlatformMemory::OnOutOfMemory(uint64 Size, uint32 Alignment)
{
	// Update memory stats before we enter the crash handler.

	OOMAllocationSize = Size;
	OOMAllocationAlignment = Alignment;

	bIsOOM = true;
	FPlatformMemoryStats PlatformMemoryStats = FPlatformMemory::GetStats();
	if (BackupOOMMemoryPool)
	{
		FPlatformMemory::BinnedFreeToOS(BackupOOMMemoryPool, BackupOOMMemoryPoolSize);
		UE_LOG(LogMemory, Warning, TEXT("Freeing %d bytes from backup pool to handle out of memory."), BackupOOMMemoryPoolSize);
	}
	UE_LOG(LogMemory, Warning, TEXT("MemoryStats:")\
		TEXT("\n\tAvailablePhysical %llu")\
		TEXT("\n\t AvailableVirtual %llu")\
		TEXT("\n\t     UsedPhysical %llu")\
		TEXT("\n\t PeakUsedPhysical %llu")\
		TEXT("\n\t      UsedVirtual %llu")\
		TEXT("\n\t  PeakUsedVirtual %llu"),
		(uint64)PlatformMemoryStats.AvailablePhysical,
		(uint64)PlatformMemoryStats.AvailableVirtual,
		(uint64)PlatformMemoryStats.UsedPhysical,
		(uint64)PlatformMemoryStats.PeakUsedPhysical,
		(uint64)PlatformMemoryStats.UsedVirtual,
		(uint64)PlatformMemoryStats.PeakUsedVirtual);
	if (GWarn)
	{
		GMalloc->DumpAllocatorStats(*GWarn);
	}
	UE_LOG(LogMemory, Fatal, TEXT("Ran out of memory allocating %llu bytes with alignment %u"), Size, Alignment);
}

FMalloc* FGenericPlatformMemory::BaseAllocator()
{
	return new FMallocAnsi();
}

FPlatformMemoryStats FGenericPlatformMemory::GetStats()
{
	UE_LOG(LogMemory, Warning, TEXT("FGenericPlatformMemory::GetStats not implemented on this platform"));
	return FPlatformMemoryStats();
}

void FGenericPlatformMemory::GetStatsForMallocProfiler( FGenericMemoryStats& out_Stats )
{
#if	STATS
	FPlatformMemoryStats Stats = FPlatformMemory::GetStats();

	// Base common stats for all platforms.
	out_Stats.Add( GET_STATDESCRIPTION( STAT_TotalPhysical ), Stats.TotalPhysical );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_TotalVirtual ), Stats.TotalVirtual );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_PageSize ), Stats.PageSize );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_TotalPhysicalGB ), (SIZE_T)Stats.TotalPhysicalGB );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_AvailablePhysical ), Stats.AvailablePhysical );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_AvailableVirtual ), Stats.AvailableVirtual );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_UsedPhysical ), Stats.UsedPhysical );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_PeakUsedPhysical ), Stats.PeakUsedPhysical );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_UsedVirtual ), Stats.UsedVirtual );
	out_Stats.Add( GET_STATDESCRIPTION( STAT_PeakUsedVirtual ), Stats.PeakUsedVirtual );
#endif // STATS
}

const FPlatformMemoryConstants& FGenericPlatformMemory::GetConstants()
{
	UE_LOG(LogMemory, Warning, TEXT("FGenericPlatformMemory::GetConstants not implemented on this platform"));
	static FPlatformMemoryConstants MemoryConstants;
	return MemoryConstants;
}

uint32 FGenericPlatformMemory::GetPhysicalGBRam()
{
	return FPlatformMemory::GetConstants().TotalPhysicalGB;
}

bool FGenericPlatformMemory::PageProtect(void* const Ptr, const SIZE_T Size, const bool bCanRead, const bool bCanWrite)
{
	UE_LOG(LogMemory, Verbose, TEXT("FGenericPlatformMemory::PageProtect not implemented on this platform"));
	return false;
}

void* FGenericPlatformMemory::BinnedAllocFromOS( SIZE_T Size )
{
	UE_LOG(LogMemory, Error, TEXT("FGenericPlatformMemory::BinnedAllocFromOS not implemented on this platform"));
	return nullptr;
}

void FGenericPlatformMemory::BinnedFreeToOS( void* Ptr, SIZE_T Size )
{
	UE_LOG(LogMemory, Error, TEXT("FGenericPlatformMemory::BinnedFreeToOS not implemented on this platform"));
}

void FGenericPlatformMemory::DumpStats( class FOutputDevice& Ar )
{
	const float InvMB = 1.0f / 1024.0f / 1024.0f;
	FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
#if !NO_LOGGING
	const FName CategoryName(LogMemory.GetCategoryName());
#else
	const FName CategoryName(TEXT("LogMemory"));
#endif
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Platform Memory Stats for %s"), ANSI_TO_TCHAR(FPlatformProperties::PlatformName()));
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Process Physical Memory: %.2f MB used, %.2f MB peak"), MemoryStats.UsedPhysical*InvMB, MemoryStats.PeakUsedPhysical*InvMB);
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Process Virtual Memory: %.2f MB used, %.2f MB peak"), MemoryStats.UsedVirtual*InvMB, MemoryStats.PeakUsedVirtual*InvMB);

	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Physical Memory: %.2f MB used, %.2f MB total"), (MemoryStats.TotalPhysical - MemoryStats.AvailablePhysical)*InvMB, MemoryStats.TotalPhysical*InvMB);
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Virtual Memory: %.2f MB used, %.2f MB total"), (MemoryStats.TotalVirtual - MemoryStats.AvailableVirtual)*InvMB, MemoryStats.TotalVirtual*InvMB);

}

void FGenericPlatformMemory::DumpPlatformAndAllocatorStats( class FOutputDevice& Ar )
{
	FPlatformMemory::DumpStats( Ar );
	GMalloc->DumpAllocatorStats( Ar );
}

void FGenericPlatformMemory::MemswapGreaterThan8( void* RESTRICT Ptr1, void* RESTRICT Ptr2, SIZE_T Size )
{
	union PtrUnion
	{
		void*   PtrVoid;
		uint8*  Ptr8;
		uint16* Ptr16;
		uint32* Ptr32;
		uint64* Ptr64;
		UPTRINT PtrUint;
	};

	PtrUnion Union1 = { Ptr1 };
	PtrUnion Union2 = { Ptr2 };

	checkf(Union1.PtrVoid && Union2.PtrVoid, TEXT("Pointers must be non-null: %p, %p"), Union1.PtrVoid, Union2.PtrVoid);

	// We may skip up to 7 bytes below, so better make sure that we're swapping more than that
	// (8 is a common case that we also want to inline before we this call, so skip that too)
	check(Size > 8);

	if (Union1.PtrUint & 1)
	{
		Valswap(*Union1.Ptr8++, *Union2.Ptr8++);
		Size -= 1;
	}
	if (Union1.PtrUint & 2)
	{
		Valswap(*Union1.Ptr16++, *Union2.Ptr16++);
		Size -= 2;
	}
	if (Union1.PtrUint & 4)
	{
		Valswap(*Union1.Ptr32++, *Union2.Ptr32++);
		Size -= 4;
	}

	uint32 CommonAlignment = FMath::Min(FMath::CountTrailingZeros(Union1.PtrUint - Union2.PtrUint), 3u);
	switch (CommonAlignment)
	{
		default:
			for (; Size >= 8; Size -= 8)
			{
				Valswap(*Union1.Ptr64++, *Union2.Ptr64++);
			}

		case 2:
			for (; Size >= 4; Size -= 4)
			{
				Valswap(*Union1.Ptr32++, *Union2.Ptr32++);
			}

		case 1:
			for (; Size >= 2; Size -= 2)
			{
				Valswap(*Union1.Ptr16++, *Union2.Ptr16++);
			}

		case 0:
			for (; Size >= 1; Size -= 1)
			{
				Valswap(*Union1.Ptr8++, *Union2.Ptr8++);
			}
	}
}

FGenericPlatformMemory::FSharedMemoryRegion::FSharedMemoryRegion(const FString& InName, uint32 InAccessMode, void* InAddress, SIZE_T InSize)
	:	AccessMode(InAccessMode)
	,	Address(InAddress)
	,	Size(InSize)
{
	FCString::Strcpy(Name, sizeof(Name) - 1, *InName);
}

FGenericPlatformMemory::FSharedMemoryRegion * FGenericPlatformMemory::MapNamedSharedMemoryRegion(const FString& Name, bool bCreate, uint32 AccessMode, SIZE_T Size)
{
	UE_LOG(LogHAL, Error, TEXT("FGenericPlatformMemory::MapNamedSharedMemoryRegion not implemented on this platform"));
	return NULL;
}

bool FGenericPlatformMemory::UnmapNamedSharedMemoryRegion(FSharedMemoryRegion * MemoryRegion)
{
	UE_LOG(LogHAL, Error, TEXT("FGenericPlatformMemory::UnmapNamedSharedMemoryRegion not implemented on this platform"));
	return false;
}


void FGenericPlatformMemory::InternalUpdateStats( const FPlatformMemoryStats& MemoryStats )
{
	// Generic method is empty. Implement at platform level.
}
