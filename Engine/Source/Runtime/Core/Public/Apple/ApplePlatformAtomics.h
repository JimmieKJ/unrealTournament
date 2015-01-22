// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	ApplePlatformAtomics.h: Apple platform Atomics functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformAtomics.h"
#include "HAL/Platform.h"
#if PLATFORM_MAC
#include "Mac/MacSystemIncludes.h"
#elif PLATFORM_IOS
#include "IOS/IOSSystemIncludes.h"
#endif

/**
 * Apple implementation of the Atomics OS functions
 **/
struct CORE_API FApplePlatformAtomics : public FGenericPlatformAtomics
{
	static FORCEINLINE int32 InterlockedIncrement( volatile int32* Value )
	{
		return (int32)OSAtomicIncrement32Barrier((int32_t*)Value);
	}

	static FORCEINLINE int64 InterlockedIncrement( volatile int64* Value )
	{
		return (int64)OSAtomicIncrement64Barrier((int64_t*)Value);
	}

	static FORCEINLINE int32 InterlockedDecrement( volatile int32* Value )
	{
		return (int32)OSAtomicDecrement32Barrier((int32_t*)Value);
	}

	static FORCEINLINE int64 InterlockedDecrement( volatile int64* Value )
	{
		return (int64)OSAtomicDecrement64Barrier((int64_t*)Value);
	}

	static FORCEINLINE int32 InterlockedAdd( volatile int32* Value, int32 Amount )
	{
		return OSAtomicAdd32Barrier((int32_t)Amount, (int32_t*)Value) - Amount;
	}

	static FORCEINLINE int64 InterlockedAdd( volatile int64* Value, int64 Amount )
	{
		return OSAtomicAdd64Barrier((int64_t)Amount, (int64_t*)Value) - Amount;
	}

	static FORCEINLINE int32 InterlockedExchange( volatile int32* Value, int32 Exchange )
	{
		int32 RetVal;

		do
		{
			RetVal = *Value;
		}
		while (!OSAtomicCompareAndSwap32Barrier(RetVal, Exchange, (int32_t*) Value));

		return RetVal;
	}

	static FORCEINLINE int64 InterlockedExchange( volatile int64* Value, int64 Exchange )
	{
		int64 RetVal;

		do
		{
			RetVal = *Value;
		}
		while (!OSAtomicCompareAndSwap64Barrier(RetVal, Exchange, (int64_t*) Value));

		return RetVal;
	}

	static FORCEINLINE void* InterlockedExchangePtr( void** Dest, void* Exchange )
	{
		void* RetVal;

		do
		{
			RetVal = *Dest;
		}
		while (!OSAtomicCompareAndSwapPtrBarrier(RetVal, Exchange, Dest));

		return RetVal;
	}

	static FORCEINLINE int32 InterlockedCompareExchange(volatile int32* Dest, int32 Exchange, int32 Comparand)
	{
		int32 RetVal;

		do
		{
			if (OSAtomicCompareAndSwap32Barrier(Comparand, Exchange, (int32_t*) Dest))
			{
				return Comparand;
			}
			RetVal = *Dest;
		}
		while (RetVal == Comparand);

		return RetVal;
	}

#if PLATFORM_64BITS
	static FORCEINLINE int64 InterlockedCompareExchange( volatile int64* Dest, int64 Exchange, int64 Comparand )
	{
		int64 RetVal;

		do
		{
			if (OSAtomicCompareAndSwap64Barrier(Comparand, Exchange, (int64_t*) Dest))
			{
				return Comparand;
			}
			RetVal = *Dest;
		}
		while (RetVal == Comparand);

		return RetVal;
	}
#endif

	static FORCEINLINE void* InterlockedCompareExchangePointer( void** Dest, void* Exchange, void* Comparand )
	{
		void *RetVal;

		do
		{
			if (OSAtomicCompareAndSwapPtrBarrier(Comparand, Exchange, Dest))
			{
				return Comparand;
			}
			RetVal = *Dest;
		}
		while (RetVal == Comparand);

		return RetVal;
	}
};

typedef FApplePlatformAtomics FPlatformAtomics;

