// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "MallocBinned.h"
#include "MallocAnsi.h"
#include "unistd.h"
#include <jni.h>
#include <sys/mman.h>

#define JNI_CURRENT_VERSION JNI_VERSION_1_6
extern JavaVM* GJavaVM;

static int64 GetNativeHeapAllocatedSize()
{
	int64 AllocatedSize = 0;
#if 0 // TODO: this works but sometimes crashes?
	JNIEnv* Env = NULL;
	GJavaVM->GetEnv((void **)&Env, JNI_CURRENT_VERSION);
	jint AttachThreadResult = GJavaVM->AttachCurrentThread(&Env, NULL);

	if(AttachThreadResult != JNI_ERR)
	{
		jclass Class = Env->FindClass("android/os/Debug");
		if (Class)
		{
			jmethodID MethodID = Env->GetStaticMethodID(Class, "getNativeHeapAllocatedSize", "()J");
			if (MethodID)
			{
				AllocatedSize = Env->CallStaticLongMethod(Class, MethodID);
			}
		}
	}
#endif
	return AllocatedSize;
}

void FAndroidPlatformMemory::Init()
{
	FGenericPlatformMemory::Init();

	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	FPlatformMemoryStats MemoryStats = GetStats();
	UE_LOG(LogInit, Log, TEXT("Memory total: Physical=%.2fMB (%dGB approx) Available=%.2fMB PageSize=%.1fKB"), 
		float(MemoryConstants.TotalPhysical/1024.0/1024.0),
		MemoryConstants.TotalPhysicalGB, 
		float(MemoryStats.AvailablePhysical/1024.0/1024.0),
		float(MemoryConstants.PageSize/1024.0)
		);
}

FPlatformMemoryStats FAndroidPlatformMemory::GetStats()
{
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();

	FPlatformMemoryStats MemoryStats;

	//int32 NumAvailPhysPages = sysconf(_SC_AVPHYS_PAGES);
	//MemoryStats.AvailablePhysical = NumAvailPhysPages * MemoryConstants.PageSize;

	MemoryStats.AvailablePhysical = MemoryConstants.TotalPhysical - GetNativeHeapAllocatedSize();
	MemoryStats.AvailableVirtual = 0;
	MemoryStats.UsedPhysical = 0;
	MemoryStats.UsedVirtual = 0;

	return MemoryStats;
}

const FPlatformMemoryConstants& FAndroidPlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if( MemoryConstants.TotalPhysical == 0 )
	{
		int32 NumPhysPages = sysconf(_SC_PHYS_PAGES);
		int32 PageSize = sysconf(_SC_PAGESIZE);

		MemoryConstants.TotalPhysical = NumPhysPages * PageSize;
		MemoryConstants.TotalVirtual = 0;
		MemoryConstants.PageSize = (uint32)PageSize;

		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
	}

	return MemoryConstants;	
}

FMalloc* FAndroidPlatformMemory::BaseAllocator()
{
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	// 1 << FMath::CeilLogTwo(MemoryConstants.TotalPhysical) should really be FMath::RoundUpToPowerOfTwo,
	// but that overflows to 0 when MemoryConstants.TotalPhysical is close to 4GB, since CeilLogTwo returns 32
	// this then causes the MemoryLimit to be 0 and crashing the app
	uint64 MemoryLimit = FMath::Min<uint64>( uint64(1) << FMath::CeilLogTwo(MemoryConstants.TotalPhysical), 0x100000000);

#if PLATFORM_ANDROID_ARM64
	// todo: track down why FMallocBinned is failing on ARM64
	return new FMallocAnsi();
#else
	return new FMallocBinned(MemoryConstants.PageSize, MemoryLimit);
#endif
}

void* FAndroidPlatformMemory::BinnedAllocFromOS(SIZE_T Size)
{
	return mmap(nullptr, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

void FAndroidPlatformMemory::BinnedFreeToOS(void* Ptr, SIZE_T Size)
{
	if (munmap(Ptr, Size) != 0)
	{
		const int ErrNo = errno;
		UE_LOG(LogHAL, Fatal, TEXT("munmap(addr=%p, len=%llu) failed with errno = %d (%s)"), Ptr, Size,
			ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
	}
}
