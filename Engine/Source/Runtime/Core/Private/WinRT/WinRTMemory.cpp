// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WinRTMemory.cpp: WinRT platform memory functions
=============================================================================*/

#include "CorePrivatePCH.h"

void FWinRTPlatformMemory::Init()
{
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	UE_LOG(LogInit, Log, TEXT("Memory total: Physical=%.1fGB (%dGB approx) Pagefile=%.1fGB Virtual=%.1fGB"), 
		float(3),
		MemoryConstants.TotalPhysicalGB, 
		float(0), 
		float(0));
}

const FPlatformMemoryConstants& FWinRTPlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if( MemoryConstants.TotalPhysical == 0 )
	{
		// Gather platform memory stats.

		//@todo.WinRT: Adjust this to final specifications
		MemoryConstants.TotalPhysical = 8 * 1024 * 1024 * 1024;
		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
	}

	return MemoryConstants;	
}
