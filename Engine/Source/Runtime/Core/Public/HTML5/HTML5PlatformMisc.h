// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	HTML5Misc.h: HTML5 platform misc functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/Platform.h"
#include "HTML5/HTML5DebugLogging.h"
#include "HTML5/HTML5SystemIncludes.h"
#include <emscripten.h>

/**
 * HTML5 implementation of the misc OS functions
 */
struct CORE_API FHTML5Misc : public FGenericPlatformMisc
{
	static void PlatformInit();
	static void PlatformPostInit(bool ShowSplashScreen = false);
	static class GenericApplication* CreateApplication();
	static uint32 GetKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings);
	static uint32 GetCharKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings);
	static FString GetDefaultLocale();
	static void SetCrashHandler(void (* CrashHandler)(const FGenericCrashContext& Context));
	static EAppReturnType::Type MessageBoxExt( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption );
	FORCEINLINE static int32 NumberOfCores()
	{
		return 1;
	}

	FORCEINLINE static void MemoryBarrier()
	{
		// Do nothing on x86; the spec requires load/store ordering even in the absence of a memory barrier.
		
		// @todo HTML5: Will this be applicable for final?
	}
	/** Return true if a debugger is present */
	FORCEINLINE static bool IsDebuggerPresent()
	{
		return true; 
	}

	/** Break into the debugger, if IsDebuggerPresent returns true, otherwise do nothing  */
	FORCEINLINE static void DebugBreak()
	{
		if (IsDebuggerPresent())
		{
#if PLATFORM_HTML5_WIN32
			__debugbreak();
#else
			emscripten_log(255, "DebugBreak() called!");
			EM_ASM(
				var callstack = new Error;
				throw callstack.stack;
			);
#endif
		}
	}

	/** Break into debugger. Returning false allows this function to be used in conditionals. */
	FORCEINLINE static bool DebugBreakReturningFalse()
	{
#if !UE_BUILD_SHIPPING
		DebugBreak();
#endif
		return false;
	}

	/** Prompts for remote debugging if debugger is not attached. Regardless of result, breaks into debugger afterwards. Returns false for use in conditionals. */
	static FORCEINLINE bool DebugBreakAndPromptForRemoteReturningFalse(bool bIsEnsure = false)
	{
#if !UE_BUILD_SHIPPING
		if (!IsDebuggerPresent())
		{
			PromptForRemoteDebugging(bIsEnsure);
		}

		DebugBreak();
#endif

		return false;
	}

	FORCEINLINE static void LocalPrint( const TCHAR* Str )
	{
		wprintf(TEXT("%ls"), Str);
	}

	static const void PreLoadMap(FString&, FString&, void*);
};

typedef FHTML5Misc FPlatformMisc;
