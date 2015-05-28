// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#import <UIKit/UIKit.h>

#include "LaunchPrivatePCH.h"
#include "IOSAppDelegate.h"
#include "IOSView.h"
#include "IOSCommandLineHelper.h"
#include "GameLaunchDaemonMessageHandler.h"
#include "AudioDevice.h"


FEngineLoop GEngineLoop;
FGameLaunchDaemonMessageHandler GCommandSystem;

void FAppEntry::Suspend()
{
	if (GEngine && GEngine->GetMainAudioDevice())
	{
		GEngine->GetMainAudioDevice()->SuspendContext();
	}
}

void FAppEntry::Resume()
{
	if (GEngine && GEngine->GetMainAudioDevice())
	{
		GEngine->GetMainAudioDevice()->ResumeContext();
	}
}

void FAppEntry::PreInit(IOSAppDelegate* AppDelegate, UIApplication* Application)
{
	// make a controller object
	AppDelegate.IOSController = [[IOSViewController alloc] init];
	
	// property owns it now
	[AppDelegate.IOSController release];

	// point to the GL view we want to use
	AppDelegate.RootView = [AppDelegate.IOSController view];

	if (AppDelegate.OSVersion >= 6.0f)
	{
		// this probably works back to OS4, but would need testing
		[AppDelegate.Window setRootViewController:AppDelegate.IOSController];
	}
	else
	{
		[AppDelegate.Window addSubview:AppDelegate.RootView];
	}

	// reset badge count on launch
	Application.applicationIconBadgeNumber = 0;
}

static void MainThreadInit()
{
	IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];

	// Size the view appropriately for any potentially dynamically attached displays,
	// prior to creating any framebuffers
	CGRect MainFrame = [[UIScreen mainScreen] bounds];

	// we need to swap if compiled with ios7, or compiled with ios8 and running on 7
#ifndef __IPHONE_8_0
	bool bDoLandscapeSwap = true;
#else
	bool bDoLandscapeSwap = AppDelegate.OSVersion < 8.0f;
#endif

	if (bDoLandscapeSwap && !AppDelegate.bDeviceInPortraitMode)
	{
		Swap(MainFrame.size.width, MainFrame.size.height);
	}
	
	// @todo: use code similar for presizing for secondary screens
// 	CGRect FullResolutionRect =
// 		CGRectMake(
// 		0.0f,
// 		0.0f,
// 		GSystemSettings.bAllowSecondaryDisplays ?
// 		Max<float>(MainFrame.size.width, GSystemSettings.SecondaryDisplayMaximumWidth)	:
// 		MainFrame.size.width,
// 		GSystemSettings.bAllowSecondaryDisplays ?
// 		Max<float>(MainFrame.size.height, GSystemSettings.SecondaryDisplayMaximumHeight) :
// 		MainFrame.size.height
// 		);

	CGRect FullResolutionRect = MainFrame;

	AppDelegate.IOSView = [[FIOSView alloc] initWithFrame:FullResolutionRect];
	AppDelegate.IOSView.clearsContextBeforeDrawing = NO;
	AppDelegate.IOSView.multipleTouchEnabled = YES;

	// add it to the window
	[AppDelegate.RootView addSubview:AppDelegate.IOSView];

	// initialize the backbuffer of the view (so the RHI can use it)
	[AppDelegate.IOSView CreateFramebuffer:YES];
}



void FAppEntry::PlatformInit()
{

	// call a function in the main thread to do some processing that needs to happen there, now that the .ini files are loaded
	dispatch_async(dispatch_get_main_queue(), ^{ MainThreadInit(); });

	// wait until the GLView is fully initialized, so the RHI can be initialized
	IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];

	while (!AppDelegate.IOSView || !AppDelegate.IOSView->bIsInitialized)
	{
		FPlatformProcess::Sleep(0.001f);
	}

	// set the GL context to this thread
	[AppDelegate.IOSView MakeCurrent];
}

void FAppEntry::Init()
{
	FPlatformProcess::SetRealTimeMode();

	//extern TCHAR GCmdLine[16384];
	GEngineLoop.PreInit(FCommandLine::Get());

	// initialize messaging subsystem
	FModuleManager::LoadModuleChecked<IMessagingModule>("Messaging");

	//Set up the message handling to interface with other endpoints on our end.
	NSLog(@"%s", "Initializing ULD Communications in game mode\n");
	GCommandSystem.Init();

	GLog->SetCurrentThreadAsMasterThread();

	// start up the engine
	GEngineLoop.Init();
}

static FSuspendRenderingThread* SuspendThread = NULL;

void FAppEntry::Tick()
{
    if (SuspendThread != NULL)
    {
        delete SuspendThread;
        SuspendThread = NULL;
        FPlatformProcess::SetRealTimeMode();
    }
    
	// tick the engine
	GEngineLoop.Tick();
}

void FAppEntry::SuspendTick()
{
    if (!SuspendThread)
    {
        SuspendThread = new FSuspendRenderingThread(true);
    }
    
    FPlatformProcess::Sleep(0.1f);
}

void FAppEntry::Shutdown()
{
	NSLog(@"%s", "Shutting down Game ULD Communications\n");
	GCommandSystem.Shutdown();
    
    // kill the engine
    GEngineLoop.Exit();
}

FString GSavedCommandLine;

int main(int argc, char *argv[])
{
    for(int Option = 1; Option < argc; Option++)
	{
		GSavedCommandLine += TEXT(" ");
		GSavedCommandLine += UTF8_TO_TCHAR(argv[Option]);
	}

	FIOSCommandLineHelper::InitCommandArgs(FString());
	
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("WaitForDebugger")))
    {
        while(!FPlatformMisc::IsDebuggerPresent())
        {
            FPlatformMisc::LowLevelOutputDebugString(TEXT("Waiting for debugger...\n"));
            FPlatformProcess::Sleep(1.f);
        }
        FPlatformMisc::LowLevelOutputDebugString(TEXT("Debugger attached.\n"));
    }
#endif
    
    
	@autoreleasepool {
	    return UIApplicationMain(argc, argv, nil, NSStringFromClass([IOSAppDelegate class]));
	}
}