// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxPlatformMemory.cpp: Linux platform memory functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "MallocAnsi.h"
#include "MallocJemalloc.h"
#include "MallocBinned.h"
#include <sys/sysinfo.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <unistd.h>		// sysconf

void FLinuxPlatformMemory::Init()
{
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	UE_LOG(LogInit, Log, TEXT(" - Physical RAM available (not considering process quota): %d GB (%lu MB, %lu KB, %lu bytes)"), 
		MemoryConstants.TotalPhysicalGB, 
		MemoryConstants.TotalPhysical / ( 1024ULL * 1024ULL ), 
		MemoryConstants.TotalPhysical / 1024ULL, 
		MemoryConstants.TotalPhysical);
}

class FMalloc* FLinuxPlatformMemory::BaseAllocator()
{
	enum EAllocatorToUse
	{
		Ansi,
		Jemalloc,
		Binned
	}
	AllocatorToUse = FORCE_ANSI_ALLOCATOR ? EAllocatorToUse::Ansi : EAllocatorToUse::Binned;

	// we get here before main due to global ctors, so need to do some hackery to get command line args
	if (!FORCE_ANSI_ALLOCATOR)
	{
		if (FILE* CmdLineFile = fopen("/proc/self/cmdline", "r"))
		{
			char CmdLineBuffer[4096] = { 0 };
			FPlatformMemory::Memzero(CmdLineBuffer, sizeof(CmdLineBuffer));
			if (fgets(CmdLineBuffer, sizeof(CmdLineBuffer)-2, CmdLineFile))	// -2 to guarantee that there are always two zeroes even if cmdline is too long
			{
				char * Arg = CmdLineBuffer;
				while (*Arg != 0 || Arg - CmdLineBuffer >= sizeof(CmdLineBuffer))
				{
					if (FCStringAnsi::Stricmp(Arg, "-jemalloc") == 0)
					{
						AllocatorToUse = EAllocatorToUse::Jemalloc;
						break;
					}

					if (FCStringAnsi::Stricmp(Arg, "-ansimalloc") == 0)
					{
						AllocatorToUse = EAllocatorToUse::Ansi;
						break;
					}

					if (FCStringAnsi::Stricmp(Arg, "-binnedmalloc") == 0)
					{
						AllocatorToUse = EAllocatorToUse::Jemalloc;
						break;
					}

					// advance till zero
					while (*Arg)
					{
						++Arg;
					}
					++Arg;	// and skip the zero
				}
			}

			fclose(CmdLineFile);
		}
	}

	FMalloc * Allocator = NULL;

	switch (AllocatorToUse)
	{
		case Ansi:
			Allocator = new FMallocAnsi();
			break;

#if PLATFORM_SUPPORTS_JEMALLOC
		case Jemalloc:
			Allocator = new FMallocJemalloc();
			break;
#endif // PLATFORM_SUPPORTS_JEMALLOC

		default:	// intentional fall-through
		case Binned:
			Allocator = new FMallocBinned(FPlatformMemory::GetConstants().PageSize & MAX_uint32, 0x100000000);
			break;
	}

	printf("Using %ls.\n", Allocator ? Allocator->GetDescriptiveName() : TEXT("NULL allocator! We will probably crash right away"));

	return Allocator;
}

void* FLinuxPlatformMemory::BinnedAllocFromOS( SIZE_T Size )
{	
	return valloc(Size);	// equivalent to memalign(sysconf(_SC_PAGESIZE),size).
}

void FLinuxPlatformMemory::BinnedFreeToOS( void* Ptr )
{
	return free(Ptr);
}

FPlatformMemoryStats FLinuxPlatformMemory::GetStats()
{
	FPlatformMemoryStats MemoryStats;

	// @todo

	return MemoryStats;
}

const FPlatformMemoryConstants& FLinuxPlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if( MemoryConstants.TotalPhysical == 0 )
	{
		// Gather platform memory stats.
		struct sysinfo SysInfo;
		unsigned long long MaxPhysicalRAMBytes = 0;

		if (0 == sysinfo(&SysInfo))
		{
			MaxPhysicalRAMBytes = static_cast< unsigned long long >( SysInfo.mem_unit ) * static_cast< unsigned long long >( SysInfo.totalram );
		}

		MemoryConstants.TotalPhysical = MaxPhysicalRAMBytes;
		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
		MemoryConstants.PageSize = sysconf(_SC_PAGESIZE);
	}

	return MemoryConstants;	
}

FPlatformMemory::FSharedMemoryRegion* FLinuxPlatformMemory::MapNamedSharedMemoryRegion(const FString& InName, bool bCreate, uint32 AccessMode, SIZE_T Size)
{
	// expecting platform-independent name, so convert it to match platform requirements
	FString Name("/");
	Name += InName;
	FTCHARToUTF8 NameUTF8(*Name);

	// correct size to match platform constraints
	FPlatformMemoryConstants MemConstants = FPlatformMemory::GetConstants();
	check(MemConstants.PageSize > 0);	// also relying on it being power of two, which should be true in foreseeable future
	if (Size & (MemConstants.PageSize - 1))
	{
		Size = Size & ~(MemConstants.PageSize - 1);
		Size += MemConstants.PageSize;
	}

	int ShmOpenFlags = bCreate ? O_CREAT : 0;
	// note that you cannot combine O_RDONLY and O_WRONLY to get O_RDWR
	check(AccessMode != 0);
	if (AccessMode == FPlatformMemory::ESharedMemoryAccess::Read)
	{
		ShmOpenFlags |= O_RDONLY;
	}
	else if (AccessMode == FPlatformMemory::ESharedMemoryAccess::Write)
	{
		ShmOpenFlags |= O_WRONLY;
	}
	else if (AccessMode == (FPlatformMemory::ESharedMemoryAccess::Write | FPlatformMemory::ESharedMemoryAccess::Read))
	{
		ShmOpenFlags |= O_RDWR;
	}

	int ShmOpenMode = (S_IRUSR | S_IWUSR) | (S_IRGRP | S_IWGRP) | (S_IROTH | S_IWOTH );	// 0666

	// open the object
	int SharedMemoryFd = shm_open(NameUTF8.Get(), ShmOpenFlags, ShmOpenMode);
	if (SharedMemoryFd == -1)
	{
		int ErrNo = errno;
		UE_LOG(LogHAL, Warning, TEXT("shm_open(name='%s', flags=0x%x, mode=0x%x) failed with errno = %d (%s)"), *Name, ShmOpenFlags, ShmOpenMode, ErrNo, 
			StringCast< TCHAR >(strerror(ErrNo)).Get());
		return NULL;
	}

	// truncate if creating (note that we may still don't have rights to do so)
	if (bCreate)
	{
		int Res = ftruncate(SharedMemoryFd, Size);
		if (Res != 0)
		{
			int ErrNo = errno;
			UE_LOG(LogHAL, Warning, TEXT("ftruncate(fd=%d, size=%d) failed with errno = %d (%s)"), SharedMemoryFd, Size, ErrNo, 
				StringCast< TCHAR >(strerror(ErrNo)).Get());
			shm_unlink(NameUTF8.Get());
			return NULL;
		}
	}

	// map
	int MmapProtFlags = 0;
	if (AccessMode & FPlatformMemory::ESharedMemoryAccess::Read)
	{
		MmapProtFlags |= PROT_READ;
	}

	if (AccessMode & FPlatformMemory::ESharedMemoryAccess::Write)
	{
		MmapProtFlags |= PROT_WRITE;
	}

	void *Ptr = mmap(NULL, Size, MmapProtFlags, MAP_SHARED, SharedMemoryFd, 0);
	if (Ptr == MAP_FAILED)
	{
		int ErrNo = errno;
		UE_LOG(LogHAL, Warning, TEXT("mmap(addr=NULL, length=%d, prot=0x%x, flags=MAP_SHARED, fd=%d, 0) failed with errno = %d (%s)"), Size, MmapProtFlags, SharedMemoryFd, ErrNo, 
			StringCast< TCHAR >(strerror(ErrNo)).Get());

		if (bCreate)
		{
			shm_unlink(NameUTF8.Get());
		}
		return NULL;
	}

	return new FLinuxSharedMemoryRegion(Name, AccessMode, Ptr, Size, SharedMemoryFd, bCreate);
}

bool FLinuxPlatformMemory::UnmapNamedSharedMemoryRegion(FSharedMemoryRegion * MemoryRegion)
{
	bool bAllSucceeded = true;

	if (MemoryRegion)
	{
		FLinuxSharedMemoryRegion * LinuxRegion = static_cast< FLinuxSharedMemoryRegion* >( MemoryRegion );

		if (munmap(LinuxRegion->GetAddress(), LinuxRegion->GetSize()) == -1) 
		{
			bAllSucceeded = false;

			int ErrNo = errno;
			UE_LOG(LogHAL, Warning, TEXT("munmap(addr=%p, len=%d) failed with errno = %d (%s)"), LinuxRegion->GetAddress(), LinuxRegion->GetSize(), ErrNo, 
				StringCast< TCHAR >(strerror(ErrNo)).Get());
		}

		if (close(LinuxRegion->GetFileDescriptor()) == -1)
		{
			bAllSucceeded = false;

			int ErrNo = errno;
			UE_LOG(LogHAL, Warning, TEXT("close(fd=%d) failed with errno = %d (%s)"), LinuxRegion->GetFileDescriptor(), ErrNo, 
				StringCast< TCHAR >(strerror(ErrNo)).Get());
		}

		if (LinuxRegion->NeedsToUnlinkRegion())
		{
			FTCHARToUTF8 NameUTF8(LinuxRegion->GetName());
			if (shm_unlink(NameUTF8.Get()) == -1)
			{
				bAllSucceeded = false;

				int ErrNo = errno;
				UE_LOG(LogHAL, Warning, TEXT("shm_unlink(name='%s') failed with errno = %d (%s)"), LinuxRegion->GetName(), ErrNo, 
					StringCast< TCHAR >(strerror(ErrNo)).Get());
			}
		}

		// delete the region
		delete LinuxRegion;
	}

	return bAllSucceeded;
}
