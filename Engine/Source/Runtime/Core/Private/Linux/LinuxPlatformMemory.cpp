// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	FGenericPlatformMemory::Init();

	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	UE_LOG(LogInit, Log, TEXT(" - Physical RAM available (not considering process quota): %d GB (%lu MB, %lu KB, %lu bytes)"), 
		MemoryConstants.TotalPhysicalGB, 
		MemoryConstants.TotalPhysical / ( 1024ULL * 1024ULL ), 
		MemoryConstants.TotalPhysical / 1024ULL, 
		MemoryConstants.TotalPhysical);
}

class FMalloc* FLinuxPlatformMemory::BaseAllocator()
{
	// This function gets executed very early, way before main() (because global constructors will allocate memory).
	// This makes it ideal, if unobvious, place for a root privilege check.
	if (geteuid() == 0)
	{
		fprintf(stderr, "Refusing to run with the root privileges.\n");
		FPlatformMisc::RequestExit(true);
		// unreachable
		return nullptr;
	}

	enum EAllocatorToUse
	{
		Ansi,
		Jemalloc,
		Binned
	}
	AllocatorToUse = EAllocatorToUse::Binned;

	// Prefer jemalloc as it consistently saves ~20% RES usage in my (RCL) tests (editor only)
	if (PLATFORM_SUPPORTS_JEMALLOC && WITH_EDITOR)
	{
		AllocatorToUse = EAllocatorToUse::Jemalloc;
	}

	if (FORCE_ANSI_ALLOCATOR)
	{
		AllocatorToUse = EAllocatorToUse::Ansi;
	}
	else
	{
		// Allow overriding on the command line.
		// We get here before main due to global ctors, so need to do some hackery to get command line args
		if (FILE* CmdLineFile = fopen("/proc/self/cmdline", "r"))
		{
			char * Arg = nullptr;
			size_t Size = 0;
			while(getdelim(&Arg, &Size, 0, CmdLineFile) != -1)
			{
#if PLATFORM_SUPPORTS_JEMALLOC
				if (FCStringAnsi::Stricmp(Arg, "-jemalloc") == 0)
				{
					AllocatorToUse = EAllocatorToUse::Jemalloc;
					break;
				}
#endif // PLATFORM_SUPPORTS_JEMALLOC
				if (FCStringAnsi::Stricmp(Arg, "-ansimalloc") == 0)
				{
					AllocatorToUse = EAllocatorToUse::Ansi;
					break;
				}

				if (FCStringAnsi::Stricmp(Arg, "-binnedmalloc") == 0)
				{
					AllocatorToUse = EAllocatorToUse::Binned;
					break;
				}	
			}
			free(Arg);
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

bool FLinuxPlatformMemory::PageProtect(void* const Ptr, const SIZE_T Size, const bool bCanRead, const bool bCanWrite)
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

void* FLinuxPlatformMemory::BinnedAllocFromOS( SIZE_T Size )
{	
	return mmap(nullptr, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

void FLinuxPlatformMemory::BinnedFreeToOS( void* Ptr, SIZE_T Size )
{
	if (munmap(Ptr, Size) != 0)
	{
		const int ErrNo = errno;
		UE_LOG(LogHAL, Fatal, TEXT("munmap(addr=%p, len=%llu) failed with errno = %d (%s)"), Ptr, Size,
			ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
	}
}

namespace LinuxPlatformMemory
{
	/**
	 * @brief Returns value in bytes from a status line
	 * @param Line in format "Blah:  10000 kB" - needs to be writable as it will modify it
	 * @return value in bytes (10240000, i.e. 10000 * 1024 for the above example)
	 */
	uint64 GetBytesFromStatusLine(char * Line)
	{
		check(Line);
		int Len = strlen(Line);

		// Len should be long enough to hold at least " kB\n"
		const int kSuffixLength = 4;	// " kB\n"
		if (Len <= kSuffixLength)
		{
			return 0;
		}

		// let's check that this is indeed "kB"
		char * Suffix = &Line[Len - kSuffixLength];
		if (strcmp(Suffix, " kB\n") != 0)
		{
			// Linux the kernel changed the format, huh?
			return 0;
		}

		// kill the kB
		*Suffix = 0;

		// find the beginning of the number
		for (const char * NumberBegin = Suffix; NumberBegin >= Line; --NumberBegin)
		{
			if (*NumberBegin == ' ')
			{
				return static_cast< uint64 >(atol(NumberBegin + 1)) * 1024ULL;
			}
		}

		// we were unable to find whitespace in front of the number
		return 0;
	}
}

FPlatformMemoryStats FLinuxPlatformMemory::GetStats()
{
	FPlatformMemoryStats MemoryStats;	// will init from constants

	// open to all kind of overflows, thanks to Linux approach of exposing system stats via /proc and lack of proper C API
	// And no, sysinfo() isn't useful for this (cannot get the same value for MemAvailable through it for example).

	if (FILE* FileGlobalMemStats = fopen("/proc/meminfo", "r"))
	{
		int FieldsSetSuccessfully = 0;
		SIZE_T MemFree = 0, Cached = 0;
		do
		{
			char LineBuffer[256] = {0};
			char *Line = fgets(LineBuffer, ARRAY_COUNT(LineBuffer), FileGlobalMemStats);
			if (Line == nullptr)
			{
				break;	// eof or an error
			}

			// if we have MemAvailable, favor that (see http://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=34e431b0ae398fc54ea69ff85ec700722c9da773)
			if (strstr(Line, "MemAvailable:") == Line)
			{
				MemoryStats.AvailablePhysical = LinuxPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "SwapFree:") == Line)
			{
				MemoryStats.AvailableVirtual = LinuxPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "MemFree:") == Line)
			{
				MemFree = LinuxPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "Cached:") == Line)
			{
				Cached = LinuxPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
		}
		while(FieldsSetSuccessfully < 4);

		// if we didn't have MemAvailable (kernels < 3.14 or CentOS 6.x), use free + cached as a (bad) approximation
		if (MemoryStats.AvailablePhysical == 0)
		{
			MemoryStats.AvailablePhysical = FMath::Min(MemFree + Cached, MemoryStats.TotalPhysical);
		}

		fclose(FileGlobalMemStats);
	}

	// again /proc "API" :/
	if (FILE* ProcMemStats = fopen("/proc/self/status", "r"))
	{
		int FieldsSetSuccessfully = 0;
		do
		{
			char LineBuffer[256] = {0};
			char *Line = fgets(LineBuffer, ARRAY_COUNT(LineBuffer), ProcMemStats);
			if (Line == nullptr)
			{
				break;	// eof or an error
			}

			if (strstr(Line, "VmPeak:") == Line)
			{
				MemoryStats.PeakUsedVirtual = LinuxPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "VmSize:") == Line)
			{
				MemoryStats.UsedVirtual = LinuxPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "VmHWM:") == Line)
			{
				MemoryStats.PeakUsedPhysical = LinuxPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "VmRSS:") == Line)
			{
				MemoryStats.UsedPhysical = LinuxPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
		}
		while(FieldsSetSuccessfully < 4);

		fclose(ProcMemStats);
	}

	// sanitize stats as sometimes peak < used for some reason
	MemoryStats.PeakUsedVirtual = FMath::Max(MemoryStats.PeakUsedVirtual, MemoryStats.UsedVirtual);
	MemoryStats.PeakUsedPhysical = FMath::Max(MemoryStats.PeakUsedPhysical, MemoryStats.UsedPhysical);

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
		unsigned long long MaxVirtualRAMBytes = 0;

		if (0 == sysinfo(&SysInfo))
		{
			MaxPhysicalRAMBytes = static_cast< unsigned long long >( SysInfo.mem_unit ) * static_cast< unsigned long long >( SysInfo.totalram );
			MaxVirtualRAMBytes = static_cast< unsigned long long >( SysInfo.mem_unit ) * static_cast< unsigned long long >( SysInfo.totalswap );
		}

		MemoryConstants.TotalPhysical = MaxPhysicalRAMBytes;
		MemoryConstants.TotalVirtual = MaxVirtualRAMBytes;
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
