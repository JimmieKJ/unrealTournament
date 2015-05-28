// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MacPlatformOutputDevices.mm: Mac implementations of OutputDevices functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "Misc/App.h"

#include "FeedbackContextAnsi.h"
#include "MacApplication.h"
#include "MacPlatformOutputDevicesPrivate.h"
#include "MacPlatformFeedbackContextPrivate.h"
#include "CocoaThread.h"

//////////////////////////////////
// FMacPlatformOutputDevices
//////////////////////////////////

class FOutputDeviceError*			FMacPlatformOutputDevices::GetError()
{
	static FOutputDeviceMacError Singleton;
	return &Singleton;
}

class FOutputDevice*				FMacPlatformOutputDevices::GetEventLog()
{
	return NULL;
}

class FOutputDeviceConsole*			FMacPlatformOutputDevices::GetLogConsole()
{
	// this is a slightly different kind of singleton that gives ownership to the caller and should not be called more than once
	// this is a slightly different kind of singleton that gives ownership to the caller and should not be called more than once
	return new FOutputDeviceConsoleMac();
}


class FFeedbackContext*				FMacPlatformOutputDevices::GetWarn()
{
#if WITH_EDITOR
	static FFeedbackContextMac Singleton;
	return &Singleton;
#else
	static FFeedbackContextAnsi Singleton;
	return &Singleton;
#endif
}

//////////////////////////////////
// FOutputDeviceMacError
//////////////////////////////////

FOutputDeviceMacError::FOutputDeviceMacError()
:	ErrorPos(0)
{}

void FOutputDeviceMacError::Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	FPlatformMisc::DebugBreak();

	if( !GIsCriticalError )
	{   
		// First appError.
		GIsCriticalError = 1;
		TCHAR ErrorBuffer[1024];
		ErrorBuffer[0] = 0;
		// pop up a crash window if we are not in unattended mode
		if( FApp::IsUnattended() == false )
		{ 
			UE_LOG(LogMac, Error, TEXT("appError called: %s"), Msg );
		} 
		// log the warnings to the log
		else
		{
			UE_LOG(LogMac, Error, TEXT("appError called: %s"), Msg );
		}
		FCString::Strncpy( GErrorHist, Msg, ARRAY_COUNT(GErrorHist) - 5 );
		FCString::Strncat( GErrorHist, TEXT("\r\n\r\n"), ARRAY_COUNT(GErrorHist) - 1  );
		ErrorPos = FCString::Strlen(GErrorHist);
	}
	else
	{
		UE_LOG(LogMac, Error, TEXT("Error reentered: %s"), Msg );
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

void FOutputDeviceMacError::HandleError()
{
	// make sure we don't report errors twice
	static int32 CallCount = 0;
	int32 NewCallCount = FPlatformAtomics::InterlockedIncrement(&CallCount);
	if (NewCallCount != 1)
	{
		UE_LOG(LogMac, Error, TEXT("HandleError re-entered.") );
		return;
	}

	// Trigger the OnSystemFailure hook if it exists
	FCoreDelegates::OnHandleSystemError.Broadcast();

	GIsGuarded				= 0;
	GIsRunning				= 0;
	GIsCriticalError		= 1;
	GLogConsole				= NULL;
	GErrorHist[ARRAY_COUNT(GErrorHist)-1]=0;

	// Dump the error and flush the log.
	UE_LOG(LogMac, Log, TEXT("=== Critical error: ===") LINE_TERMINATOR TEXT("%s") LINE_TERMINATOR, GErrorExceptionDescription);
	UE_LOG(LogMac, Log, GErrorHist);

	GLog->Flush();

	// Unhide the mouse.
	// @TODO: Remove usage of deprecated CGCursorIsVisible function
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	while (!CGCursorIsVisible())
	{
		CGDisplayShowCursor(kCGDirectMainDisplay);
	}
#pragma clang diagnostic pop
	// Release capture and allow mouse to freely roam around.
	CGAssociateMouseAndMouseCursorPosition(true);

	FPlatformMisc::ClipboardCopy(GErrorHist);

	FPlatformMisc::SubmitErrorReport( GErrorHist, EErrorReportMode::Interactive );

	FCoreDelegates::OnShutdownAfterError.Broadcast();
}

@implementation FMacConsoleWindow
- (void)windowWillClose:(NSNotification*)Notification
{
	if (!MacApplication && [[NSApp orderedWindows] count] == 1)
	{
		_Exit(0);
	}
}
@end

////////////////////////////////////////
// FOutputDeviceConsoleMac
////////////////////////////////////////

FOutputDeviceConsoleMac::FOutputDeviceConsoleMac()
	: ConsoleHandle(NULL)
	, TextView(NULL)
	, ScrollView(NULL)
	, TextViewTextColor(NULL)
	, OutstandingTasks(0)
{
}

FOutputDeviceConsoleMac::~FOutputDeviceConsoleMac()
{
	DestroyConsole();
}

void FOutputDeviceConsoleMac::SaveToINI()
{
	if (GConfig && IniFilename.Len())
	{
		NSRect Frame = [ConsoleHandle frame];

		GConfig->SetInt(TEXT("DebugMac"), TEXT("ConsoleWidth"), Frame.size.width, IniFilename);
		GConfig->SetInt(TEXT("DebugMac"), TEXT("ConsoleHeight"), Frame.size.height, IniFilename);
		GConfig->SetInt(TEXT("DebugMac"), TEXT("ConsoleX"), Frame.origin.x, IniFilename);
		GConfig->SetInt(TEXT("DebugMac"), TEXT("ConsoleY"), Frame.origin.y, IniFilename);
	}
}

void FOutputDeviceConsoleMac::CreateConsole()
{
	if (ConsoleHandle || GIsBuildMachine)
	{
		return;
	}

	SCOPED_AUTORELEASE_POOL;

	int32 ConsoleWidth = 800;
	int32 ConsoleHeight = 600;
	int32 ConsolePosX = 0;
	int32 ConsolePosY = 0;
	bool bHasX = false;
	bool bHasY = false;

	if(GConfig)
	{
		GConfig->GetInt(TEXT("DebugMac"), TEXT("ConsoleWidth"), ConsoleWidth, GGameIni);
		GConfig->GetInt(TEXT("DebugMac"), TEXT("ConsoleHeight"), ConsoleHeight, GGameIni);
		bHasX = GConfig->GetInt(TEXT("DebugMac"), TEXT("ConsoleX"), ConsolePosX, GGameIni);
		bHasY = GConfig->GetInt(TEXT("DebugMac"), TEXT("ConsoleY"), ConsolePosY, GGameIni);
	}

	MainThreadCall(^{
		ConsoleHandle = [[FMacConsoleWindow alloc] initWithContentRect: NSMakeRect(ConsolePosX, ConsolePosY, ConsoleWidth, ConsoleHeight)
										styleMask: (NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask)
										  backing: NSBackingStoreBuffered
											defer: NO];
		[ConsoleHandle setDelegate:ConsoleHandle];

		ScrollView = [[NSScrollView alloc] initWithFrame:[[ConsoleHandle contentView] frame]];
		NSSize ContentSize = [ScrollView contentSize];
		
		[ScrollView setBorderType:NSNoBorder];
		[ScrollView setHasVerticalScroller:YES];
		[ScrollView setHasHorizontalScroller:NO];
		[ScrollView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
		
		TextView = [[NSTextView alloc] initWithFrame:NSMakeRect( 0, 0, ContentSize.width, ContentSize.height )];
		[TextView setMinSize:NSMakeSize( 0.0, ContentSize.height ) ];
		[TextView setMaxSize:NSMakeSize( FLT_MAX, FLT_MAX )];
		[TextView setVerticallyResizable:YES];
		[TextView setHorizontallyResizable:NO];
		[TextView setAutoresizingMask:NSViewWidthSizable];
		[TextView setBackgroundColor: [NSColor blackColor]];
		
		[[TextView textContainer] setContainerSize:NSMakeSize( ContentSize.width, FLT_MAX )];
		[[TextView textContainer] setWidthTracksTextView:YES];
		
		[ScrollView setDocumentView:TextView];
		[ConsoleHandle setContentView:ScrollView];

		if (!bHasX || !bHasY)
		{
			[ConsoleHandle center];
		}

		[ConsoleHandle setOpaque:YES];
		[ConsoleHandle makeKeyAndOrderFront:nil];
		
		if(!MacApplication)
		{
			do
			{
				FPlatformMisc::PumpMessages( true );
			} while(ConsoleHandle && ![ConsoleHandle isVisible]);
		}
		
		SetDefaultTextColor();
	}, UE4NilEventMode, true);
}

void FOutputDeviceConsoleMac::DestroyConsole()
{
	if (ConsoleHandle)
	{
		do
		{
			FPlatformMisc::PumpMessages( true );
		} while(OutstandingTasks);

		SaveToINI();
		
		MainThreadCall(^{
			SCOPED_AUTORELEASE_POOL;
			if( TextViewTextColor )
				[TextViewTextColor release];
			
			[ConsoleHandle close];
			ConsoleHandle = NULL;
			TextViewTextColor = NULL;
		}, UE4NilEventMode, true);
	}
}

void FOutputDeviceConsoleMac::Show( bool ShowWindow )
{
	if( ShowWindow )
	{
		CreateConsole();
	}
	else
	{
		DestroyConsole();
	}
}

bool FOutputDeviceConsoleMac::IsShown()
{
	return ConsoleHandle != NULL;
}

void FOutputDeviceConsoleMac::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	if( ConsoleHandle )
	{
		FScopeLock ScopeLock( &CriticalSection );

		static bool Entry=0;
		if( !GIsCriticalError || Entry )
		{
			// here we can change the color of the text to display, it's in the format:
			// ForegroundRed | ForegroundGreen | ForegroundBlue | ForegroundBright | BackgroundRed | BackgroundGreen | BackgroundBlue | BackgroundBright
			// where each value is either 0 or 1 (can leave off trailing 0's), so 
			// blue on bright yellow is "00101101" and red on black is "1"
			// An empty string reverts to the normal gray on black
			
			if (Verbosity == ELogVerbosity::SetColor)
			{
				if (FCString::Stricmp(Data, TEXT("")) == 0)
				{
					SetDefaultTextColor();
				}
				else
				{
					SCOPED_AUTORELEASE_POOL;
					
					// turn the string into a bunch of 0's and 1's
					TCHAR String[9];
					FMemory::Memset(String, 0, sizeof(TCHAR) * ARRAY_COUNT(String));
					FCString::Strncpy(String, Data, ARRAY_COUNT(String));
					for (TCHAR* S = String; *S; S++)
					{
						*S -= '0';
					}
					
					NSMutableArray* Colors = [[NSMutableArray alloc] init];
					NSMutableArray* AttributeKeys = [[NSMutableArray alloc] init];
					
					// Get FOREGROUND_INTENSITY and calculate final color
					CGFloat Intensity = String[3] ? 1.0 : 0.5;
					[Colors addObject:[NSColor colorWithSRGBRed:(String[0] ? 1.0 * Intensity : 0.0) green:(String[1] ? 1.0 * Intensity : 0.0) blue:(String[2] ? 1.0 * Intensity : 0.0) alpha:1.0]];
					
					// Get BACKGROUND_INTENSITY and calculate final color
					Intensity = String[7] ? 1.0 : 0.5;
					[Colors addObject:[NSColor colorWithSRGBRed:(String[4] ? 1.0 * Intensity : 0.0) green:(String[5] ? 1.0 * Intensity : 0.0) blue:(String[6] ? 1.0 * Intensity : 0.0) alpha:1.0]];
					
					[AttributeKeys addObject:NSForegroundColorAttributeName];
					[AttributeKeys addObject:NSBackgroundColorAttributeName];
					
					OutstandingTasks++;
					MainThreadCall(^{
						if( TextViewTextColor )
							[TextViewTextColor release];
						
						TextViewTextColor = [[NSDictionary alloc] initWithObjects:Colors forKeys:AttributeKeys];
						
						[Colors release];
						[AttributeKeys release];
						OutstandingTasks--;
					}, NSDefaultRunLoopMode, false);
				}
			}
			else
			{
				SCOPED_AUTORELEASE_POOL;
				
				TCHAR OutputString[MAX_SPRINTF]=TEXT(""); //@warning: this is safe as FCString::Sprintf only use 1024 characters max
				FCString::Sprintf(OutputString,TEXT("%s%s"),*FOutputDevice::FormatLogLine(Verbosity, Category, Data, GPrintLogTimes),LINE_TERMINATOR);

				CFStringRef CocoaText = FPlatformString::TCHARToCFString(OutputString);
				
				OutstandingTasks++;
				MainThreadCall(^{
					NSAttributedString *AttributedString = [[NSAttributedString alloc] initWithString:(NSString*)CocoaText attributes:TextViewTextColor];
					[[TextView textStorage] appendAttributedString:AttributedString];
					[TextView scrollRangeToVisible:NSMakeRange([[TextView string] length], 0)];
					[AttributedString release];
					CFRelease(CocoaText);
					OutstandingTasks--;
				}, NSDefaultRunLoopMode, false);
				
				if(!MacApplication)
				{
					FPlatformMisc::PumpMessages( true );
				}
			}
		}
		else
		{
			Entry=1;
			try
			{
				// Ignore errors to prevent infinite-recursive exception reporting.
				Serialize( Data, Verbosity, Category );
			}
			catch( ... )
			{}
			Entry=0;
		}
	}
}

void FOutputDeviceConsoleMac::SetDefaultTextColor()
{
	SCOPED_AUTORELEASE_POOL;
	FScopeLock ScopeLock( &CriticalSection );

	NSMutableArray* Colors = [[NSMutableArray alloc] init];
	NSMutableArray* AttributeKeys = [[NSMutableArray alloc] init];
	
	[Colors addObject:[NSColor grayColor]];
	[Colors addObject:[NSColor blackColor]];
	
	[AttributeKeys addObject:NSForegroundColorAttributeName];
	[AttributeKeys addObject:NSBackgroundColorAttributeName];

	OutstandingTasks++;
	MainThreadCall(^{
		if( TextViewTextColor )
			[TextViewTextColor release];
		
		TextViewTextColor = [[NSDictionary alloc] initWithObjects:Colors forKeys:AttributeKeys];
		
		[Colors release];
		[AttributeKeys release];
		OutstandingTasks--;
	}, NSDefaultRunLoopMode, false);
}
