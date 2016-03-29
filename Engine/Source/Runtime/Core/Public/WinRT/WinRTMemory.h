// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	WinRTPlatformMemory.h: WinRT platform memory functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformMemory.h"

/**
 *	WinRT implementation of the FGenericPlatformMemoryStats.
 */
struct FPlatformMemoryStats : public FGenericPlatformMemoryStats
{};

/**
* WinRT implementation of the memory OS functions
**/
struct CORE_API FWinRTPlatformMemory : public FGenericPlatformMemory
{
	//~ Begin FGenericPlatformMemory Interface
	static void Init();
	static const FPlatformMemoryConstants& GetConstants();
	//~ End FGenericPlatformMemory Interface
};

typedef FWinRTPlatformMemory FPlatformMemory;



