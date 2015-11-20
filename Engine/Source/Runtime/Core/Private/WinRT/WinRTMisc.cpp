// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WinRTMisc.cpp: WinRT implementations of misc functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "ExceptionHandling.h"
#include "SecureHash.h"
#include <time.h>
#include "WinRTApplication.h"

/** 
 * Whether support for integrating into the firewall is there
 */
#define WITH_FIREWALL_SUPPORT	0

/** Original C- Runtime pure virtual call handler that is being called in the (highly likely) case of a double fault */
_purecall_handler DefaultPureCallHandler;

/**
 * Our own pure virtual function call handler, set by appPlatformPreInit. Falls back
 * to using the default C- Runtime handler in case of double faulting.
 */ 
static void PureCallHandler()
{
	static bool bHasAlreadyBeenCalled = false;
	FPlatformMisc::DebugBreak();
	if (bHasAlreadyBeenCalled)
	{
		// Call system handler if we're double faulting.
		if (DefaultPureCallHandler)
		{
			DefaultPureCallHandler();
		}
	}
	else
	{
		bHasAlreadyBeenCalled = true;
		if (GIsRunning)
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("Core", "PureVirtualFunctionCalledWhileRunningApp", "Pure virtual function being called while application was running (GIsRunning == 1)."));
		}
		UE_LOG(LogTemp, Fatal, TEXT("Pure virtual function being called"));
	}
}

/*-----------------------------------------------------------------------------
	SHA-1 functions.
-----------------------------------------------------------------------------*/

/**
 * Get the hash values out of the executable hash section
 *
 * NOTE: hash keys are stored in the executable, you will need to put a line like the
 *		 following into your PCLaunch.rc settings:
 *			ID_HASHFILE RCDATA "../../../../GameName/Build/Hashes.sha"
 *
 *		 Then, use the -sha option to the cooker (must be from commandline, not
 *       frontend) to generate the hashes for .ini, loc, startup packages, and .usf shader files
 *
 *		 You probably will want to make and checkin an empty file called Hashses.sha
 *		 into your source control to avoid linker warnings. Then for testing or final
 *		 final build ONLY, use the -sha command and relink your executable to put the
 *		 hashes for the current files into the executable.
 */
static void InitSHAHashes()
{
	//@todo.WinRT: Implement this using GetModuleSection?
}

void FWinRTMisc::PlatformPreInit()
{
	FGenericPlatformMisc::PlatformPreInit();

	// Use our own handler for pure virtuals being called.
	DefaultPureCallHandler = _set_purecall_handler(PureCallHandler);

	// initialize the file SHA hash mapping
	InitSHAHashes();
}

void FWinRTMisc::PlatformInit()
{
	// Identity.
	UE_LOG(LogInit, Log, TEXT("Computer: %s"), FPlatformProcess::ComputerName());
	UE_LOG(LogInit, Log, TEXT("User: %s"), FPlatformProcess::UserName());

	// Timer resolution.
	UE_LOG(LogInit, Log, TEXT("High frequency timer resolution =%f MHz"), 0.000001 / FPlatformTime::GetSecondsPerCycle());
}

GenericApplication* FWinRTMisc::CreateApplication()
{
	return FWinRTApplication::CreateWinRTApplication();
}

void FWinRTMisc::GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength)
{
	//@todo.WinRT:
	*Result = 0;
	ResultLength = 0;
}

// Defined in WinRTLaunch.cpp
extern void appWinPumpMessages();

void FWinRTMisc::PumpMessages(bool bFromMainLoop)
{
	// Handle all incoming messages if we're not using wxWindows in which case this is done by their
	// message loop.
	appWinPumpMessages();

	if (!bFromMainLoop)
	{
		// Process pending windows messages, which is necessary to the rendering thread in some rare cases where DX10
		// sends window messages (from IDXGISwapChain::Present) to the main thread owned viewport window.
		// Only process sent messages to minimize the opportunity for re-entry in the editor, since wx messages are not deferred.
		return;
	}

	// check to see if the window in the foreground was made by this process (ie, does this app
	// have focus)
	//@todo.WinRT: Will this always be true?
	bool HasFocus = true;
	// if its our window, allow sound, otherwise apply multiplier
	FApp::SetVolumeMultiplier(HasFocus ? 1.0f : FApp::GetUnfocusedVolumeMultiplier());
}

uint32 FWinRTMisc::GetCharKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings)
{
	return FGenericPlatformMisc::GetStandardPrintableKeyMap(KeyCodes, KeyNames, MaxMappings, true, false);
}

uint32 FWinRTMisc::GetKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings)
{
#define ADDKEYMAP(KeyCode, KeyName)		if (NumMappings<MaxMappings) { KeyCodes[NumMappings]=KeyCode; KeyNames[NumMappings]=KeyName; ++NumMappings; };

	uint32 NumMappings = 0;

	if ( KeyCodes && KeyNames && (MaxMappings > 0) )
	{
		ADDKEYMAP(VK_LBUTTON, TEXT("LeftMouseButton"));
		ADDKEYMAP(VK_RBUTTON, TEXT("RightMouseButton"));
		ADDKEYMAP(VK_MBUTTON, TEXT("MiddleMouseButton"));

		ADDKEYMAP(VK_XBUTTON1, TEXT("ThumbMouseButton"));
		ADDKEYMAP(VK_XBUTTON2, TEXT("ThumbMouseButton2"));

		ADDKEYMAP(VK_BACK, TEXT("BackSpace"));
		ADDKEYMAP(VK_TAB, TEXT("Tab"));
		ADDKEYMAP(VK_RETURN, TEXT("Enter"));
		ADDKEYMAP(VK_PAUSE, TEXT("Pause"));

		ADDKEYMAP(VK_CAPITAL, TEXT("CapsLock"));
		ADDKEYMAP(VK_ESCAPE, TEXT("Escape"));
		ADDKEYMAP(VK_SPACE, TEXT("SpaceBar"));
		ADDKEYMAP(VK_PRIOR, TEXT("PageUp"));
		ADDKEYMAP(VK_NEXT, TEXT("PageDown"));
		ADDKEYMAP(VK_END, TEXT("End"));
		ADDKEYMAP(VK_HOME, TEXT("Home"));

		ADDKEYMAP(VK_LEFT, TEXT("Left"));
		ADDKEYMAP(VK_UP, TEXT("Up"));
		ADDKEYMAP(VK_RIGHT, TEXT("Right"));
		ADDKEYMAP(VK_DOWN, TEXT("Down"));

		ADDKEYMAP(VK_INSERT, TEXT("Insert"));
		ADDKEYMAP(VK_DELETE, TEXT("Delete"));

		ADDKEYMAP(VK_NUMPAD0, TEXT("NumPadZero"));
		ADDKEYMAP(VK_NUMPAD1, TEXT("NumPadOne"));
		ADDKEYMAP(VK_NUMPAD2, TEXT("NumPadTwo"));
		ADDKEYMAP(VK_NUMPAD3, TEXT("NumPadThree"));
		ADDKEYMAP(VK_NUMPAD4, TEXT("NumPadFour"));
		ADDKEYMAP(VK_NUMPAD5, TEXT("NumPadFive"));
		ADDKEYMAP(VK_NUMPAD6, TEXT("NumPadSix"));
		ADDKEYMAP(VK_NUMPAD7, TEXT("NumPadSeven"));
		ADDKEYMAP(VK_NUMPAD8, TEXT("NumPadEight"));
		ADDKEYMAP(VK_NUMPAD9, TEXT("NumPadNine"));

		ADDKEYMAP(VK_MULTIPLY, TEXT("Multiply"));
		ADDKEYMAP(VK_ADD, TEXT("Add"));
		ADDKEYMAP(VK_SUBTRACT, TEXT("Subtract"));
		ADDKEYMAP(VK_DECIMAL, TEXT("Decimal"));
		ADDKEYMAP(VK_DIVIDE, TEXT("Divide"));

		ADDKEYMAP(VK_F1, TEXT("F1"));
		ADDKEYMAP(VK_F2, TEXT("F2"));
		ADDKEYMAP(VK_F3, TEXT("F3"));
		ADDKEYMAP(VK_F4, TEXT("F4"));
		ADDKEYMAP(VK_F5, TEXT("F5"));
		ADDKEYMAP(VK_F6, TEXT("F6"));
		ADDKEYMAP(VK_F7, TEXT("F7"));
		ADDKEYMAP(VK_F8, TEXT("F8"));
		ADDKEYMAP(VK_F9, TEXT("F9"));
		ADDKEYMAP(VK_F10, TEXT("F10"));
		ADDKEYMAP(VK_F11, TEXT("F11"));
		ADDKEYMAP(VK_F12, TEXT("F12"));

		ADDKEYMAP(VK_NUMLOCK, TEXT("NumLock"));

		ADDKEYMAP(VK_SCROLL, TEXT("ScrollLock"));

		ADDKEYMAP(VK_LSHIFT, TEXT("LeftShift"));
		ADDKEYMAP(VK_RSHIFT, TEXT("RightShift"));
		ADDKEYMAP(VK_LCONTROL, TEXT("LeftControl"));
		ADDKEYMAP(VK_RCONTROL, TEXT("RightControl"));
		ADDKEYMAP(VK_LMENU, TEXT("LeftAlt"));
		ADDKEYMAP(VK_RMENU, TEXT("RightAlt"));
	}

	check(NumMappings < MaxMappings);
	return NumMappings;
}

void FWinRTMisc::LowLevelOutputDebugString(const TCHAR *Message)
{
	OutputDebugString(Message);
}

void FWinRTMisc::RequestExit(bool Force)
{
	UE_LOG(LogTemp, Log, TEXT("FWinRTMisc::RequestExit(%i)"), Force);
	if (Force)
	{
		// Force immediate exit.
		// Dangerous because config code isn't flushed, global destructors aren't called, etc.
		//::TerminateProcess(GetCurrentProcess(), 0); 
		GIsRequestingExit = 1;
	}
	else
	{
		GIsRequestingExit = 1;
	}
}

const TCHAR* FWinRTMisc::GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error)
{
	check(OutBuffer && BufferCount);
	*OutBuffer = TEXT('\0');
	if (Error == 0)
	{
		Error = GetLastError();
	}
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, Error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), OutBuffer, BufferCount, NULL);
	TCHAR* Found = FCString::Strchr(OutBuffer,TEXT('\r'));
	if (Found)
	{
		*Found = TEXT('\0');
	}
	Found = FCString::Strchr(OutBuffer,TEXT('\n'));
	if (Found)
	{
		*Found = TEXT('\0');
	}
	return OutBuffer;
}

void FWinRTMisc::CreateGuid(FGuid& Result)
{
	verify(CoCreateGuid((GUID*)&Result)==S_OK);
}

int32 FWinRTMisc::NumberOfCores()
{
	//@todo.WinRT: Adjust this to final specifications
	return 7;
}

/** Get the application root directory. */
const TCHAR* FWinRTMisc::RootDir()
{
	static FString Path;
	if (Path.Len() == 0)
	{
		Path = FPlatformProcess::BaseDir();
		// if the path ends in a separator, remove it
		if (Path.Right(1)==TEXT("/"))
		{
			Path = Path.LeftChop(1);
		}
	}
	return *Path;
}
