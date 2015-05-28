// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	LinuxPlatformMisc.h: Linux platform misc functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Linux/LinuxSystemIncludes.h"
#include "Misc/Build.h"

class FString;
struct FGenericCrashContext;

/**
 * Linux implementation of the misc OS functions
 */
struct CORE_API FLinuxPlatformMisc : public FGenericPlatformMisc
{
	static void PlatformInit();
	static void PlatformTearDown();
	static void SetGracefulTerminationHandler();
	static void SetCrashHandler(void (* CrashHandler)(const FGenericCrashContext& Context));
	static class GenericApplication* CreateApplication();
	static void GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength);
	static void SetEnvironmentVar(const TCHAR* VariableName, const TCHAR* Value);
#if !UE_BUILD_SHIPPING
	static bool IsDebuggerPresent();
	FORCEINLINE static void DebugBreak()
	{
		if( IsDebuggerPresent() )
		{
			raise(SIGTRAP);
		}
	}
#endif // !UE_BUILD_SHIPPING

	/** Break into debugger. Returning false allows this function to be used in conditionals. */
	FORCEINLINE static bool DebugBreakReturningFalse()
	{
#if !UE_BUILD_SHIPPING
		DebugBreak();
#endif
		return false;
	}

	/** Prompts for remote debugging if debugger is not attached. Regardless of result, breaks into debugger afterwards. Returns false for use in conditionals. */
	static FORCEINLINE bool DebugBreakAndPromptForRemoteReturningFalse()
	{
#if !UE_BUILD_SHIPPING
		if (!IsDebuggerPresent())
		{
			PromptForRemoteDebugging(false);
		}

		DebugBreak();
#endif

		return false;
	}

	static void PumpMessages(bool bFromMainLoop);
	static uint32 GetKeyMap( uint16* KeyCodes, FString* KeyNames, uint32 MaxMappings );
	static uint32 GetCharKeyMap(uint16* KeyCodes, FString* KeyNames, uint32 MaxMappings);
	static void LowLevelOutputDebugString(const TCHAR *Message);
	static bool ControlScreensaver(EScreenSaverAction Action);

	static void ClipboardCopy(const TCHAR* Str);
	static void ClipboardPaste(class FString& Dest);

	static const TCHAR* RootDir();
	static void NormalizePath(FString& InPath);

	static const TCHAR* GetPathVarDelimiter()
	{
		return TEXT(":");
	}

	static EAppReturnType::Type MessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption);

	FORCEINLINE static void MemoryBarrier()
	{
		__sync_synchronize();
	}

	static int32 NumberOfCores();
	static int32 NumberOfCoresIncludingHyperthreads();
	static void LoadPreInitModules();
	static void LoadStartupModules();
	static FString GetOperatingSystemId();

	/**
	 * Determines the shader format for the platform
	 *
	 * @return	Returns the shader format to be used by that platform
	 */
	static const TCHAR* GetNullRHIShaderFormat();

#if PLATFORM_HAS_CPUID
	/**
	 * Uses cpuid instruction to get the vendor string
	 *
	 * @return	CPU vendor name
	 */
	static FString GetCPUVendor();

	/**
	 * Uses cpuid instruction to get the vendor string
	 *
	 * @return	CPU info bitfield
	 *
	 *			Bits 0-3	Stepping ID
	 *			Bits 4-7	Model
	 *			Bits 8-11	Family
	 *			Bits 12-13	Processor type (Intel) / Reserved (AMD)
	 *			Bits 14-15	Reserved
	 *			Bits 16-19	Extended model
	 *			Bits 20-27	Extended family
	 *			Bits 28-31	Reserved
	 */
	static uint32 GetCPUInfo();
#endif // PLATFORM_HAS_CPUID

	static const TCHAR* EngineDir()
	{
		return TEXT("../../../Engine/");
	}

	/**
	 * Initializes video (and not only) subsystem.
	 */
	static bool PlatformInitMultimedia();

	/**
	 * Returns whether the program has been started remotely (e.g. over SSH)
	 */
	static bool HasBeenStartedRemotely();
};

typedef FLinuxPlatformMisc FPlatformMisc;
