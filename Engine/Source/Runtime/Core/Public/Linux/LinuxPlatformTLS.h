// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	LinuxPlatformTLS.h: Linux platform TLS (Thread local storage and thread ID) functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformTLS.h"
#include "Linux/LinuxSystemIncludes.h"

#if defined(_GNU_SOURCE)
	#include <sys/syscall.h>	// SYS_gettid
#endif // _GNU_SOURCE

/**
 * Linux implementation of the TLS OS functions
 */
struct CORE_API FLinuxTLS : public FGenericPlatformTLS
{
	/**
	 * Returns the currently executing thread's id
	 */
	static FORCEINLINE uint32 GetCurrentThreadId(void)
	{
		// note: cannot use pthread_self() without updating the rest of API to opaque (or at least 64-bit) thread handles
#if defined(_GNU_SOURCE)
		// syscall() is relatively heavy and shows up in the profiler, given that IsInGameThread() is used quite often. Cache thread id in TLS.
		
	// note: this caching currently breaks the editor on Ubuntu 14.04 and earlier. The libraries are using static TLS and glibc 2.19 runs out of limit for TLS slots (see https://bugs.launchpad.net/ubuntu/+source/glibc/+bug/1375555 for a similar problem).
	// the proper fix is perhaps not to use initial-exec TLS model, but for some reason just passing -ftls-model=local-dynamic doesn't work. Disable this caching until the issue is investigated (tracked as UE-11541)
	#if defined(__GLIBC__) && (__GLIBC__ == 2) && (__GLIBC_MINOR__ <= 19)

		pid_t ThreadId = static_cast<pid_t>(syscall(SYS_gettid));
		static_assert(sizeof(pid_t) <= sizeof(uint32), "pid_t is larger than uint32, reconsider implementation of GetCurrentThreadId()");
		return static_cast<pid_t>(ThreadId);		

	#else

		// cached version - should work fine in future glibc versions (also works fine in Ubuntu 14.10 after glibc 2.19-10ubuntu2, but there's no appropriate #define to discover that)
		static __thread uint32 ThreadIdTLS = 0;
		if (ThreadIdTLS == 0)
		{
			pid_t ThreadId = static_cast<pid_t>(syscall(SYS_gettid));
			static_assert(sizeof(pid_t) <= sizeof(uint32), "pid_t is larger than uint32, reconsider implementation of GetCurrentThreadId()");
			ThreadIdTLS = static_cast<uint32>(ThreadId);
			checkf(ThreadIdTLS != 0, TEXT("ThreadId is 0 - reconsider implementation of GetCurrentThreadId() (syscall changed?)"));
		}
		return ThreadIdTLS;

	#endif // glibc version check

#else
		// better than nothing...
		return static_cast< uint32 >(pthread_self());
#endif
	}

	/**
	 * Allocates a thread local store slot
	 */
	static FORCEINLINE uint32 AllocTlsSlot(void)
	{
		// allocate a per-thread mem slot
		pthread_key_t Key = 0;
		if (pthread_key_create(&Key, NULL) != 0)
		{
			Key = 0xFFFFFFFF;  // matches the Windows TlsAlloc() retval //@todo android: should probably check for this below, or assert out instead
		}
		return Key;
	}

	/**
	 * Sets a value in the specified TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 * @param Value the value to store in the slot
	 */
	static FORCEINLINE void SetTlsValue(uint32 SlotIndex,void* Value)
	{
		pthread_setspecific((pthread_key_t)SlotIndex, Value);
	}

	/**
	 * Reads the value stored at the specified TLS slot
	 *
	 * @return the value stored in the slot
	 */
	static FORCEINLINE void* GetTlsValue(uint32 SlotIndex)
	{
		return pthread_getspecific((pthread_key_t)SlotIndex);
	}

	/**
	 * Frees a previously allocated TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 */
	static FORCEINLINE void FreeTlsSlot(uint32 SlotIndex)
	{
		pthread_key_delete((pthread_key_t)SlotIndex);
	}
};

typedef FLinuxTLS FPlatformTLS;
