// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateViewerApp.h"
#include "ExceptionHandling.h"
#include "CocoaThread.h"
#if WITH_CEF3
#import "include/cef_application_mac.h"
#endif

static FString GSavedCommandLine;

@interface UE4AppDelegate : NSObject <NSApplicationDelegate, NSFileManagerDelegate>
{
}

@end

@implementation UE4AppDelegate

//handler for the quit apple event used by the Dock menu
- (void)handleQuitEvent:(NSAppleEventDescriptor*)Event withReplyEvent:(NSAppleEventDescriptor*)ReplyEvent
{
    [NSApp terminate:self];
}

- (void) runGameThread:(id)Arg
{
	FPlatformMisc::SetGracefulTerminationHandler();
	FPlatformMisc::SetCrashHandler(nullptr);
	
	RunSlateViewer(*GSavedCommandLine);
	
	[NSApp terminate: self];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)Sender;
{
	if(!GIsRequestingExit || ([NSThread gameThread] && [NSThread gameThread] != [NSThread mainThread]))
	{
		GIsRequestingExit = true;
		return NSTerminateLater;
	}
	else
	{
		return NSTerminateNow;
	}
}

- (void)applicationDidFinishLaunching:(NSNotification *)Notification
{
	//install the custom quit event handler
    NSAppleEventManager* appleEventManager = [NSAppleEventManager sharedAppleEventManager];
    [appleEventManager setEventHandler:self andSelector:@selector(handleQuitEvent:withReplyEvent:) forEventClass:kCoreEventClass andEventID:kAEQuitApplication];
	
	RunGameThread(self, @selector(runGameThread:));
}

@end

#if WITH_CEF3
@interface UE4Application : NSApplication<CefAppProtocol>
{
@private
	BOOL bHandlingSendEvent;
}
@end

@implementation UE4Application
- (BOOL)isHandlingSendEvent
{
	return bHandlingSendEvent;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent
{
	bHandlingSendEvent = handlingSendEvent;
}

- (void)sendEvent:(NSEvent*)event
{
	CefScopedSendingEvent sendingEventScoper;
	[super sendEvent:event];
}
@end
#endif

int main(int argc, char *argv[])
{
	for (int32 Option = 1; Option < argc; Option++)
	{
		GSavedCommandLine += TEXT(" ");
		FString Argument(ANSI_TO_TCHAR(argv[Option]));
		if (Argument.Contains(TEXT(" ")))
		{
			if (Argument.Contains(TEXT("=")))
			{
				FString ArgName;
				FString ArgValue;
				Argument.Split( TEXT("="), &ArgName, &ArgValue );
				Argument = FString::Printf( TEXT("%s=\"%s\""), *ArgName, *ArgValue );
			}
			else
			{
				Argument = FString::Printf(TEXT("\"%s\""), *Argument);
			}
		}
		GSavedCommandLine += Argument;
	}

	SCOPED_AUTORELEASE_POOL;
#if WITH_CEF3
	[UE4Application sharedApplication];
#else
	[NSApplication sharedApplication];
#endif
	[NSApp setDelegate:[UE4AppDelegate new]];
	[NSApp run];
	return 0;
}
