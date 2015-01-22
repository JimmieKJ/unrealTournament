// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LaunchPrivatePCH.h"
#include <SDL.h>
#if PLATFORM_HTML5_BROWSER
	#include <emscripten.h>
#else
	// SDL defines main to be SDL_main and expects you to use SDLmain.  We don't.
	#undef main
#endif


DEFINE_LOG_CATEGORY_STATIC(LogHTML5Launch, Log, All);

#include "PlatformIncludes.h"
#include <string.h>

FEngineLoop	GEngineLoop;
TCHAR GCmdLine[2048];

#if PLATFORM_HTML5_WIN32 
#include "DLL/LoadDLL.h"
#endif 

void HTML5_Tick()
{
	GEngineLoop.Tick();
}

void HTML5_Init () 
{	
	// initialize the engine
	UE_LOG(LogTemp,Display,TEXT("PreInit Start"));
	GEngineLoop.PreInit(0, NULL, GCmdLine);
	UE_LOG(LogHTML5Launch,Display,TEXT("PreInit Complete"));
	UE_LOG(LogHTML5Launch,Display,TEXT("Init Start"));
	GEngineLoop.Init();
	UE_LOG(LogHTML5Launch,Display,TEXT("Init Complete"));


#if PLATFORM_HTML5_WIN32
	while (!GIsRequestingExit)
	{
		HTML5_Tick();
	}
#else
	emscripten_set_main_loop(HTML5_Tick, 0, true);
#endif


}


int main(int argc, char* argv[])
{
	UE_LOG(LogHTML5Launch,Display,TEXT("Starting UE4 ... %s\n"), GCmdLine);

#if PLATFORM_HTML5_WIN32
	// load the delay load DLLs
	HTML5Win32::LoadANGLE(TCHAR_TO_ANSI(*FPaths::EngineDir())); 
	HTML5Win32::LoadPhysXDLL (TCHAR_TO_ANSI(*FPaths::EngineDir()));  
	HTML5Win32::LoadOpenAL(TCHAR_TO_ANSI(*FPaths::EngineDir()));
	
#endif

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_NOPARACHUTE);
 	GCmdLine[0] = 0;
	// to-do use Platform Str functions. 
	FCString::Strcpy(GCmdLine, TEXT(" "));

 	for (int Option = 1; Option < argc; Option++)
 	{
		FCString::Strcat(GCmdLine, TEXT("  "));
 		FCString::Strcat(GCmdLine, ANSI_TO_TCHAR(argv[Option]));
 	}

	UE_LOG(LogHTML5Launch,Display,TEXT("Command line: %s\n"), GCmdLine);

	HTML5_Init();

	return 0;
}

void EmptyLinkFunctionForStaticInitializationHTML5Win32(void) {} 
