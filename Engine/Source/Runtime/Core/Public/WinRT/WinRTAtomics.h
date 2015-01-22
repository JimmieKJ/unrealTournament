// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	WinRTAtomics.h: WinRT platform Atomics functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformAtomics.h"
#include "WinRT/WinRTSystemIncludes.h"

/**
 * WinRT implementation of the Atomics OS functions
 */
struct CORE_API FWinRTAtomics : public FGenericPlatformAtomics
{
	/**
	 * Atomically increments the value pointed to and returns that to the caller
	 */
	static FORCEINLINE int32 InterlockedIncrement(volatile int32* Value)
	{
		return (int32)::InterlockedIncrement((LPLONG)Value);
	}
	
	/**
	 * Atomically increments the value pointed to and returns that to the caller
	 */
	static FORCEINLINE int64 InterlockedIncrement (volatile int64* Value)
	{
		return (int64)::InterlockedIncrement64((LONGLONG*)Value);
	}

	/**
	 * Atomically decrements the value pointed to and returns that to the caller
	 */
	static FORCEINLINE int32 InterlockedDecrement(volatile int32* Value)
	{
		return (int32)::InterlockedDecrement((LPLONG)Value);
	}

	/**
	 * Atomically decrements the value pointed to and returns that to the caller
	 */
	static FORCEINLINE int64 InterlockedDecrement (volatile int64* Value)
	{
		return (int64)::InterlockedDecrement64((LONGLONG*)Value);
	}

	/**
	 * Atomically adds the amount to the value pointed to and returns the old
	 * value to the caller
	 */
	static FORCEINLINE int32 InterlockedAdd(volatile int32* Value,int32 Amount)
	{
		return (int32)::InterlockedExchangeAdd((LPLONG)Value, (LONG)Amount);
	}

	/**
	 * Atomically adds the amount to the value pointed to and returns the old
	 * value to the caller
	 */
	static FORCEINLINE int64 InterlockedAdd (volatile int64* Value, int64 Amount)
	{
		return (int64)::InterlockedExchangeAdd64((LONGLONG*)Value, (LONGLONG)Amount);
	}

	/**
	 * Atomically swaps two values returning the original value to the caller
	 */
	static FORCEINLINE int32 InterlockedExchange(volatile int32* Value,int32 Exchange)
	{
		return (int32)::InterlockedExchange((LPLONG)Value, (LONG)Exchange);
	}

	/**
	 * Atomically swaps two values returning the original value to the caller
	 */
	static FORCEINLINE int64 InterlockedExchange (volatile int64* Value, int64 Exchange)
	{
		return (int64)::InterlockedExchange64((LONGLONG*)Value, (LONGLONG)Exchange);
	}

	static FORCEINLINE void* InterlockedExchangePtr( void** Dest, void* Exchange )
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (IsAligned(Dest) == false)
		{
			HandleAtomicsFailure(TEXT("InterlockedExchangePointer requires Dest pointer to be aligned to %d bytes"), sizeof(void*));
		}
#endif

		return ::InterlockedExchangePointer(Dest, Exchange);
	}

	/**
	 * Atomically compares the value to comparand and replaces with the exchange
	 * value if they are equal and returns the original value
	 */
	static FORCEINLINE int32 InterlockedCompareExchange(volatile int32* Dest,int32 Exchange,int32 Comparand)
	{
		return (int32)::InterlockedCompareExchange((LPLONG)Dest,(LONG)Exchange,(LONG)Comparand);
	}

#if PLATFORM_64BITS
	/**
	 * Atomically compares the value to comparand and replaces with the exchange
	 * value if they are equal and returns the original value
	 */
	static FORCEINLINE int64 InterlockedCompareExchange (volatile int64* Dest, int64 Exchange, int64 Comparand)
	{
		return (int64)::InterlockedCompareExchange64((LONGLONG*)Dest, (LONGLONG)Exchange, (LONGLONG)Comparand);
	}
#endif

	/**
	 * Atomically compares the pointer to comparand and replaces with the exchange
	 * pointer if they are equal and returns the original value
	 */
	static FORCEINLINE void* InterlockedCompareExchangePointer(void** Dest,void* Exchange,void* Comparand)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Dest needs to be aligned otherwise the function will behave unpredictably 
		if (IsAligned( Dest ) == false)
		{
			HandleAtomicsFailure( TEXT( "InterlockedCompareExchangePointer requires Dest pointer to be aligned to %d bytes" ), sizeof(void*) );
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		return ::InterlockedCompareExchangePointer(Dest,Exchange,Comparand);
	}

private:
	/** Checks if a pointer is aligned and can be used with atomic functions */
	static inline bool IsAligned( const void* Ptr )
	{
		return (((DWORD64)Ptr) & (sizeof(void*) - 1)) == 0 ? true : false;
	}

	/** Handles atomics function failure. Since 'check' has not yet been declared here we need to call external function to use it. */
	static void HandleAtomicsFailure( const TCHAR* InFormat, ... );
};

typedef FWinRTAtomics FPlatformAtomics;
