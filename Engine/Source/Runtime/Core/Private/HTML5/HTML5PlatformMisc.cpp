// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5Misc.cpp: HTML5 implementations of misc functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "HTML5Application.h"

#if PLATFORM_HTML5_BROWSER
#include "HTML5JavaScriptFx.h"
#endif 

#include "trace.h"

#if defined(_MSC_VER) && USING_CODE_ANALYSIS
	#pragma warning(push)
	#pragma warning(disable:28251)
	#pragma warning(disable:28252)
	#pragma warning(disable:28253)
#endif
	#include "unicode/locid.h"
#if defined(_MSC_VER) && USING_CODE_ANALYSIS
	#pragma warning(pop)
#endif
#include "GenericPlatformCrashContext.h"
#include <SDL.h>
#include <ctime>

#include "MapPakDownloaderModule.h"
#include "MapPakDownloader.h"


void FHTML5Misc::PlatformInit()
{
	// Identity.
	UE_LOG(LogInit, Log, TEXT("Computer: %s"), FPlatformProcess::ComputerName());
	UE_LOG(LogInit, Log, TEXT("User: %s"), FPlatformProcess::UserName());
	UE_LOG(LogInit, Log, TEXT("Current Culture: %s"), *FHTML5Misc::GetDefaultLocale());

	// Timer resolution.
	UE_LOG(LogInit, Log, TEXT("High frequency timer resolution =%f MHz"), 0.000001 / FPlatformTime::GetSecondsPerCycle());
}

GenericApplication* FHTML5Misc::CreateApplication()
{
	return FHTML5Application::CreateHTML5Application();
}

uint32 FHTML5Misc::GetCharKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings)
{
	return FGenericPlatformMisc::GetStandardPrintableKeyMap(KeyCodes, KeyNames, MaxMappings, false, true);
}

uint32 FHTML5Misc::GetKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings)
{
#define ADDKEYMAP(KeyCode, KeyName)		if (NumMappings<MaxMappings) { KeyCodes[NumMappings]=KeyCode; KeyNames[NumMappings]=KeyName; ++NumMappings; };

	uint32 NumMappings = 0;

	if (KeyCodes && KeyNames && (MaxMappings > 0))
	{
		ADDKEYMAP(SDL_SCANCODE_BACKSPACE, TEXT("BackSpace"));
		ADDKEYMAP(SDL_SCANCODE_TAB, TEXT("Tab"));
		ADDKEYMAP(SDL_SCANCODE_RETURN, TEXT("Enter"));
		ADDKEYMAP(SDL_SCANCODE_RETURN2, TEXT("Enter"));
		ADDKEYMAP(SDL_SCANCODE_KP_ENTER, TEXT("Enter"));
		ADDKEYMAP(SDL_SCANCODE_PAUSE, TEXT("Pause"));

		ADDKEYMAP(SDL_SCANCODE_ESCAPE, TEXT("Escape"));
		ADDKEYMAP(SDL_SCANCODE_SPACE, TEXT("SpaceBar"));
		ADDKEYMAP(SDL_SCANCODE_PAGEUP, TEXT("PageUp"));
		ADDKEYMAP(SDL_SCANCODE_PAGEDOWN, TEXT("PageDown"));
		ADDKEYMAP(SDL_SCANCODE_END, TEXT("End"));
		ADDKEYMAP(SDL_SCANCODE_HOME, TEXT("Home"));

		ADDKEYMAP(SDL_SCANCODE_LEFT, TEXT("Left"));
		ADDKEYMAP(SDL_SCANCODE_UP, TEXT("Up"));
		ADDKEYMAP(SDL_SCANCODE_RIGHT, TEXT("Right"));
		ADDKEYMAP(SDL_SCANCODE_DOWN, TEXT("Down"));

		ADDKEYMAP(SDL_SCANCODE_INSERT, TEXT("Insert"));
		ADDKEYMAP(SDL_SCANCODE_DELETE, TEXT("Delete"));

		ADDKEYMAP(SDL_SCANCODE_F1, TEXT("F1"));
		ADDKEYMAP(SDL_SCANCODE_F2, TEXT("F2"));
		ADDKEYMAP(SDL_SCANCODE_F3, TEXT("F3"));
		ADDKEYMAP(SDL_SCANCODE_F4, TEXT("F4"));
		ADDKEYMAP(SDL_SCANCODE_F5, TEXT("F5"));
		ADDKEYMAP(SDL_SCANCODE_F6, TEXT("F6"));
		ADDKEYMAP(SDL_SCANCODE_F7, TEXT("F7"));
		ADDKEYMAP(SDL_SCANCODE_F8, TEXT("F8"));
		ADDKEYMAP(SDL_SCANCODE_F9, TEXT("F9"));
		ADDKEYMAP(SDL_SCANCODE_F10, TEXT("F10"));
		ADDKEYMAP(SDL_SCANCODE_F11, TEXT("F11"));
		ADDKEYMAP(SDL_SCANCODE_F12, TEXT("F12"));

		ADDKEYMAP(SDL_SCANCODE_CAPSLOCK, TEXT("CapsLock"));
		ADDKEYMAP(SDL_SCANCODE_LCTRL, TEXT("LeftControl"));
		ADDKEYMAP(SDL_SCANCODE_LSHIFT, TEXT("LeftShift"));
		ADDKEYMAP(SDL_SCANCODE_LALT, TEXT("LeftAlt"));
		ADDKEYMAP(SDL_SCANCODE_RCTRL, TEXT("RightControl"));
		ADDKEYMAP(SDL_SCANCODE_RSHIFT, TEXT("RightShift"));
		ADDKEYMAP(SDL_SCANCODE_RALT, TEXT("RightAlt"));
	}

	check(NumMappings < MaxMappings);
	return NumMappings;

	return NumMappings;
}

FString FHTML5Misc::GetDefaultLocale()
{
#if PLATFORM_HTML5_BROWSER
	char AsciiCultureName[512];
	if (UE_GetCurrentCultureName(AsciiCultureName,sizeof(AsciiCultureName)))
	{
		return FString(StringCast<TCHAR>(AsciiCultureName).Get());
	}
	else
#endif
	{
		icu::Locale ICUDefaultLocale = icu::Locale::getDefault();
		return FString(ICUDefaultLocale.getName());
	}
}

EAppReturnType::Type FHTML5Misc::MessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)
{

#if PLATFORM_HTML5_BROWSER 

	ANSICHAR* AText = TCHAR_TO_ANSI(Text);
	ANSICHAR* ACaption = TCHAR_TO_ANSI(Caption);
	return static_cast<EAppReturnType::Type>(UE_MessageBox(MsgType,AText,ACaption));

#endif 

#if PLATFORM_HTML5_WIN32

	return FGenericPlatformMisc::MessageBoxExt(MsgType, Text, Caption); 

#endif 

}

void (*GHTML5CrashHandler)(const FGenericCrashContext& Context) = nullptr;

extern "C"
{
#if PLATFORM_HTML5_BROWSER
	// callback from javascript. 
	void on_fatal(const char* msg, const char* error)
	{
#ifdef __EMSCRIPTEN_TRACING__
		emscripten_log(EM_LOG_CONSOLE, "Fatal Error: Closing trace!");
		emscripten_trace_close();
#endif
		// !!JM todo: pass msg & error to a crash context? Must be copied?
		if (GHTML5CrashHandler)
		{
			FGenericCrashContext Ctx;
			GHTML5CrashHandler(Ctx);
		}
	}
#endif 
}

void FHTML5Misc::SetCrashHandler(void(* CrashHandler)(const FGenericCrashContext& Context))
{
	GHTML5CrashHandler = CrashHandler;
}

const void FHTML5Misc::PreLoadMap(FString& Map, FString& LastMap, void* DynData)
{
	static TSharedPtr<FMapPakDownloader> Downloader = FModuleManager::GetModulePtr<IMapPakDownloaderModule>("MapPakDownloader")->GetDownloader();
	Downloader->Cache(Map, LastMap, DynData);
}

void FHTML5Misc::PlatformPostInit(bool ShowSplashScreen /*= false*/)
{
	FModuleManager::Get().LoadModule("MapPakDownloader");
}
