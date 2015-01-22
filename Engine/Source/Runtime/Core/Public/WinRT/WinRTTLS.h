// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	WinRTTLS.h: WinRT platform TLS (Thread local storage and thread ID) functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformTLS.h"
#include "WinRT/WinRTSystemIncludes.h"
//@todo.WINRT: Do NOT use the ThreadEmulation if this is ever checked in!!!!
// Or if we do, it must have a TPS filed.
// (Legal approved temporary usage for my Free Friday test)
#include "../Private/WinRT/ThreadEmulation.h"

/**
 * WinRT implementation of the TLS OS functions
 */
struct CORE_API FWinRTTLS : public FGenericPlatformTLS
{
	/**
	 * Returns the currently executing thread's id
	 */
	static FORCEINLINE uint32 GetCurrentThreadId(void)
	{
		return ::GetCurrentThreadId();
	}

	/**
	 * Allocates a thread local store slot
	 */
	static FORCEINLINE uint32 AllocTlsSlot(void)
	{
		return ThreadEmulation::TlsAlloc();
	}

	/**
	 * Sets a value in the specified TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 * @param Value the value to store in the slot
	 */
	static FORCEINLINE void SetTlsValue(uint32 SlotIndex,void* Value)
	{
		ThreadEmulation::TlsSetValue(SlotIndex,Value);
	}

	/**
	 * Reads the value stored at the specified TLS slot
	 *
	 * @return the value stored in the slot
	 */
	static FORCEINLINE void* GetTlsValue(uint32 SlotIndex)
	{
		return ThreadEmulation::TlsGetValue(SlotIndex);
	}

	/**
	 * Frees a previously allocated TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 */
	static FORCEINLINE void FreeTlsSlot(uint32 SlotIndex)
	{
		ThreadEmulation::TlsFree(SlotIndex);
	}
};

typedef FWinRTTLS FPlatformTLS;
