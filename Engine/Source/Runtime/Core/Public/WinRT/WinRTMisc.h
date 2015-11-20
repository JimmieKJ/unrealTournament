// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	WinRTMisc.h: WinRT platform misc functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/Platform.h"
#include "Misc/Build.h"
#include "WinRT/WinRTSystemIncludes.h"

class FString;

/**
 * WinRT implementation of the misc OS functions
 */
struct CORE_API FWinRTMisc : public FGenericPlatformMisc
{
	static void PlatformPreInit();
	static void PlatformInit();
	static class GenericApplication* CreateApplication();
	static void GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength);

#if !UE_BUILD_SHIPPING
	FORCEINLINE static bool IsDebuggerPresent()
	{
		return !!::IsDebuggerPresent(); 
	}
	FORCEINLINE static void DebugBreak()
	{
		if (IsDebuggerPresent())
		{
			int32* pBreak = NULL;
			*pBreak++;
		}
	}
#endif

	static void PumpMessages(bool bFromMainLoop);
	static uint32 GetKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings);
	static uint32 GetCharKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings);
	static void LowLevelOutputDebugString(const TCHAR *Message);
	static void RequestExit(bool Force);
	static const TCHAR* GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error);
	static void CreateGuid(struct FGuid& Result);
	static int32 NumberOfCores();

	/** Get the application root directory. */
	static const TCHAR* RootDir();
};

typedef FWinRTMisc FPlatformMisc;
