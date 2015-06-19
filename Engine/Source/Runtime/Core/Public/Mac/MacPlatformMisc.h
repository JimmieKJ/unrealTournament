// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	MacPlatformMisc.h: Mac platform misc functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Mac/MacSystemIncludes.h"

typedef void (*UpdateCachedMacMenuStateProc)(void);

/**
* Mac implementation of the misc OS functions
**/
struct CORE_API FMacPlatformMisc : public FGenericPlatformMisc
{
	static void PlatformPreInit();
	static void PlatformInit();
	static void PlatformPostInit(bool ShowSplashScreen = false);
	static void PlatformTearDown();
	static class GenericApplication* CreateApplication();
	static void GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength);
	static void SetEnvironmentVar(const TCHAR* VariableName, const TCHAR* Value);
	static const TCHAR* GetPathVarDelimiter()
	{
		return TEXT(":");
	}

	static TArray<uint8> GetMacAddress();
#if !UE_BUILD_SHIPPING
	FORCEINLINE static bool IsDebuggerPresent()
	{
		// Based on http://developer.apple.com/library/mac/#qa/qa1361/_index.html

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
		struct kinfo_proc Info = { 0 };
#pragma clang diagnostic pop
		int32 Mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
		SIZE_T Size = sizeof(Info);

		sysctl( Mib, sizeof( Mib ) / sizeof( *Mib ), &Info, &Size, NULL, 0 );

		return ( Info.kp_proc.p_flag & P_TRACED ) != 0;
	}
	FORCEINLINE static void DebugBreak()
	{
		if( IsDebuggerPresent() )
		{
			__asm__ ( "int $3" );
		}
	}
#endif

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

	FORCEINLINE static void MemoryBarrier()
	{
		OSMemoryBarrier();
	}

	static void PumpMessages(bool bFromMainLoop);
	static uint32 GetKeyMap( uint16* KeyCodes, FString* KeyNames, uint32 MaxMappings );
	static uint32 GetCharKeyMap(uint16* KeyCodes, FString* KeyNames, uint32 MaxMappings);
	static void RequestExit(bool Force);
	static void RequestMinimize();
	static const TCHAR* GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error);
	static void ClipboardCopy(const TCHAR* Str);
	static void ClipboardPaste(class FString& Dest);
	static void CreateGuid(struct FGuid& Result);
	static EAppReturnType::Type MessageBoxExt( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption );
	static bool ControlScreensaver(EScreenSaverAction Action);
	static bool CommandLineCommands();
	static int32 NumberOfCores();
	static int32 NumberOfCoresIncludingHyperthreads();
	static void LoadPreInitModules();
	static void NormalizePath(FString& InPath);
	static FString GetPrimaryGPUBrand();
	static void GetOSVersions( FString& out_OSVersionLabel, FString& out_OSSubVersionLabel );


	/** 
	 * Determines the shader format for the plarform
	 *
	 * @return	Returns the shader format to be used by that platform
	 */
	FORCEINLINE static const TCHAR* GetNullRHIShaderFormat() 
	{ 
		return TEXT("GLSL_150"); 
	}

	/**
	 * Sample the displayed pixel color from anywhere on the screen using the OS
	 *
	 * @param	InScreenPos		The screen coords to sample for current pixel color
	 * @param	InGamma			Optional gamma correction to apply to the screen color
	 *
	 * @return					The color of the pixel displayed at the chosen location
	 */
	static struct FLinearColor GetScreenPixelColor(const FVector2D& InScreenPos, float InGamma = 1.0f);

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

	/**
	 * Converts Y position from Slate (left-top origin) to Cocoa (left-bottom origin) coordinates
	 *
	 * @param	YPosition		Position to convert
	 *
	 * @return					Converted position
	 */
    static int32 ConvertSlateYPositionToCocoa(int32 YPosition);
    
    /**
     * Converts Y position from Cocoa (left-bottom origin) to Slate (left-top origin) coordinates
     *
     * @param	YPosition		Position to convert
     *
     * @return					Converted position
     */
    static int32 ConvertCocoaYPositionToSlate(int32 YPosition);
	
	static void SetGracefulTerminationHandler();
	
	static void SetCrashHandler(void (* CrashHandler)(const FGenericCrashContext& Context));

	static FString GetDefaultLocale();

	/** @return Get the name of the platform specific file manager (Finder) */
	static FText GetFileManagerName();

	static void UpdateWindowMenu();

	static UpdateCachedMacMenuStateProc UpdateCachedMacMenuState;

	static void ActivateApplication();

	/**
	 * Returns whether the platform is running on battery power or not.
	 */
	static bool IsRunningOnBattery();

	/**
	 * Returns whether the Mac OS X version is 10.9.x or not.
	 */
	static bool IsRunningOnMavericks();

	/**
	 * Returns if current < target returns 1, if current > target returns 1, else current == target and returns 0.
	 */
	static int32 MacOSXVersionCompare(uint8 Major, uint8 Minor, uint8 Revision);

	/**
	 * Gets a globally unique ID the represents a particular operating system install.
	 */
	static FString GetOperatingSystemId();

	static bool bChachedMacMenuStateNeedsUpdate;

	static id<NSObject> CommandletActivity;
    
    static void MergeDefaultArgumentsIntoCommandLine(FString& CommandLine, FString DefaultArguments);
};

#ifdef __OBJC__

class FScopeAutoreleasePool
{
public:

	FScopeAutoreleasePool()
	{
		Pool = [[NSAutoreleasePool alloc] init];
	}

	~FScopeAutoreleasePool()
	{
		[Pool release];
	}

private:

	NSAutoreleasePool*	Pool;
};

#define SCOPED_AUTORELEASE_POOL const FScopeAutoreleasePool PREPROCESSOR_JOIN(Pool,__LINE__);

#endif // __OBJC__

typedef FMacPlatformMisc FPlatformMisc;

enum EMacModifierKeys
{
	MMK_RightCommand	= 0xF754,
	MMK_LeftCommand		= 0xF755,
	MMK_LeftShift		= 0xF756,
	MMK_CapsLock		= 0xF757,
	MMK_LeftAlt			= 0xF758,
	MMK_LeftControl		= 0xF759,
	MMK_RightShift		= 0xF760,
	MMK_RightAlt		= 0xF761,
	MMK_RightControl	= 0xF762
};
