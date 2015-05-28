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
#include "trace.h"

FEngineLoop	GEngineLoop;
TCHAR GCmdLine[2048];

#if PLATFORM_HTML5_WIN32 
#include "DLL/LoadDLL.h"
#endif 

void HTML5_Tick()
{
	static uint32 FrameCount = 1;
	char Buf[128];
	sprintf(Buf, "Frame %u", FrameCount);
	emscripten_trace_record_frame_start();
	emscripten_trace_enter_context(Buf);
	GEngineLoop.Tick();
	emscripten_trace_exit_context();
	emscripten_trace_record_frame_end();

	// Assuming 60fps, log out the memory report. This isn't important to be
	// exactly every second, it just needs done periodically
	if (FrameCount % 60 == 0)
	{
		emscripten_trace_report_memory_layout();
	}

	++FrameCount;
}

void HTML5_Init () 
{	
	emscripten_trace_record_frame_start();

	// initialize the engine
	UE_LOG(LogTemp,Display,TEXT("PreInit Start"));
	emscripten_trace_enter_context("PreInit");
	GEngineLoop.PreInit(0, NULL, GCmdLine);
	emscripten_trace_exit_context();
	UE_LOG(LogHTML5Launch,Display,TEXT("PreInit Complete"));
	UE_LOG(LogHTML5Launch,Display,TEXT("Init Start"));
	emscripten_trace_enter_context("Init");
	GEngineLoop.Init();
	emscripten_trace_exit_context();
	UE_LOG(LogHTML5Launch,Display,TEXT("Init Complete"));

	emscripten_trace_record_frame_end();


#if PLATFORM_HTML5_WIN32
	while (!GIsRequestingExit)
	{
		HTML5_Tick();
	}
#else
	emscripten_set_main_loop(HTML5_Tick, 0, true);
#endif


}

class FHTML5Exec : private FSelfRegisteringExec
{
public:

	FHTML5Exec()
		{}

	/** Console commands **/
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{
		if (FParse::Command(&Cmd, TEXT("em_trace_close")))
		{
			emscripten_trace_exit_context();//"main");
			emscripten_trace_close();
			return true;
		}
		return false;
	}
};
static TAutoPtr<FHTML5Exec> GHTML5Exec;

int main(int argc, char* argv[])
{
	UE_LOG(LogHTML5Launch,Display,TEXT("Starting UE4 ... %s\n"), GCmdLine);

#if PLATFORM_HTML5_WIN32
	// load the delay load DLLs
	HTML5Win32::LoadANGLE(TCHAR_TO_ANSI(*FPaths::EngineDir())); 
	HTML5Win32::LoadPhysXDLL (TCHAR_TO_ANSI(*FPaths::EngineDir()));  
	HTML5Win32::LoadOpenAL(TCHAR_TO_ANSI(*FPaths::EngineDir()));
	
#endif

	// TODO: configure this via the command line?
	emscripten_trace_configure("http://127.0.0.1:5000/", "UE4Game");
	GHTML5Exec = new FHTML5Exec();

	emscripten_trace_enter_context("main");

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
