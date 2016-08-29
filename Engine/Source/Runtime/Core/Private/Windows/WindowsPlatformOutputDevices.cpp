// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#include "FeedbackContextAnsi.h"
#include "../Private/Windows/WindowsPlatformOutputDevicesPrivate.h"
#include "../Private/Windows/WindowsPlatformFeedbackContextPrivate.h"
#include "HAL/ThreadHeartBeat.h"

#include "AllowWindowsPlatformTypes.h"

namespace OutputDeviceConstants
{
	uint32 WIN_STD_OUTPUT_HANDLE = STD_OUTPUT_HANDLE;
	uint32 WIN_ATTACH_PARENT_PROCESS = ATTACH_PARENT_PROCESS;
}

#include "HideWindowsPlatformTypes.h"


//////////////////////////////////
// FWindowsPlatformOutputDevices
//////////////////////////////////

class FOutputDeviceError*			FWindowsPlatformOutputDevices::GetError()
{
	static FOutputDeviceWindowsError Singleton;
	return &Singleton;
}

class FOutputDevice*				FWindowsPlatformOutputDevices::GetEventLog()
{
#if WANTS_WINDOWS_EVENT_LOGGING
	static FOutputDeviceEventLog Singleton;
	return &Singleton;
#else // no event logging
	return NULL;
#endif //WANTS_WINDOWS_EVENT_LOGGING
}

class FOutputDeviceConsole*			FWindowsPlatformOutputDevices::GetLogConsole()
{
	// this is a slightly different kind of singleton that gives ownership to the caller and should not be called more than once
	return new FOutputDeviceConsoleWindows();
}


class FFeedbackContext*				FWindowsPlatformOutputDevices::GetWarn()
{
#if WITH_EDITOR
	static FFeedbackContextWindows Singleton;
	return &Singleton;
#else
	static FFeedbackContextAnsi Singleton;
	return &Singleton;
#endif
}

//////////////////////////////////
// FOutputDeviceWindowsError
//////////////////////////////////

FOutputDeviceWindowsError::FOutputDeviceWindowsError()
{
}

void FOutputDeviceWindowsError::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	FPlatformMisc::DebugBreak();
   
	if( !GIsCriticalError )
	{   
		const int32 LastError = ::GetLastError();

		// First appError.
		GIsCriticalError = 1;
		TCHAR ErrorBuffer[1024];
		ErrorBuffer[0] = 0;

		// Windows error.
		if (LastError == 0)
		{
			UE_LOG(LogWindows, Log, TEXT("Windows GetLastError: %s (%i)"), FPlatformMisc::GetSystemErrorMessage(ErrorBuffer, 1024, LastError), LastError);
		}
		else
		{
			UE_LOG(LogWindows, Error, TEXT("Windows GetLastError: %s (%i)"), FPlatformMisc::GetSystemErrorMessage(ErrorBuffer, 1024, LastError), LastError);
		}
	}
	else
	{
		UE_LOG(LogWindows, Error, TEXT("Error reentered: %s"), Msg );
	}

	if( GIsGuarded )
	{
		// Propagate error so structured exception handler can perform necessary work.
#if PLATFORM_EXCEPTIONS_DISABLED
		FPlatformMisc::DebugBreak();
#endif
		FPlatformMisc::RaiseException( 1 );
	}
	else
	{
		// We crashed outside the guarded code (e.g. appExit).
		HandleError();
		FPlatformMisc::RequestExit( true );
	}
}

void FOutputDeviceWindowsError::HandleError()
{
	// make sure we don't report errors twice
	static int32 CallCount = 0;
	int32 NewCallCount = FPlatformAtomics::InterlockedIncrement(&CallCount);
	if (NewCallCount != 1)
	{
		UE_LOG(LogWindows, Error, TEXT("HandleError re-entered.") );
		return;
	}
	
	GIsGuarded				= 0;
	GIsRunning				= 0;
	GIsCriticalError		= 1;
	GLogConsole				= NULL;
	GErrorHist[ARRAY_COUNT(GErrorHist)-1]=0;

	// Trigger the OnSystemFailure hook if it exists
	// make sure it happens after GIsGuarded is set to 0 in case this hook crashes
	FCoreDelegates::OnHandleSystemError.Broadcast();

	// Dump the error and flush the log.
#if !NO_LOGGING
	FDebug::OutputMultiLineCallstack(__FILE__, __LINE__, LogWindows.GetCategoryName(), TEXT("=== Critical error: ==="), GErrorHist, ELogVerbosity::Error);
#endif
	GLog->PanicFlushThreadedLogs();

	// Unhide the mouse.
	while( ::ShowCursor(true)<0 );
	// Release capture.
	::ReleaseCapture();
	// Allow mouse to freely roam around.
	::ClipCursor(NULL);
	
	// Copy to clipboard in non-cooked editor builds.
	FPlatformMisc::ClipboardCopy(GErrorHist);

	FPlatformMisc::SubmitErrorReport( GErrorHist, EErrorReportMode::Interactive );

	FCoreDelegates::OnShutdownAfterError.Broadcast();
}

////////////////////////////////////////
// FOutputDeviceConsoleWindows
////////////////////////////////////////

FOutputDeviceConsoleWindows::FOutputDeviceConsoleWindows()
	: ConsoleHandle(0)
	, OverrideColorSet(false)
	, bIsAttached(false)
{
}

FOutputDeviceConsoleWindows::~FOutputDeviceConsoleWindows()
{
	SaveToINI();

	// WRH - 2007/08/23 - This causes the process to take a LONG time to shut down when clicking the "close window"
	// button on the top-right of the console window.
	//FreeConsole();
}

void FOutputDeviceConsoleWindows::SaveToINI()
{
	if (GConfig && !IniFilename.IsEmpty())
	{
		HWND Handle = GetConsoleWindow();
		if (Handle != NULL)
		{
			RECT WindowRect;
			::GetWindowRect(Handle, &WindowRect);

			GConfig->SetInt(TEXT("DebugWindows"), TEXT("ConsoleX"), WindowRect.left, IniFilename);
			GConfig->SetInt(TEXT("DebugWindows"), TEXT("ConsoleY"), WindowRect.top, IniFilename);
		}
		
		if (ConsoleHandle != NULL)
		{
			CONSOLE_SCREEN_BUFFER_INFO Info;
			if (GetConsoleScreenBufferInfo(ConsoleHandle, &Info))
			{
				GConfig->SetInt(TEXT("DebugWindows"), TEXT("ConsoleWidth"), Info.dwSize.X, IniFilename);
				GConfig->SetInt(TEXT("DebugWindows"), TEXT("ConsoleHeight"), Info.dwSize.Y, IniFilename);
			}
		}
	}
}

void FOutputDeviceConsoleWindows::Show( bool ShowWindow )
{
	if( ShowWindow )
	{
		if( !ConsoleHandle )
		{
			if (!AllocConsole())
			{
				bIsAttached = true;
			}
			ConsoleHandle = GetStdHandle(OutputDeviceConstants::WIN_STD_OUTPUT_HANDLE);

			if( ConsoleHandle != INVALID_HANDLE_VALUE )
			{
				COORD Size;
				Size.X = 160;
				Size.Y = 4000;

				int32 ConsoleWidth = 160;
				int32 ConsoleHeight = 4000;
				int32 ConsolePosX = 0;
				int32 ConsolePosY = 0;
				bool bHasX = false;
				bool bHasY = false;

				if(GConfig)
				{
					GConfig->GetInt(TEXT("DebugWindows"), TEXT("ConsoleWidth"), ConsoleWidth, GGameIni);
					GConfig->GetInt(TEXT("DebugWindows"), TEXT("ConsoleHeight"), ConsoleHeight, GGameIni);
					bHasX = GConfig->GetInt(TEXT("DebugWindows"), TEXT("ConsoleX"), ConsolePosX, GGameIni);
					bHasY = GConfig->GetInt(TEXT("DebugWindows"), TEXT("ConsoleY"), ConsolePosY, GGameIni);

					Size.X = (SHORT)ConsoleWidth;
					Size.Y = (SHORT)ConsoleHeight;
				}

				SetConsoleScreenBufferSize( ConsoleHandle, Size );

				CONSOLE_SCREEN_BUFFER_INFO ConsoleInfo;

				// Try to set the window width to match the screen buffer width, so that no manual horizontal scrolling or resizing is necessary
				if (::GetConsoleScreenBufferInfo( ConsoleHandle, &ConsoleInfo ) != 0)
				{
					SMALL_RECT NewConsoleWindowRect = ConsoleInfo.srWindow;
					NewConsoleWindowRect.Right = ConsoleInfo.dwSize.X - 1;
					::SetConsoleWindowInfo( ConsoleHandle, true, &NewConsoleWindowRect );	
				}

				RECT WindowRect;
				::GetWindowRect( GetConsoleWindow(), &WindowRect );

				if (!FParse::Value(FCommandLine::Get(), TEXT("ConsoleX="), ConsolePosX) && !bHasX)
				{
					ConsolePosX = WindowRect.left;
				}

				if (!FParse::Value(FCommandLine::Get(), TEXT("ConsoleY="), ConsolePosY) && !bHasY)
				{
					ConsolePosY = WindowRect.top;
				}

				FDisplayMetrics DisplayMetrics;
				FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);

				// Make sure that the positions specified by INI/CMDLINE are proper
				static const int32 ActualConsoleWidth = WindowRect.right - WindowRect.left;
				static const int32 ActualConsoleHeight = WindowRect.bottom - WindowRect.top;

				static const int32 ActualScreenWidth = DisplayMetrics.VirtualDisplayRect.Right - DisplayMetrics.VirtualDisplayRect.Left;
				static const int32 ActualScreenHeight = DisplayMetrics.VirtualDisplayRect.Bottom - DisplayMetrics.VirtualDisplayRect.Top;

				static const int32 RightPadding = FMath::Max(50, FMath::Min((ActualConsoleWidth / 2), ActualScreenWidth / 2));
				static const int32 BottomPadding = FMath::Max(50, FMath::Min((ActualConsoleHeight / 2), ActualScreenHeight / 2));
				
				ConsolePosX = FMath::Min(FMath::Max(ConsolePosX, DisplayMetrics.VirtualDisplayRect.Left), DisplayMetrics.VirtualDisplayRect.Right - RightPadding);
				ConsolePosY = FMath::Min(FMath::Max(ConsolePosY, DisplayMetrics.VirtualDisplayRect.Top), DisplayMetrics.VirtualDisplayRect.Bottom - BottomPadding);

				::SetWindowPos( GetConsoleWindow(), HWND_TOP, ConsolePosX, ConsolePosY, 0, 0, SWP_NOSIZE | SWP_NOSENDCHANGING | SWP_NOZORDER );
				
				// set the control-c, etc handler
				FPlatformMisc::SetGracefulTerminationHandler();
			}
		}
	}
	else if( ConsoleHandle )
	{
		SaveToINI();

		ConsoleHandle = NULL;
		FreeConsole();
		bIsAttached = false;
	}
}

bool FOutputDeviceConsoleWindows::IsShown()
{
	return ConsoleHandle != NULL;
}

void FOutputDeviceConsoleWindows::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time )
{
	if( ConsoleHandle )
	{
		const double RealTime = Time == -1.0f ? FPlatformTime::Seconds() - GStartTime : Time;

		static bool Entry=false;
		if( !GIsCriticalError || Entry )
		{
			if (Verbosity == ELogVerbosity::SetColor)
			{
				SetColor( Data );
				OverrideColorSet = FCString::Strcmp(COLOR_NONE, Data) != 0;
			}
			else
			{
				bool bNeedToResetColor = false;
				if( !OverrideColorSet )
				{
					if( Verbosity == ELogVerbosity::Error )
					{
						SetColor( COLOR_RED );
						bNeedToResetColor = true;
					}
					else if( Verbosity == ELogVerbosity::Warning )
					{
						SetColor( COLOR_YELLOW );
						bNeedToResetColor = true;
					}
				}
				TCHAR OutputString[MAX_SPRINTF]=TEXT(""); //@warning: this is safe as FCString::Sprintf only use 1024 characters max
				FCString::Sprintf(OutputString,TEXT("%s%s"),*FOutputDeviceHelper::FormatLogLine(Verbosity, Category, Data, GPrintLogTimes,RealTime),LINE_TERMINATOR);
				uint32 Written;
				WriteConsole( ConsoleHandle, OutputString, FCString::Strlen(OutputString), (::DWORD*)&Written, NULL );

				if( bNeedToResetColor )
				{
					SetColor( COLOR_NONE );
				}
			}
		}
		else
		{
			Entry=true;
			Serialize( Data, Verbosity, Category );
			Entry=false;
		}
	}
}

void FOutputDeviceConsoleWindows::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	Serialize( Data, Verbosity, Category, -1.0 );
}

void FOutputDeviceConsoleWindows::SetColor( const TCHAR* Color )
{
	// here we can change the color of the text to display, it's in the format:
	// ForegroundRed | ForegroundGreen | ForegroundBlue | ForegroundBright | BackgroundRed | BackgroundGreen | BackgroundBlue | BackgroundBright
	// where each value is either 0 or 1 (can leave off trailing 0's), so 
	// blue on bright yellow is "00101101" and red on black is "1"
	// An empty string reverts to the normal gray on black
	if (FCString::Stricmp(Color, TEXT("")) == 0)
	{
		SetConsoleTextAttribute(ConsoleHandle, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);
	}
	else
	{
		// turn the string into a bunch of 0's and 1's
		TCHAR String[9];
		FMemory::Memset(String, 0, sizeof(TCHAR) * ARRAY_COUNT(String));
		FCString::Strncpy(String, Color, ARRAY_COUNT(String));
		for (TCHAR* S = String; *S; S++)
		{
			*S -= '0';
		}
		// make the color
		SetConsoleTextAttribute(ConsoleHandle, 
			(String[0] ? FOREGROUND_RED			: 0) | 
			(String[1] ? FOREGROUND_GREEN		: 0) | 
			(String[2] ? FOREGROUND_BLUE		: 0) | 
			(String[3] ? FOREGROUND_INTENSITY	: 0) | 
			(String[4] ? BACKGROUND_RED			: 0) | 
			(String[5] ? BACKGROUND_GREEN		: 0) | 
			(String[6] ? BACKGROUND_BLUE		: 0) | 
			(String[7] ? BACKGROUND_INTENSITY	: 0) );
	}
}

bool FOutputDeviceConsoleWindows::IsAttached()
{
	if (ConsoleHandle != NULL)
	{
		return bIsAttached;
	}
	else if (!AttachConsole(OutputDeviceConstants::WIN_ATTACH_PARENT_PROCESS))
	{
		if (GetLastError() == ERROR_ACCESS_DENIED)
		{
			return true;
		}
	}
	else
	{
		FreeConsole();		
	}
	return false;
}

bool FFeedbackContextWindows::YesNof(const FText& Question)
{
	if ((GIsClient || GIsEditor) && ((GIsSilent != true) && (FApp::IsUnattended() != true)))
	{
		FSlowHeartBeatScope SuspendHeartBeat;
		return(::MessageBox(NULL, Question.ToString().GetCharArray().GetData(), *NSLOCTEXT("Core", "Question", "Question").ToString(), MB_YESNO | MB_TASKMODAL) == IDYES);
	}
	else
	{
		return false;
	}
}