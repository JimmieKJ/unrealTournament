// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
WinRTProcess.cpp: WinRT implementations of Process functions
=============================================================================*/

#include "CorePrivatePCH.h"

//@todo.WINRT: Remove this usage!!!!
#include "../Private/WinRT/ThreadEmulation.h"

#include "AllowWinRTPlatformTypes.h"

const TCHAR* FWinRTProcess::BaseDir()
{
	static bool bFirstTime = true;
	static TCHAR Result[512]=TEXT("");
	if (!Result[0] && bFirstTime)
	{
		{
			// Testing
			DWORD  Access    = GENERIC_WRITE;
			DWORD  WinFlags  = 0;
			DWORD  Create    = CREATE_ALWAYS;
			HANDLE Handle	 = CreateFile2(L"Test.txt", Access, WinFlags, Create, NULL);
			if (Handle != INVALID_HANDLE_VALUE)
			{
				CloseHandle(Handle);
			}
			else
			{
				DWORD LastErr = GetLastError();
				Handle = CreateFile2(L"c:\\Test.txt", Access, WinFlags, Create, NULL);
				if (Handle != INVALID_HANDLE_VALUE)
				{
					CloseHandle(Handle);
				}
				else
				{
					LastErr = GetLastError();
					uint32 dummy = LastErr;
				}
			}
		}

		// Check the commandline for -BASEDIR=<base directory>
		const TCHAR* CmdLine = FCommandLine::Get();
		if ((CmdLine == NULL) || (FCString::Strlen(CmdLine) == 0))
		{
			const LONG MaxCmdLineSize = 65536;
			char AnsiCmdLine[MaxCmdLineSize] = {0};
			// Load the text file...
			const TCHAR* CmdLineFilename = L"WinRTCmdLine.txt";
			DWORD  Access    = GENERIC_READ;
			DWORD  WinFlags  = FILE_SHARE_READ;
			DWORD  Create    = OPEN_EXISTING;
			HANDLE Handle	 = CreateFile2(CmdLineFilename, Access, WinFlags, Create, NULL);
			if (Handle == INVALID_HANDLE_VALUE)
			{
				DWORD ErrorCode = GetLastError();
				Handle = CreateFile2(L"D:\\Depot\\UE4\\ExampleGame\\Binaries\\WinRT\\AppX\\WinRTCmdLine.txt", Access, WinFlags, Create, NULL);
			}
			if (Handle != INVALID_HANDLE_VALUE)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed to open CmdLine text file via CreateFile2!!!"));

				LONG TotalSize = 0;
				// Get the file size
				WIN32_FILE_ATTRIBUTE_DATA Info;
				if (GetFileAttributesExW(CmdLineFilename, GetFileExInfoStandard, &Info) == TRUE)
				{
					if ((Info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
					{
						LARGE_INTEGER li;
						li.HighPart = Info.nFileSizeHigh;
						li.LowPart = Info.nFileSizeLow;
//							return li.QuadPart;
						TotalSize = Info.nFileSizeLow;
					}
				}

				if ((TotalSize > 0) && (TotalSize < MaxCmdLineSize))
				{
					DWORD Result=0;
					if (!ReadFile(Handle, AnsiCmdLine, DWORD(TotalSize), &Result, NULL) || (Result != DWORD(TotalSize)))
					{
						FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAILED TO READ CmdLine.txt file!!!"));
					}
					else
					{
						TCHAR CmdLine[MaxCmdLineSize] = {0};
						FCString::Strcpy(CmdLine, ANSI_TO_TCHAR(AnsiCmdLine));
						// Strip off \r\n
						while (CmdLine[FCString::Strlen(CmdLine) - 1] == TEXT('\n'))
						{
							CmdLine[FCString::Strlen(CmdLine) - 1] = 0;
							if (CmdLine[FCString::Strlen(CmdLine) - 1] == TEXT('\r'))
							{
								CmdLine[FCString::Strlen(CmdLine) - 1] = 0;
							}
						}
						// We have to do this here for WinRT due to the pre-init needing the BaseDir...
						FCommandLine::Set(CmdLine); 
					}
				}

				CloseHandle(Handle);
			}
			else
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed to open CmdLine.txt file!"));
			}
		}

		if (CmdLine != NULL)
		{
			FString BaseDirToken = TEXT("-BASEDIR=");
			FString NextToken;
			while (FParse::Token(CmdLine, NextToken, false))
			{
				if (NextToken.StartsWith(BaseDirToken) == true)
				{
					FString BaseDir = NextToken.Right(NextToken.Len() - BaseDirToken.Len());
					BaseDir.ReplaceInline(TEXT("\\"), TEXT("/"));
					FCString::Strcpy(Result, *BaseDir);
				}
			}
		}

		Platform::String^ LocationPath = Windows::ApplicationModel::Package::Current->InstalledLocation->Path;
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("LocationPath = %s\n"), LocationPath->Data());
		FCString::Strcpy(Result, LocationPath->Data());

		bFirstTime = false;
	}
	return Result;
}

#include "HideWinRTPlatformTypes.h"

void FWinRTProcess::Sleep( float Seconds )
{
	SCOPE_CYCLE_COUNTER(STAT_Sleep);
//	::Sleep((int32)(Seconds * 1000.0));
	FThreadIdleStats::FScopeIdle Scope;
	ThreadEmulation::Sleep((int32)(Seconds * 1000.0));
}

void FWinRTProcess::SleepInfinite()
{
//	::Sleep(INFINITE);
	ThreadEmulation::Sleep(INFINITE);
}

#include "WinRTEvent.h"

FEvent* FWinRTProcess::CreateSynchEvent(bool bIsManualReset /*= false*/)
{
	// Allocate the new object
	FEvent* Event = NULL;
	if (FPlatformProcess::SupportsMultithreading())
	{
		Event = new FEventWinRT();
	}
	else
	{
		// Fake vent object.
		Event = new FSingleThreadEvent();
	}
	// If the internal create fails, delete the instance and return NULL
	if (!Event->Create(bIsManualReset))
	{
		delete Event;
		Event = NULL;
	}
	return Event;
}

#include "AllowWinRTPlatformTypes.h"

bool FEventWinRT::Wait(uint32 WaitTime, const bool bIgnoreThreadIdleStats /*= false*/)
{
	FScopeCycleCounter Counter(StatID);
	FThreadIdleStats::FScopeIdle Scope(bIgnoreThreadIdleStats);
	check(Event);

	//		return (WaitForSingleObject(Event, WaitTime) == WAIT_OBJECT_0);
	return WaitForSingleObjectEx(Event, WaitTime, FALSE) == WAIT_OBJECT_0;
}

#include "HideWinRTPlatformTypes.h"

#include "../WinRT/WinRTRunnableThread.h"

FRunnableThread* FWinRTProcess::CreateRunnableThread()
{
	return new FRunnableThreadWinRT();
}
