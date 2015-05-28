// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5Process.cpp: HTML5 implementations of Process functions
=============================================================================*/

#include "CorePrivatePCH.h"

const TCHAR* FHTML5PlatformProcess::ComputerName()
{
	return TEXT("Browser"); 
}
#if PLATFORM_HTML5_WIN32
#include <direct.h>
#endif
const TCHAR* FHTML5PlatformProcess::BaseDir()
{
#if PLATFORM_HTML5_WIN32
	static TCHAR Result[512]=TEXT("");
	if( !Result[0] )
	{
		// Get directory this executable was launched from.
		char* path;
		_get_pgmptr(&(path));
		FString TempResult(ANSI_TO_TCHAR(path));
		TempResult = TempResult.Replace(TEXT("\\"), TEXT("/"));
		FCString::Strcpy(Result, *TempResult);
		int32 StringLength = FCString::Strlen(Result);
		if(StringLength > 0)
		{
			--StringLength;
			for(; StringLength > 0; StringLength-- )
			{
				if( Result[StringLength - 1] == TEXT('/') || Result[StringLength - 1] == TEXT('\\') )
				{
					break;
				}
			}
		}
		Result[StringLength] = 0;

		FString CollapseResult(Result);
		FPaths::CollapseRelativeDirectories(CollapseResult);
		FCString::Strcpy(Result, *CollapseResult);
	}
	return Result;
#else
	return TEXT("");
#endif
}

DECLARE_CYCLE_STAT(TEXT("CPU Stall - HTML5Sleep"),STAT_HTML5Sleep,STATGROUP_CPUStalls);

void FHTML5PlatformProcess::Sleep( float Seconds )
{
	SCOPE_CYCLE_COUNTER(STAT_HTML5Sleep);
	FThreadIdleStats::FScopeIdle Scope;

	SleepNoStats(Seconds);
	// @todo html5 threading: actually put the thread to sleep for awhile (UE3 did nothing, we do the same now)
}

void FHTML5PlatformProcess::SleepNoStats(float Seconds)
{
	// @todo html5 threading: actually put the thread to sleep for awhile (UE3 did nothing, we do the same now)
}

void FHTML5PlatformProcess::SleepInfinite()
{
	// stop this thread forever
	//pause();
}

#include "HTML5PlatformRunnableThread.h"

FRunnableThread* FHTML5PlatformProcess::CreateRunnableThread()
{
	return new FHTML5RunnableThread();
}

class FEvent* FHTML5PlatformProcess::CreateSynchEvent( bool bIsManualReset /*= 0*/ )
{
	FEvent* Event = new FSingleThreadEvent();
	return Event;
}

bool FHTML5PlatformProcess::SupportsMultithreading()
{
   return false; 
}

void FHTML5PlatformProcess::LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error) 
{
	auto TmpURL = StringCast<ANSICHAR>(URL);
	EM_ASM_ARGS({var InUrl = Pointer_stringify($0); console.log("Opening "+InUrl); window.open(InUrl);}, (ANSICHAR*)TmpURL.Get());
}

const TCHAR* FHTML5PlatformProcess::ExecutableName(bool bRemoveExtension)
{
	return FApp::GetGameName();
}
