// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	ApplePlatformSymbolication.h: Apple platform implementation of symbolication
==============================================================================================*/

#pragma once

#include "ApplePlatformStackWalk.h"

/**
 * Opaque symbol cache for improved symbolisation performance.
 */
struct FApplePlatformSymbolCache;

/**
 * Apple platform implementation of symbolication - not async. handler safe, so don't call during crash reporting!
 */
struct CORE_API FApplePlatformSymbolication
{
	static void SetSymbolicationAllowed(bool const bAllow);
	static bool SymbolInfoForAddress(uint64 ProgramCounter, FProgramCounterSymbolInfo& Info);
	static bool SymbolInfoForFunctionFromModule(ANSICHAR const* MangledName, ANSICHAR const* ModuleName, FProgramCounterSymbolInfo& Info);
	
	static FApplePlatformSymbolCache* CreateSymbolCache(void);
	static bool SymbolInfoForStrippedSymbol(FApplePlatformSymbolCache* Cache, uint64 ProgramCounter, ANSICHAR const* ModulePath, ANSICHAR const* ModuleUUID, FProgramCounterSymbolInfo& Info);
	static void DestroySymbolCache(FApplePlatformSymbolCache* Cache);
};
