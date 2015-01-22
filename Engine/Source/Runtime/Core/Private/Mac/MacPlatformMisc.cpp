// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MacPlatformMisc.mm: Mac implementations of misc functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "Misc/App.h"
#include "ExceptionHandling.h"
#include "SecureHash.h"
#include "VarargsHelper.h"
#include "MacApplication.h"
#include "MacCursor.h"
#include "CocoaMenu.h"
#include "CocoaThread.h"
#include "Runtime/Launch/Resources/Version.h"
#include "EngineVersion.h"
#include "MacMallocZone.h"
#include "ApplePlatformSymbolication.h"
#include "MacPlatformCrashContext.h"

#include <dlfcn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <mach-o/dyld.h>
#include <libproc.h>
#include <notify.h>
#include <uuid/uuid.h>


/**
 * Information that cannot be obtained during a signal-handler is initialised here.
 * This ensures that we only call safe functions within the crash reporting handler.
 */
struct MacApplicationInfo
{
	void Init()
	{
		SCOPED_AUTORELEASE_POOL;
		
		NSDictionary* SystemVersion = [NSDictionary dictionaryWithContentsOfFile: @"/System/Library/CoreServices/SystemVersion.plist"];
		OSVersion = FString((NSString*)[SystemVersion objectForKey: @"ProductVersion"]);
		FCStringAnsi::Strcpy(OSVersionUTF8, PATH_MAX+1, [[SystemVersion objectForKey: @"ProductVersion"] UTF8String]);
		OSBuild = FString((NSString*)[SystemVersion objectForKey: @"ProductBuildVersion"]);
		
		TArray<FString> Components;
		OSVersion.ParseIntoArray(&Components, TEXT("."), true);
		uint8 ComponentValues[2] = {0};
		for(uint32 i = 0; i < Components.Num() && i < 2; i++)
		{
			TTypeFromString<uint8>::FromString(ComponentValues[i], *Components[i]);
		}
		RunningOnMavericks = ComponentValues[0] == 10 && ComponentValues[1] == 9;
		
		char TempSysCtlBuffer[PATH_MAX] = {};
		size_t TempSysCtlBufferSize = PATH_MAX;
		
		pid_t ParentPID = getppid();
		proc_pidpath(ParentPID, TempSysCtlBuffer, PATH_MAX);
		ParentProcess = TempSysCtlBuffer;
		
		MachineUUID = TEXT("00000000-0000-0000-0000-000000000000");
		io_service_t PlatformExpert = IOServiceGetMatchingService(kIOMasterPortDefault,IOServiceMatching("IOPlatformExpertDevice"));
		if(PlatformExpert)
		{
			CFTypeRef SerialNumberAsCFString = IORegistryEntryCreateCFProperty(PlatformExpert,CFSTR(kIOPlatformUUIDKey),kCFAllocatorDefault, 0);
			if(SerialNumberAsCFString)
			{
				MachineUUID = FString((NSString*)SerialNumberAsCFString);
				CFRelease(SerialNumberAsCFString);
			}
			IOObjectRelease(PlatformExpert);
		}
		
		sysctlbyname("kern.osrelease", TempSysCtlBuffer, &TempSysCtlBufferSize, NULL, 0);
		BiosRelease = TempSysCtlBuffer;
		uint32 KernelRevision = 0;
		TempSysCtlBufferSize = 4;
		sysctlbyname("kern.osrevision", &KernelRevision, &TempSysCtlBufferSize, NULL, 0);
		BiosRevision = FString::Printf(TEXT("%d"), KernelRevision);
		TempSysCtlBufferSize = PATH_MAX;
		sysctlbyname("kern.uuid", TempSysCtlBuffer, &TempSysCtlBufferSize, NULL, 0);
		BiosUUID = TempSysCtlBuffer;
		TempSysCtlBufferSize = PATH_MAX;
		sysctlbyname("hw.model", TempSysCtlBuffer, &TempSysCtlBufferSize, NULL, 0);
		MachineModel = TempSysCtlBuffer;
		TempSysCtlBufferSize = PATH_MAX+1;
		sysctlbyname("machdep.cpu.brand_string", MachineCPUString, &TempSysCtlBufferSize, NULL, 0);
		
		FMacPlatformMisc::NumberOfCores();
		FMacPlatformMisc::NumberOfCoresIncludingHyperthreads();
		
		AppName = FApp::GetGameName();
		FCStringAnsi::Strcpy(AppNameUTF8, PATH_MAX+1, TCHAR_TO_UTF8(*AppName));
		
		gethostname(MachineName, ARRAY_COUNT(MachineName));
		
		FString CrashVideoPath = FPaths::GameLogDir() + TEXT("CrashVideo.avi");
		
		BranchBaseDir = FString::Printf( TEXT( "%s!%s!%s!%d" ), TEXT( BRANCH_NAME ), FPlatformProcess::BaseDir(), FPlatformMisc::GetEngineMode(), BUILT_FROM_CHANGELIST );
		
		// Get the paths that the files will actually have been saved to
		FString LogDirectory = FPaths::GameLogDir();
		TCHAR CommandlineLogFile[MAX_SPRINTF]=TEXT("");
		
		// Use the log file specified on the commandline if there is one
		CommandLine = FCommandLine::Get();
		if (FParse::Value(*CommandLine, TEXT("LOG="), CommandlineLogFile, ARRAY_COUNT(CommandlineLogFile)))
		{
			LogDirectory += CommandlineLogFile;
		}
		else if (AppName.Len() != 0)
		{
			// If the app name is defined, use it as the log filename
			LogDirectory += FString::Printf(TEXT("%s.Log"), *AppName);
		}
		else
		{
			// Revert to hardcoded UE4.log
			LogDirectory += TEXT("UE4.Log");
		}
		FString LogPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*LogDirectory);
		FCStringAnsi::Strcpy(AppLogPath, PATH_MAX+1, TCHAR_TO_UTF8(*LogPath));

		FString UserCrashVideoPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*CrashVideoPath);
		FCStringAnsi::Strcpy(CrashReportVideo, PATH_MAX+1, TCHAR_TO_UTF8(*UserCrashVideoPath));
		
		// Cache & create the crash report folder.
		FString ReportPath = FPaths::ConvertRelativePathToFull(FString::Printf(TEXT("%s"), *(FPaths::GameAgnosticSavedDir() / TEXT("Crashes"))));
		FCStringAnsi::Strcpy(CrashReportPath, PATH_MAX+1, TCHAR_TO_UTF8(*ReportPath));
		FString ReportClient = FPaths::ConvertRelativePathToFull(FPlatformProcess::GenerateApplicationPath(TEXT("CrashReportClient"), EBuildConfigurations::Development));
		FCStringAnsi::Strcpy(CrashReportClient, PATH_MAX+1, TCHAR_TO_UTF8(*ReportClient));
		IFileManager::Get().MakeDirectory(*ReportPath, true);
		
		AppPath = FPlatformProcess::GenerateApplicationPath(FApp::GetName(), FApp::GetBuildConfiguration());
		
		AppBundleID = FString([[NSBundle mainBundle] bundleIdentifier]);
		
		LCID = FString::Printf(TEXT("%d"), FInternationalization::Get().GetCurrentCulture()->GetLCID());
		
		PrimaryGPU = FPlatformMisc::GetPrimaryGPUBrand();
		
		// Notification handler to check we are running from a battery - this only applies to MacBook's.
		notify_handler_t PowerSourceNotifyHandler = ^(int32 Token){
			RunningOnBattery = false;
			CFTypeRef PowerSourcesInfo = IOPSCopyPowerSourcesInfo();
			if (PowerSourcesInfo)
			{
				CFArrayRef PowerSourcesArray = IOPSCopyPowerSourcesList(PowerSourcesInfo);
				for (CFIndex Index = 0; Index < CFArrayGetCount(PowerSourcesArray); Index++)
				{
					CFTypeRef PowerSource = CFArrayGetValueAtIndex(PowerSourcesArray, Index);
					NSDictionary* Description = (NSDictionary*)IOPSGetPowerSourceDescription(PowerSourcesInfo, PowerSource);
					if ([(NSString*)[Description objectForKey: @kIOPSPowerSourceStateKey] isEqualToString: @kIOPSBatteryPowerValue])
					{
						RunningOnBattery = true;
						break;
					}
				}
				CFRelease(PowerSourcesArray);
				CFRelease(PowerSourcesInfo);
			}
		};
		
		// Call now to fetch the status
		PowerSourceNotifyHandler(0);
		
		uint32 Status = notify_register_dispatch(kIOPSNotifyPowerSource, &PowerSourceNotification, dispatch_get_main_queue(), PowerSourceNotifyHandler);
		check(Status == NOTIFY_STATUS_OK);
		
		NumCores = FPlatformMisc::NumberOfCores();
		
		FPlatformMisc::CreateGuid(RunUUID);
	}
	
	bool RunningOnBattery;
	bool RunningOnMavericks;
	int32 PowerSourceNotification;
	int32 NumCores;
	char AppNameUTF8[PATH_MAX+1];
	char AppLogPath[PATH_MAX+1];
	char CrashReportPath[PATH_MAX+1];
	char CrashReportClient[PATH_MAX+1];
	char CrashReportVideo[PATH_MAX+1];
	char OSVersionUTF8[PATH_MAX+1];
	char MachineName[PATH_MAX+1];
	char MachineCPUString[PATH_MAX+1];
	FString AppPath;
	FString AppName;
	FString AppBundleID;
	FString OSVersion;
	FString OSBuild;
	FString MachineUUID;
	FString MachineModel;
	FString BiosRelease;
	FString BiosRevision;
	FString BiosUUID;
	FString ParentProcess;
	FString LCID;
	FString CommandLine;
	FString BranchBaseDir;
	FString PrimaryGPU;
	FGuid RunUUID;
};
static MacApplicationInfo GMacAppInfo;

UpdateCachedMacMenuStateProc FMacPlatformMisc::UpdateCachedMacMenuState = nullptr;
bool FMacPlatformMisc::bChachedMacMenuStateNeedsUpdate = true;

void FMacPlatformMisc::PlatformPreInit()
{
	FGenericPlatformMisc::PlatformPreInit();
	
	GMacAppInfo.Init();

	// Increase the maximum number of simultaneously open files
	uint32 MaxFilesPerProc = OPEN_MAX;
	size_t UInt32Size = sizeof(uint32);
	sysctlbyname("kern.maxfilesperproc", &MaxFilesPerProc, &UInt32Size, NULL, 0);

	struct rlimit Limit = {MaxFilesPerProc, RLIM_INFINITY};
	int32 Result = getrlimit(RLIMIT_NOFILE, &Limit);
	if (Result == 0)
	{
		if(Limit.rlim_max != RLIM_INFINITY)
		{
			UE_LOG(LogInit, Warning, TEXT("Hard Max File Limit Too Small: %llu, should be RLIM_INFINITY, UE4 may be unstable."), Limit.rlim_max);
		}
		if(Limit.rlim_max == RLIM_INFINITY)
		{
			Limit.rlim_cur = MaxFilesPerProc;
		}
		else
		{
			Limit.rlim_cur = FMath::Min(Limit.rlim_max, (rlim_t)MaxFilesPerProc);
		}
	}
	Result = setrlimit(RLIMIT_NOFILE, &Limit);
	if (Result != 0)
	{
		UE_LOG(LogInit, Warning, TEXT("Failed to change open file limit, UE4 may be unstable."));
	}
}

void FMacPlatformMisc::PlatformInit()
{
	// Identity.
	UE_LOG(LogInit, Log, TEXT("Computer: %s"), FPlatformProcess::ComputerName() );
	UE_LOG(LogInit, Log, TEXT("User: %s"), FPlatformProcess::UserName() );

	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	UE_LOG(LogInit, Log, TEXT("CPU Page size=%i, Cores=%i"), MemoryConstants.PageSize, FPlatformMisc::NumberOfCores() );

	// Timer resolution.
	UE_LOG(LogInit, Log, TEXT("High frequency timer resolution =%f MHz"), 0.000001 / FPlatformTime::GetSecondsPerCycle() );
	
	UE_LOG(LogInit, Log, TEXT("Power Source: %s"), GMacAppInfo.RunningOnBattery ? TEXT(kIOPSBatteryPowerValue) : TEXT(kIOPSACPowerValue) );
}

void FMacPlatformMisc::PlatformPostInit(bool ShowSplashScreen)
{
	// Setup the app menu in menu bar
	const bool bIsBundledApp = [[[NSBundle mainBundle] bundlePath] hasSuffix:@".app"];
	if (bIsBundledApp)
	{
		NSString* AppName = GIsEditor ? @"Unreal Editor" : FString(FApp::GetGameName()).GetNSString();

		SEL ShowAboutSelector = [[NSApp delegate] respondsToSelector:@selector(showAboutWindow:)] ? @selector(showAboutWindow:) : @selector(orderFrontStandardAboutPanel:);
		NSMenuItem* AboutItem = [[[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"About %@", AppName] action:ShowAboutSelector keyEquivalent:@""] autorelease];

		NSMenuItem* PreferencesItem = GIsEditor ? [[[NSMenuItem alloc] initWithTitle:@"Preferences..." action:@selector(showPreferencesWindow:) keyEquivalent:@","] autorelease] : nil;

		NSMenuItem* HideItem = [[[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"Hide %@", AppName] action:@selector(hide:) keyEquivalent:@"h"] autorelease];
		NSMenuItem* HideOthersItem = [[[NSMenuItem alloc] initWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"] autorelease];
		[HideOthersItem setKeyEquivalentModifierMask:NSCommandKeyMask | NSAlternateKeyMask];
		NSMenuItem* ShowAllItem = [[[NSMenuItem alloc] initWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""] autorelease];

		SEL RequestQuitSelector = [[NSApp delegate] respondsToSelector:@selector(requestQuit:)] ? @selector(requestQuit:) : @selector(terminate:);
		NSMenuItem* QuitItem = [[[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"Quit %@", AppName] action:RequestQuitSelector keyEquivalent:@"q"] autorelease];

		NSMenuItem* ServicesItem = [[NSMenuItem new] autorelease];
		FCocoaMenu* ServicesMenu = [[FCocoaMenu new] autorelease];
		[ServicesItem setTitle:@"Services"];
		[ServicesItem setSubmenu:ServicesMenu];
		[NSApp setServicesMenu:ServicesMenu];

		FCocoaMenu* AppMenu = [[FCocoaMenu new] autorelease];
		[AppMenu addItem:AboutItem];
		[AppMenu addItem:[NSMenuItem separatorItem]];
		if (PreferencesItem)
		{
			[AppMenu addItem:PreferencesItem];
			[AppMenu addItem:[NSMenuItem separatorItem]];
		}
		[AppMenu addItem:ServicesItem];
		[AppMenu addItem:[NSMenuItem separatorItem]];
		[AppMenu addItem:HideItem];
		[AppMenu addItem:HideOthersItem];
		[AppMenu addItem:ShowAllItem];
		[AppMenu addItem:[NSMenuItem separatorItem]];
		[AppMenu addItem:QuitItem];

		FCocoaMenu* MenuBar = [[FCocoaMenu new] autorelease];
		NSMenuItem* AppMenuItem = [[NSMenuItem new] autorelease];
		[MenuBar addItem:AppMenuItem];
		[NSApp setMainMenu:MenuBar];
		[AppMenuItem setSubmenu:AppMenu];

		UpdateWindowMenu();
	}
}

void FMacPlatformMisc::UpdateWindowMenu()
{
	NSMenu* WindowMenu = [NSApp windowsMenu];
	if (!WindowMenu)
	{
		WindowMenu = [[FCocoaMenu new] autorelease];
		[WindowMenu setTitle:@"Window"];
		NSMenuItem* WindowMenuItem = [[NSMenuItem new] autorelease];
		[WindowMenuItem setSubmenu:WindowMenu];
		[[NSApp mainMenu] addItem:WindowMenuItem];
		[NSApp setWindowsMenu:WindowMenu];
	}

	NSMenuItem* MinimizeItem = [[[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(miniaturize:) keyEquivalent:@"m"] autorelease];
	NSMenuItem* ZoomItem = [[[NSMenuItem alloc] initWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""] autorelease];
	NSMenuItem* CloseItem = [[[NSMenuItem alloc] initWithTitle:@"Close" action:@selector(performClose:) keyEquivalent:@"w"] autorelease];
	NSMenuItem* BringAllToFrontItem = [[[NSMenuItem alloc] initWithTitle:@"Bring All to Front" action:@selector(arrangeInFront:) keyEquivalent:@""] autorelease];

	[WindowMenu addItem:MinimizeItem];
	[WindowMenu addItem:ZoomItem];
	[WindowMenu addItem:CloseItem];
	[WindowMenu addItem:[NSMenuItem separatorItem]];
	[WindowMenu addItem:BringAllToFrontItem];
	[WindowMenu addItem:[NSMenuItem separatorItem]];
}

void FMacPlatformMisc::ActivateApplication()
{
	MainThreadCall(^{
		[NSApp activateIgnoringOtherApps:YES];
	});
}

bool FMacPlatformMisc::ControlScreensaver(EScreenSaverAction Action)
{
	static uint32 IOPMNoSleepAssertion = 0;
	static bool bDisplaySleepEnabled = true;
	
	switch(Action)
	{
		case EScreenSaverAction::Disable:
		{
			// Prevent display sleep.
			if(bDisplaySleepEnabled)
			{
				SCOPED_AUTORELEASE_POOL;
				
				//  NOTE: IOPMAssertionCreateWithName limits the string to 128 characters.
				FString ReasonForActivity = FString::Printf(TEXT("Running %s"), FApp::GetGameName());
				
				CFStringRef ReasonForActivityCF = (CFStringRef)ReasonForActivity.GetNSString();
				
				IOReturn Success = IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, ReasonForActivityCF, &IOPMNoSleepAssertion);
				bDisplaySleepEnabled = !(Success == kIOReturnSuccess);
				ensure(!bDisplaySleepEnabled);
			}
			break;
		}
		case EScreenSaverAction::Enable:
		{
			// Stop preventing display sleep now that we are done.
			if(!bDisplaySleepEnabled)
			{
				IOReturn Success = IOPMAssertionRelease(IOPMNoSleepAssertion);
				bDisplaySleepEnabled = (Success == kIOReturnSuccess);
				ensure(bDisplaySleepEnabled);
			}
			break;
		}
    }
	
	return true;
}

GenericApplication* FMacPlatformMisc::CreateApplication()
{
	return FMacApplication::CreateMacApplication();
}

void FMacPlatformMisc::GetEnvironmentVariable(const TCHAR* InVariableName, TCHAR* Result, int32 ResultLength)
{
	FString VariableName = InVariableName;
	VariableName.ReplaceInline(TEXT("-"), TEXT("_"));
	ANSICHAR *AnsiResult = getenv(TCHAR_TO_ANSI(*VariableName));
	if (AnsiResult)
	{
		wcsncpy(Result, ANSI_TO_TCHAR(AnsiResult), ResultLength);
	}
	else
	{
		*Result = 0;
	}
}

TArray<uint8> FMacPlatformMisc::GetMacAddress()
{
	TArray<uint8> Result;

	io_iterator_t InterfaceIterator;
	{
		CFMutableDictionaryRef MatchingDict = IOServiceMatching(kIOEthernetInterfaceClass);

		if (!MatchingDict)
		{
			UE_LOG(LogMac, Warning, TEXT("GetMacAddress failed - no Ethernet interfaces"));
			return Result;
		}

		CFMutableDictionaryRef PropertyMatchDict =
			CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

		if (!PropertyMatchDict)
		{
			UE_LOG(LogMac, Warning, TEXT("GetMacAddress failed - can't create CoreFoundation mutable dictionary!"));
			return Result;
		}

		CFDictionarySetValue(PropertyMatchDict, CFSTR(kIOPrimaryInterface), kCFBooleanTrue);
		CFDictionarySetValue(MatchingDict, CFSTR(kIOPropertyMatchKey), PropertyMatchDict);
		CFRelease(PropertyMatchDict);

		if (IOServiceGetMatchingServices(kIOMasterPortDefault, MatchingDict, &InterfaceIterator) != KERN_SUCCESS)
		{
			UE_LOG(LogMac, Warning, TEXT("GetMacAddress failed - error getting matching services"));
			return Result;
		}
	}

	io_object_t InterfaceService;
	while ( (InterfaceService = IOIteratorNext(InterfaceIterator)) != 0)
	{
		io_object_t ControllerService;
		if (IORegistryEntryGetParentEntry(InterfaceService, kIOServicePlane, &ControllerService) == KERN_SUCCESS)
		{
			CFTypeRef MACAddressAsCFData = IORegistryEntryCreateCFProperty(
				ControllerService, CFSTR(kIOMACAddress), kCFAllocatorDefault, 0);
			if (MACAddressAsCFData)
			{
				Result.AddZeroed(kIOEthernetAddressSize);
				CFDataGetBytes((CFDataRef)MACAddressAsCFData, CFRangeMake(0, kIOEthernetAddressSize), Result.GetData());
				break;
				CFRelease(MACAddressAsCFData);
            }
			IOObjectRelease(ControllerService);
        }
		IOObjectRelease(InterfaceService);
	}
	IOObjectRelease(InterfaceIterator);

	return Result;
}

void FMacPlatformMisc::PumpMessages( bool bFromMainLoop )
{
	if( bFromMainLoop )
	{
		ProcessGameThreadEvents();

		if (UpdateCachedMacMenuState && bChachedMacMenuStateNeedsUpdate && MacApplication && !MacApplication->IsProcessingNSEvent() && IsInGameThread())
		{
			UpdateCachedMacMenuState();
			bChachedMacMenuStateNeedsUpdate = false;
		}
	}
}

uint32 FMacPlatformMisc::GetCharKeyMap(uint16* KeyCodes, FString* KeyNames, uint32 MaxMappings)
{
	return FGenericPlatformMisc::GetStandardPrintableKeyMap(KeyCodes, KeyNames, MaxMappings, false, true);
}

uint32 FMacPlatformMisc::GetKeyMap( uint16* KeyCodes, FString* KeyNames, uint32 MaxMappings )
{
#define ADDKEYMAP(KeyCode, KeyName)		if (NumMappings<MaxMappings) { KeyCodes[NumMappings]=KeyCode; KeyNames[NumMappings]=KeyName; ++NumMappings; };

	uint32 NumMappings = 0;

	if ( KeyCodes && KeyNames && (MaxMappings > 0) )
	{
		ADDKEYMAP( kVK_Delete, TEXT("BackSpace") );
		ADDKEYMAP( kVK_Tab, TEXT("Tab") );
		ADDKEYMAP( kVK_Return, TEXT("Enter") );
		ADDKEYMAP( kVK_ANSI_KeypadEnter, TEXT("Enter") );

		ADDKEYMAP( kVK_CapsLock, TEXT("CapsLock") );
		ADDKEYMAP( kVK_Escape, TEXT("Escape") );
		ADDKEYMAP( kVK_Space, TEXT("SpaceBar") );
		ADDKEYMAP( kVK_PageUp, TEXT("PageUp") );
		ADDKEYMAP( kVK_PageDown, TEXT("PageDown") );
		ADDKEYMAP( kVK_End, TEXT("End") );
		ADDKEYMAP( kVK_Home, TEXT("Home") );

		ADDKEYMAP( kVK_LeftArrow, TEXT("Left") );
		ADDKEYMAP( kVK_UpArrow, TEXT("Up") );
		ADDKEYMAP( kVK_RightArrow, TEXT("Right") );
		ADDKEYMAP( kVK_DownArrow, TEXT("Down") );

		ADDKEYMAP( kVK_ForwardDelete, TEXT("Delete") );

		ADDKEYMAP( kVK_ANSI_Keypad0, TEXT("NumPadZero") );
		ADDKEYMAP( kVK_ANSI_Keypad1, TEXT("NumPadOne") );
		ADDKEYMAP( kVK_ANSI_Keypad2, TEXT("NumPadTwo") );
		ADDKEYMAP( kVK_ANSI_Keypad3, TEXT("NumPadThree") );
		ADDKEYMAP( kVK_ANSI_Keypad4, TEXT("NumPadFour") );
		ADDKEYMAP( kVK_ANSI_Keypad5, TEXT("NumPadFive") );
		ADDKEYMAP( kVK_ANSI_Keypad6, TEXT("NumPadSix") );
		ADDKEYMAP( kVK_ANSI_Keypad7, TEXT("NumPadSeven") );
		ADDKEYMAP( kVK_ANSI_Keypad8, TEXT("NumPadEight") );
		ADDKEYMAP( kVK_ANSI_Keypad9, TEXT("NumPadNine") );

		ADDKEYMAP( kVK_ANSI_KeypadMultiply, TEXT("Multiply") );
		ADDKEYMAP( kVK_ANSI_KeypadPlus, TEXT("Add") );
		ADDKEYMAP( kVK_ANSI_KeypadMinus, TEXT("Subtract") );
		ADDKEYMAP( kVK_ANSI_KeypadDecimal, TEXT("Decimal") );
		ADDKEYMAP( kVK_ANSI_KeypadDivide, TEXT("Divide") );

		ADDKEYMAP( kVK_F1, TEXT("F1") );
		ADDKEYMAP( kVK_F2, TEXT("F2") );
		ADDKEYMAP( kVK_F3, TEXT("F3") );
		ADDKEYMAP( kVK_F4, TEXT("F4") );
		ADDKEYMAP( kVK_F5, TEXT("F5") );
		ADDKEYMAP( kVK_F6, TEXT("F6") );
		ADDKEYMAP( kVK_F7, TEXT("F7") );
		ADDKEYMAP( kVK_F8, TEXT("F8") );
		ADDKEYMAP( kVK_F9, TEXT("F9") );
		ADDKEYMAP( kVK_F10, TEXT("F10") );
		ADDKEYMAP( kVK_F11, TEXT("F11") );
		ADDKEYMAP( kVK_F12, TEXT("F12") );

		ADDKEYMAP( MMK_RightControl, TEXT("RightControl") );
		ADDKEYMAP( MMK_LeftControl, TEXT("LeftControl") );
		ADDKEYMAP( MMK_LeftShift, TEXT("LeftShift") );
		ADDKEYMAP( MMK_CapsLock, TEXT("CapsLock") );
		ADDKEYMAP( MMK_LeftAlt, TEXT("LeftAlt") );
		ADDKEYMAP( MMK_LeftCommand, TEXT("LeftCommand") );
		ADDKEYMAP( MMK_RightShift, TEXT("RightShift") );
		ADDKEYMAP( MMK_RightAlt, TEXT("RightAlt") );
		ADDKEYMAP( MMK_RightCommand, TEXT("RightCommand") );
	}

	check(NumMappings < MaxMappings);
	return NumMappings;
}

void FMacPlatformMisc::RequestExit( bool Force )
{
	UE_LOG(LogMac, Log,  TEXT("FPlatformMisc::RequestExit(%i)"), Force );
	
	notify_cancel(GMacAppInfo.PowerSourceNotification);
	GMacAppInfo.PowerSourceNotification = 0;
	
	if( Force )
	{
		// Abort allows signal handler to know we aborted.
		abort();
	}
	else
	{
		// Tell the platform specific code we want to exit cleanly from the main loop.
		GIsRequestingExit = 1;
	}
}

const TCHAR* FMacPlatformMisc::GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error)
{
	// There's no Mac equivalent for GetLastError()
	check(OutBuffer && BufferCount);
	*OutBuffer = TEXT('\0');
	return OutBuffer;
}

void FMacPlatformMisc::ClipboardCopy(const TCHAR* Str)
{
	SCOPED_AUTORELEASE_POOL;

	CFStringRef CocoaString = FPlatformString::TCHARToCFString(Str);
	NSPasteboard *Pasteboard = [NSPasteboard generalPasteboard];
	[Pasteboard clearContents];
	NSPasteboardItem *Item = [[[NSPasteboardItem alloc] init] autorelease];
	[Item setString: (NSString *)CocoaString forType: NSPasteboardTypeString];
	[Pasteboard writeObjects:[NSArray arrayWithObject:Item]];
	CFRelease(CocoaString);
}

void FMacPlatformMisc::ClipboardPaste(class FString& Result)
{
	SCOPED_AUTORELEASE_POOL;

	NSPasteboard *Pasteboard = [NSPasteboard generalPasteboard];
	NSString *CocoaString = [Pasteboard stringForType: NSPasteboardTypeString];
	if (CocoaString)
	{
		TArray<TCHAR> Ch;
		Ch.AddUninitialized([CocoaString length] + 1);
		FPlatformString::CFStringToTCHAR((CFStringRef)CocoaString, Ch.GetData());
		Result = Ch.GetData();
	}
	else
	{
		Result = TEXT("");
	}
}

void FMacPlatformMisc::CreateGuid(FGuid& Result)
{
    uuid_t UUID;
	uuid_generate(UUID);
    
    uint32* Values = (uint32*)(&UUID[0]);
    Result[0] = Values[0];
    Result[1] = Values[1];
    Result[2] = Values[2];
    Result[3] = Values[3];
}

EAppReturnType::Type FMacPlatformMisc::MessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)
{
	SCOPED_AUTORELEASE_POOL;

	if (MacApplication)
	{
		MacApplication->UseMouseCaptureWindow(false);
	}

	EAppReturnType::Type RetValue = MainThreadReturn(^{
		EAppReturnType::Type RetValue = EAppReturnType::Cancel;
		NSInteger Result;

		NSAlert* AlertPanel = [NSAlert new];
		[AlertPanel setInformativeText:FString(Text).GetNSString()];
		[AlertPanel setMessageText:FString(Caption).GetNSString()];

		switch (MsgType)
		{
			case EAppMsgType::Ok:
				[AlertPanel addButtonWithTitle:@"OK"];
				[AlertPanel runModal];
				RetValue = EAppReturnType::Ok;
				break;

			case EAppMsgType::YesNo:
				[AlertPanel addButtonWithTitle:@"Yes"];
				[AlertPanel addButtonWithTitle:@"No"];
				Result = [AlertPanel runModal];
				if (Result == NSAlertFirstButtonReturn)
				{
					RetValue = EAppReturnType::Yes;
				}
				else if (Result == NSAlertSecondButtonReturn)
				{
					RetValue = EAppReturnType::No;
				}
				break;

			case EAppMsgType::OkCancel:
				[AlertPanel addButtonWithTitle:@"OK"];
				[AlertPanel addButtonWithTitle:@"Cancel"];
				Result = [AlertPanel runModal];
				if (Result == NSAlertFirstButtonReturn)
				{
					RetValue = EAppReturnType::Ok;
				}
				else if (Result == NSAlertSecondButtonReturn)
				{
					RetValue = EAppReturnType::Cancel;
				}
				break;

			case EAppMsgType::YesNoCancel:
				[AlertPanel addButtonWithTitle:@"Yes"];
				[AlertPanel addButtonWithTitle:@"No"];
				[AlertPanel addButtonWithTitle:@"Cancel"];
				Result = [AlertPanel runModal];
				if (Result == NSAlertFirstButtonReturn)
				{
					RetValue = EAppReturnType::Yes;
				}
				else if (Result == NSAlertSecondButtonReturn)
				{
					RetValue = EAppReturnType::No;
				}
				else
				{
					RetValue = EAppReturnType::Cancel;
				}
				break;

			case EAppMsgType::CancelRetryContinue:
				[AlertPanel addButtonWithTitle:@"Continue"];
				[AlertPanel addButtonWithTitle:@"Retry"];
				[AlertPanel addButtonWithTitle:@"Cancel"];
				Result = [AlertPanel runModal];
				if (Result == NSAlertFirstButtonReturn)
				{
					RetValue = EAppReturnType::Continue;
				}
				else if (Result == NSAlertSecondButtonReturn)
				{
					RetValue = EAppReturnType::Retry;
				}
				else
				{
					RetValue = EAppReturnType::Cancel;
				}
				break;

			case EAppMsgType::YesNoYesAllNoAll:
				[AlertPanel addButtonWithTitle:@"Yes"];
				[AlertPanel addButtonWithTitle:@"No"];
				[AlertPanel addButtonWithTitle:@"Yes to all"];
				[AlertPanel addButtonWithTitle:@"No to all"];
				Result = [AlertPanel runModal];
				if (Result == NSAlertFirstButtonReturn)
				{
					RetValue = EAppReturnType::Yes;
				}
				else if (Result == NSAlertSecondButtonReturn)
				{
					RetValue = EAppReturnType::No;
				}
				else if (Result == NSAlertThirdButtonReturn)
				{
					RetValue = EAppReturnType::YesAll;
				}
				else
				{
					RetValue = EAppReturnType::NoAll;
				}
				break;

			case EAppMsgType::YesNoYesAllNoAllCancel:
				[AlertPanel addButtonWithTitle:@"Yes"];
				[AlertPanel addButtonWithTitle:@"No"];
				[AlertPanel addButtonWithTitle:@"Yes to all"];
				[AlertPanel addButtonWithTitle:@"No to all"];
				[AlertPanel addButtonWithTitle:@"Cancel"];
				Result = [AlertPanel runModal];
				if (Result == NSAlertFirstButtonReturn)
				{
					RetValue = EAppReturnType::Yes;
				}
				else if (Result == NSAlertSecondButtonReturn)
				{
					RetValue = EAppReturnType::No;
				}
				else if (Result == NSAlertThirdButtonReturn)
				{
					RetValue = EAppReturnType::YesAll;
				}
				else if (Result == NSAlertThirdButtonReturn + 1)
				{
					RetValue = EAppReturnType::NoAll;
				}
				else
				{
					RetValue = EAppReturnType::Cancel;
				}
				break;

			case EAppMsgType::YesNoYesAll:
				[AlertPanel addButtonWithTitle:@"Yes"];
				[AlertPanel addButtonWithTitle:@"No"];
				[AlertPanel addButtonWithTitle:@"Yes to all"];
				Result = [AlertPanel runModal];
				if (Result == NSAlertFirstButtonReturn)
				{
					RetValue = EAppReturnType::Yes;
				}
				else if (Result == NSAlertSecondButtonReturn)
				{
					RetValue = EAppReturnType::No;
				}
				else
				{
					RetValue = EAppReturnType::YesAll;
				}
				break;

			default:
				break;
		}

		[AlertPanel release];

		return RetValue;
	});

	if (MacApplication)
	{
		MacApplication->UseMouseCaptureWindow(true);
	}

	return RetValue;
}

static bool HandleFirstInstall()
{
	if (FParse::Param( FCommandLine::Get(), TEXT("firstinstall")))
	{
		GLog->Flush();

		// Flush config to ensure language changes are written to disk.
		GConfig->Flush(false);

		return false; // terminate the game
	}
	return true; // allow the game to continue;
}

bool FMacPlatformMisc::CommandLineCommands()
{
	return HandleFirstInstall();
}

int32 FMacPlatformMisc::NumberOfCores()
{	
	static int32 NumberOfCores = -1;
	if (NumberOfCores == -1)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("usehyperthreading")))
		{
			NumberOfCores = NumberOfCoresIncludingHyperthreads();
		}
		else
		{
			SIZE_T Size = sizeof(int32);
		
			if (sysctlbyname("hw.physicalcpu", &NumberOfCores, &Size, NULL, 0) != 0)
			{
				NumberOfCores = 1;
			}
		}
	}
	return NumberOfCores;
}

int32 FMacPlatformMisc::NumberOfCoresIncludingHyperthreads()
{
	static int32 NumberOfCores = -1;
	if (NumberOfCores == -1)
	{
		SIZE_T Size = sizeof(int32);
		
		if (sysctlbyname("hw.ncpu", &NumberOfCores, &Size, NULL, 0) != 0)
		{
			NumberOfCores = 1;
		}
	}
	return NumberOfCores;
}

void FMacPlatformMisc::NormalizePath(FString& InPath)
{
	if (InPath.Len() > 1)
	{
		const bool bAppendSlash = InPath[InPath.Len() - 1] == '/'; // NSString will remove the trailing slash, if present, so we need to restore it after conversion
		InPath = [[InPath.GetNSString() stringByStandardizingPath] stringByResolvingSymlinksInPath];
		if (bAppendSlash)
		{
			InPath += TEXT("/");
		}
	}
}

FString FMacPlatformMisc::GetPrimaryGPUBrand()
{
	static FString PrimaryGPU;
	if(PrimaryGPU.IsEmpty())
	{
		// Enumerate the GPUs via IOKit to avoid dragging in OpenGL
		io_iterator_t Iterator;
		CFMutableDictionaryRef MatchDictionary = IOServiceMatching("IOPCIDevice");
		if(IOServiceGetMatchingServices(kIOMasterPortDefault, MatchDictionary, &Iterator) == kIOReturnSuccess)
		{
			io_registry_entry_t ServiceEntry;
			while((ServiceEntry = IOIteratorNext(Iterator)))
			{
				CFMutableDictionaryRef ServiceInfo;
				if(IORegistryEntryCreateCFProperties(ServiceEntry, &ServiceInfo, kCFAllocatorDefault, kNilOptions) == kIOReturnSuccess)
				{
					// GPUs are class-code 0x30000
					const CFDataRef ClassCode = (const CFDataRef)CFDictionaryGetValue(ServiceInfo, CFSTR("class-code"));
					if(ClassCode && CFGetTypeID(ClassCode) == CFDataGetTypeID())
					{
						const uint32* Value = reinterpret_cast<const uint32*>(CFDataGetBytePtr(ClassCode));
						if(Value && *Value == 0x30000)
						{
							// If there are StartupDisplay & hda-gfx entries then this is likely the online display
							const CFDataRef HDAGfx = (const CFDataRef)CFDictionaryGetValue(ServiceInfo, CFSTR("hda-gfx"));
							if(HDAGfx)
							{
								const CFDataRef Model = (const CFDataRef)CFDictionaryGetValue(ServiceInfo, CFSTR("model"));
								if(Model && CFGetTypeID(Model) == CFDataGetTypeID())
								{
									CFStringRef ModelName = CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, Model, kCFStringEncodingASCII);
									// Append all the GPUs together, watching out for malformed Intel strings...
									if ( PrimaryGPU.IsEmpty() )
									{
										PrimaryGPU = FString((NSString*)ModelName).Trim();
									}
									else
									{
										PrimaryGPU += TEXT(" - ");
										PrimaryGPU += FString((NSString*)ModelName).Trim();
									}
									CFRelease(ModelName);
								}
							}
						}
					}
					CFRelease(ServiceInfo);
				}
				IOObjectRelease(ServiceEntry);
			}
			IOObjectRelease(Iterator);
			
			if ( PrimaryGPU.IsEmpty() )
			{
				PrimaryGPU = FGenericPlatformMisc::GetPrimaryGPUBrand();
			}
		}
	}
	return PrimaryGPU;
}

void FMacPlatformMisc::GetOSVersions( FString& out_OSVersionLabel, FString& out_OSSubVersionLabel )
{
	out_OSVersionLabel = GMacAppInfo.OSVersion;
	out_OSSubVersionLabel = GMacAppInfo.OSBuild;
}

#include "ModuleManager.h"

void FMacPlatformMisc::LoadPreInitModules()
{
	FModuleManager::Get().LoadModule(TEXT("OpenGLDrv"));
	FModuleManager::Get().LoadModule(TEXT("CoreAudio"));
}

FLinearColor FMacPlatformMisc::GetScreenPixelColor(const FVector2D& InScreenPos, float InGamma)
{
	SCOPED_AUTORELEASE_POOL;

	CGImageRef ScreenImage = CGWindowListCreateImage(CGRectMake(InScreenPos.X, InScreenPos.Y, 1, 1), kCGWindowListOptionOnScreenBelowWindow, kCGNullWindowID, kCGWindowImageDefault);
    NSBitmapImageRep *BitmapRep = [[[NSBitmapImageRep alloc] initWithCGImage: ScreenImage] autorelease];
    NSImage *Image = [[[NSImage alloc] init] autorelease];
    [Image addRepresentation: BitmapRep];
	[Image lockFocus];
	NSColor* PixelColor = NSReadPixel(NSMakePoint(0.0f, 0.0f));
	[Image unlockFocus];
	CGImageRelease(ScreenImage);

	FLinearColor ScreenColor([PixelColor redComponent], [PixelColor greenComponent], [PixelColor blueComponent], 1.0f);

	if (InGamma > 1.0f)
	{
		// Correct for render gamma
		ScreenColor.R = FMath::Pow(ScreenColor.R, InGamma);
		ScreenColor.G = FMath::Pow(ScreenColor.G, InGamma);
		ScreenColor.B = FMath::Pow(ScreenColor.B, InGamma);
	}
	
	return ScreenColor;
}

FString FMacPlatformMisc::GetCPUVendor()
{
	union
	{
		char Buffer[12+1];
		struct
		{
			int dw0;
			int dw1;
			int dw2;
		} Dw;
	} VendorResult;


	int32 Args[4];
	asm( "cpuid" : "=a" (Args[0]), "=b" (Args[1]), "=c" (Args[2]), "=d" (Args[3]) : "a" (0));

	VendorResult.Dw.dw0 = Args[1];
	VendorResult.Dw.dw1 = Args[3];
	VendorResult.Dw.dw2 = Args[2];
	VendorResult.Buffer[12] = 0;

	return ANSI_TO_TCHAR(VendorResult.Buffer);
}

uint32 FMacPlatformMisc::GetCPUInfo()
{
	uint32 Args[4];
	asm( "cpuid" : "=a" (Args[0]), "=b" (Args[1]), "=c" (Args[2]), "=d" (Args[3]) : "a" (1));

	return Args[0];
}

int32 FMacPlatformMisc::ConvertSlateYPositionToCocoa(int32 YPosition)
{
	NSArray* AllScreens = [NSScreen screens];
	NSScreen* PrimaryScreen = (NSScreen*)[AllScreens objectAtIndex: 0];
	NSRect ScreenFrame = [PrimaryScreen frame];
	NSRect WholeWorkspace = {{0,0},{0,0}};
	for(NSScreen* Screen in AllScreens)
	{
		if(Screen)
		{
			WholeWorkspace = NSUnionRect(WholeWorkspace, [Screen frame]);
		}
	}
	
	const float WholeWorkspaceOrigin = FMath::Min((ScreenFrame.size.height - (WholeWorkspace.origin.y + WholeWorkspace.size.height)), 0.0);
	const float WholeWorkspaceHeight = WholeWorkspace.origin.y + WholeWorkspace.size.height;
	return -((YPosition - WholeWorkspaceOrigin) - WholeWorkspaceHeight + 1);
}


FString FMacPlatformMisc::GetDefaultLocale()
{
	CFLocaleRef loc = CFLocaleCopyCurrent();

	TCHAR langCode[20];
	CFArrayRef langs = CFLocaleCopyPreferredLanguages();
	CFStringRef langCodeStr = (CFStringRef)CFArrayGetValueAtIndex(langs, 0);
	FPlatformString::CFStringToTCHAR(langCodeStr, langCode);

	TCHAR countryCode[20];
	CFStringRef countryCodeStr = (CFStringRef)CFLocaleGetValue(loc, kCFLocaleCountryCode);
	FPlatformString::CFStringToTCHAR(countryCodeStr, countryCode);

	return FString::Printf(TEXT("%s_%s"), langCode, countryCode);
}

FText FMacPlatformMisc::GetFileManagerName()
{
	return NSLOCTEXT("MacPlatform", "FileManagerName", "Finder");
}

bool FMacPlatformMisc::IsRunningOnBattery()
{
	return GMacAppInfo.RunningOnBattery;
}

bool FMacPlatformMisc::IsRunningOnMavericks()
{
	return GMacAppInfo.RunningOnMavericks;
}

int32 FMacPlatformMisc::MacOSXVersionCompare(uint8 Major, uint8 Minor, uint8 Revision)
{
	TArray<FString> Components;
	GMacAppInfo.OSVersion.ParseIntoArray(&Components, TEXT("."), true);
	
	uint8 TargetValues[3] = {Major, Minor, Revision};
	uint8 ComponentValues[3] = {0, 0, 0};
	
	for(uint32 i = 0; i < Components.Num(); i++)
	{
		TTypeFromString<uint8>::FromString(ComponentValues[i], *Components[i]);
	}
	
	for(uint32 i = 0; i < 3; i++)
	{
		if(ComponentValues[i] < TargetValues[i])
		{
			return -1;
		}
		else if(ComponentValues[i] > TargetValues[i])
		{
			return 1;
		}
	}
	
	return 0;
}

/** Global pointer to crash handler */
void (* GCrashHandlerPointer)(const FGenericCrashContext& Context) = NULL;
FMacMallocCrashHandler* GCrashMalloc = nullptr;

/**
 * Good enough default crash reporter.
 */
static void DefaultCrashHandler(FMacCrashContext const& Context)
{
	Context.ReportCrash();
	if (GLog)
	{
		GLog->SetCurrentThreadAsMasterThread();
		GLog->Flush();
	}
	if (GWarn)
	{
		GWarn->Flush();
	}
	if (GError)
	{
		GError->Flush();
		GError->HandleError();
	}
	
	return Context.GenerateCrashInfoAndLaunchReporter();
}

/** True system-specific crash handler that gets called first */
static void PlatformCrashHandler(int32 Signal, siginfo_t* Info, void* Context)
{
	FApplePlatformSymbolication::SetSymbolicationAllowed( false );
	
	FMacCrashContext CrashContext;
	CrashContext.InitFromSignal(Signal, Info, Context);
	
	// Switch to crash handler malloc to avoid malloc reentrancy
	check(GCrashMalloc);
	GCrashMalloc->Enable(&CrashContext, FPlatformTLS::GetCurrentThreadId());
	
	if (GCrashHandlerPointer)
	{
		GCrashHandlerPointer(CrashContext);
	}
	else
	{
		// call default one
		DefaultCrashHandler(CrashContext);
	}
}

/**
 * Handles graceful termination. Gives time to exit gracefully, but second signal will quit immediately.
 */
static void GracefulTerminationHandler(int32 Signal, siginfo_t* Info, void* Context)
{
	// make sure as much data is written to disk as possible
	if (GLog)
	{
		GLog->Flush();
	}
	if (GWarn)
	{
		GWarn->Flush();
	}
	if (GError)
	{
		GError->Flush();
	}
	
	if( !GIsRequestingExit )
	{
		GIsRequestingExit = 1;
	}
	else
	{
		_Exit(0);
	}
}

void FMacPlatformMisc::SetGracefulTerminationHandler()
{
	struct sigaction Action;
	FMemory::Memzero(&Action, sizeof(struct sigaction));
	Action.sa_sigaction = GracefulTerminationHandler;
	sigemptyset(&Action.sa_mask);
	Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
	sigaction(SIGINT, &Action, NULL);
	sigaction(SIGTERM, &Action, NULL);
	sigaction(SIGHUP, &Action, NULL);	//  this should actually cause the server to just re-read configs (restart?)
}

void FMacPlatformMisc::SetCrashHandler(void (* CrashHandler)(const FGenericCrashContext& Context))
{
	GCrashHandlerPointer = CrashHandler;
	
	// Configure the crash handler malloc zone to reserve some VM space for itself
	GCrashMalloc = new FMacMallocCrashHandler( 128 * 1024 * 1024 );
	
	struct sigaction Action;
	FMemory::Memzero(&Action, sizeof(struct sigaction));
	Action.sa_sigaction = PlatformCrashHandler;
	sigemptyset(&Action.sa_mask);
	Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
	sigaction(SIGQUIT, &Action, NULL);	// SIGQUIT is a user-initiated "crash".
	sigaction(SIGILL, &Action, NULL);
	sigaction(SIGEMT, &Action, NULL);
	sigaction(SIGFPE, &Action, NULL);
	sigaction(SIGBUS, &Action, NULL);
	sigaction(SIGSEGV, &Action, NULL);
	sigaction(SIGSYS, &Action, NULL);
	sigaction(SIGABRT, &Action, NULL);
}

void FMacCrashContext::GenerateWindowsErrorReport(char const* WERPath) const
{
	int ReportFile = open(WERPath, O_CREAT|O_WRONLY, 0766);
	if (ReportFile != -1)
	{
		TCHAR Line[PATH_MAX] = {};
		
		// write BOM
		static uint16 ByteOrderMarker = 0xFEFF;
		write(ReportFile, &ByteOrderMarker, sizeof(ByteOrderMarker));
		
		WriteLine(ReportFile, TEXT("<?xml version=\"1.0\" encoding=\"UTF-16\"?>"));
		WriteLine(ReportFile, TEXT("<WERReportMetadata>"));
		
		WriteLine(ReportFile, TEXT("\t<OSVersionInformation>"));
		WriteUTF16String(ReportFile, TEXT("\t\t<WindowsNTVersion>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.OSVersion);
		WriteLine(ReportFile, TEXT("</WindowsNTVersion>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<Build>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.OSVersion);
		WriteUTF16String(ReportFile, TEXT(" ("));
		WriteUTF16String(ReportFile, *GMacAppInfo.OSBuild);
		WriteLine(ReportFile, TEXT(")</Build>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<Product>(0x30): Mac OS X "));
		WriteUTF16String(ReportFile, *GMacAppInfo.OSVersion);
		WriteLine(ReportFile, TEXT("</Product>"));
		
		WriteLine(ReportFile, TEXT("\t\t<Edition>Mac OS X</Edition>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<BuildString>Mac OS X "));
		WriteUTF16String(ReportFile, *GMacAppInfo.OSVersion);
		WriteUTF16String(ReportFile, TEXT(" ("));
		WriteUTF16String(ReportFile, *GMacAppInfo.OSBuild);
		WriteLine(ReportFile, TEXT(")</BuildString>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<Revision>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.OSBuild);
		WriteLine(ReportFile, TEXT("</Revision>"));
		
		WriteLine(ReportFile, TEXT("\t\t<Flavor>Multiprocessor Free</Flavor>"));
		WriteLine(ReportFile, TEXT("\t\t<Architecture>X64</Architecture>"));
		WriteUTF16String(ReportFile, TEXT("\t\t<LCID>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.LCID);
		WriteLine(ReportFile, TEXT("</LCID>"));
		WriteLine(ReportFile, TEXT("\t</OSVersionInformation>"));
		
		WriteLine(ReportFile, TEXT("\t<ParentProcessInformation>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<ParentProcessId>"));
		WriteUTF16String(ReportFile, ItoTCHAR(getppid(), 10));
		WriteLine(ReportFile, TEXT("</ParentProcessId>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<ParentProcessPath>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.ParentProcess);
		WriteLine(ReportFile, TEXT("</ParentProcessPath>"));
		
		WriteLine(ReportFile, TEXT("\t\t<ParentProcessCmdLine></ParentProcessCmdLine>"));	// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t</ParentProcessInformation>"));
		
		WriteLine(ReportFile, TEXT("\t<ProblemSignatures>"));
		WriteLine(ReportFile, TEXT("\t\t<EventType>APPCRASH</EventType>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<Parameter0>UE4-"));
		WriteUTF16String(ReportFile, *GMacAppInfo.AppName);
		WriteLine(ReportFile, TEXT("</Parameter0>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<Parameter1>"));
		WriteUTF16String(ReportFile, ItoTCHAR(ENGINE_MAJOR_VERSION, 10));
		WriteUTF16String(ReportFile, TEXT("."));
		WriteUTF16String(ReportFile, ItoTCHAR(ENGINE_MINOR_VERSION, 10));
		WriteUTF16String(ReportFile, TEXT("."));
		WriteUTF16String(ReportFile, ItoTCHAR(ENGINE_PATCH_VERSION, 10));
		WriteLine(ReportFile, TEXT(".0</Parameter1>"));

		// App time stamp
		WriteLine(ReportFile, TEXT("\t\t<Parameter2>528f2d37</Parameter2>"));													// FIXME: supply valid?
		
		uint64 BackTrace[7] = {0, 0, 0, 0, 0, 0, 0};
		FPlatformStackWalk::CaptureStackBackTrace( BackTrace, 7, Context );
		
		if(BackTrace[6] != 0)
		{
			Dl_info Info;
			dladdr((void*)BackTrace[6], &Info);
			
			// Crash Module name
			WriteUTF16String(ReportFile, TEXT("\t\t<Parameter3>"));
			if (FCStringAnsi::Strlen(Info.dli_fname))
			{
				FMemory::Memzero(Line, PATH_MAX * sizeof(TCHAR));
				FUTF8ToTCHAR_Convert::Convert(Line, PATH_MAX, Info.dli_fname, FCStringAnsi::Strlen(Info.dli_fname));
				WriteUTF16String(ReportFile, Line);
			}
			else
			{
				WriteUTF16String(ReportFile, TEXT("Unknown"));
			}
			WriteLine(ReportFile, TEXT("</Parameter3>"));
			
			// Check header
			uint32 Version = 0;
			uint32 TimeStamp = 0;
			struct mach_header_64* Header = (struct mach_header_64*)Info.dli_fbase;
			struct load_command *CurrentCommand = (struct load_command *)( (char *)Header + sizeof(struct mach_header_64) );
			if( Header->magic == MH_MAGIC_64 )
			{
				for( int32 i = 0; i < Header->ncmds; i++ )
				{
					if( CurrentCommand->cmd == LC_LOAD_DYLIB )
					{
						struct dylib_command *DylibCommand = (struct dylib_command *) CurrentCommand;
						Version = DylibCommand->dylib.current_version;
						TimeStamp = DylibCommand->dylib.timestamp;
						Version = ((Version & 0xff) + ((Version >> 8) & 0xff) * 100 + ((Version >> 16) & 0xffff) * 10000);
						break;
					}
					
					CurrentCommand = (struct load_command *)( (char *)CurrentCommand + CurrentCommand->cmdsize );
				}
			}
			
			// Module version
			WriteUTF16String(ReportFile, TEXT("\t\t<Parameter4>"));
			WriteUTF16String(ReportFile, ItoTCHAR(Version, 10));
			WriteLine(ReportFile, TEXT("</Parameter4>"));
			
			// Module time stamp
			WriteUTF16String(ReportFile, TEXT("\t\t<Parameter5>"));
			WriteUTF16String(ReportFile, ItoTCHAR(TimeStamp, 16));
			WriteLine(ReportFile, TEXT("</Parameter5>"));
			
			// MethodDef token -> no equivalent
			WriteLine(ReportFile, TEXT("\t\t<Parameter6>00000001</Parameter6>"));
			
			// IL Offset -> Function pointer
			WriteUTF16String(ReportFile, TEXT("\t\t<Parameter7>"));
			WriteUTF16String(ReportFile, ItoTCHAR(BackTrace[6], 16));
			WriteLine(ReportFile, TEXT("</Parameter7>"));
		}
		
		// Command line, must match the Windows version.
		WriteUTF16String(ReportFile, TEXT("\t\t<Parameter8>!"));
		WriteUTF16String(ReportFile, FCommandLine::Get());
		WriteLine(ReportFile, TEXT("!</Parameter8>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<Parameter9>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.BranchBaseDir);
		WriteLine(ReportFile, TEXT("</Parameter9>"));
		
		WriteLine(ReportFile, TEXT("\t</ProblemSignatures>"));
		
		WriteLine(ReportFile, TEXT("\t<DynamicSignatures>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<Parameter1>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.BiosUUID);
		WriteLine(ReportFile, TEXT("</Parameter1>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<Parameter2>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.LCID);
		WriteLine(ReportFile, TEXT("</Parameter2>"));
		WriteLine(ReportFile, TEXT("\t</DynamicSignatures>"));
		
		WriteLine(ReportFile, TEXT("\t<SystemInformation>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<MID>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.MachineUUID);
		WriteLine(ReportFile, TEXT("</MID>"));
		
		WriteLine(ReportFile, TEXT("\t\t<SystemManufacturer>Apple Inc.</SystemManufacturer>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<SystemProductName>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.MachineModel);
		WriteLine(ReportFile, TEXT("</SystemProductName>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<BIOSVersion>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.BiosRelease);
		WriteUTF16String(ReportFile, TEXT("-"));
		WriteUTF16String(ReportFile, *GMacAppInfo.BiosRevision);
		WriteLine(ReportFile, TEXT("</BIOSVersion>"));
		
		WriteUTF16String(ReportFile, TEXT("\t\t<GraphicsCard>"));
		WriteUTF16String(ReportFile, *GMacAppInfo.PrimaryGPU);
		WriteLine(ReportFile, TEXT("</GraphicsCard>"));
		
		WriteLine(ReportFile, TEXT("\t</SystemInformation>"));
		
		WriteLine(ReportFile, TEXT("</WERReportMetadata>"));
		
		close(ReportFile);
	}
}

void FMacCrashContext::GenerateMinidump(char const* Path) const
{
	int ReportFile = open(Path, O_CREAT|O_WRONLY, 0766);
	if (ReportFile != -1)
	{
		TCHAR Line[PATH_MAX] = {};
		
		// write BOM
		static uint16 ByteOrderMarker = 0xFEFF;
		write(ReportFile, &ByteOrderMarker, sizeof(ByteOrderMarker));
		
		WriteUTF16String(ReportFile, TEXT("Process:\tUE4-"));
		WriteUTF16String(ReportFile, *GMacAppInfo.AppName);
		WriteUTF16String(ReportFile, TEXT(" ["));
		WriteUTF16String(ReportFile, ItoTCHAR(FPlatformProcess::GetCurrentProcessId(), 10));
		WriteLine(ReportFile, TEXT("]"));
		
		WriteUTF16String(ReportFile, TEXT("Path:\t"));
		WriteLine(ReportFile, *GMacAppInfo.AppPath);
		
		WriteUTF16String(ReportFile, TEXT("Identifier:\t"));
		WriteLine(ReportFile, *GMacAppInfo.AppBundleID);
		
		WriteUTF16String(ReportFile, TEXT("Version:\t"));
		WriteUTF16String(ReportFile, ENGINE_VERSION_STRINGIFY(ENGINE_MAJOR_VERSION) ENGINE_VERSION_TEXT(".") ENGINE_VERSION_STRINGIFY(ENGINE_MINOR_VERSION) ENGINE_VERSION_TEXT(".") ENGINE_VERSION_STRINGIFY(ENGINE_PATCH_VERSION));
		WriteUTF16String(ReportFile, TEXT(" ("));
		WriteUTF16String(ReportFile, ENGINE_VERSION_STRING);
		WriteLine(ReportFile, TEXT(")"));
		
		WriteLine(ReportFile, TEXT("Code Type:\tX86-64 (Native)"));
		
		WriteUTF16String(ReportFile, TEXT("Parent Process:\t"));
		WriteUTF16String(ReportFile, *GMacAppInfo.ParentProcess);
		WriteUTF16String(ReportFile, TEXT(" ["));
		WriteUTF16String(ReportFile, ItoTCHAR(getppid(), 10));
		WriteLine(ReportFile, TEXT("]"));
		
		WriteUTF16String(ReportFile, TEXT("Responsible:\t"));
		WriteUTF16String(ReportFile, *GMacAppInfo.ParentProcess);
		WriteUTF16String(ReportFile, TEXT(" ["));
		WriteUTF16String(ReportFile, ItoTCHAR(getppid(), 10));
		WriteLine(ReportFile, TEXT("]"));
		
		WriteUTF16String(ReportFile, TEXT("User ID:\t"));
		WriteLine(ReportFile, ItoTCHAR(getuid(), 10));
		
		WriteLine(ReportFile, TEXT(""));
		
		WriteUTF16String(ReportFile, TEXT("Date/Time:\t"));
		
		time_t Now;
		time(&Now);
		struct tm LocalTime;
		localtime_r(&Now, &LocalTime);
		wcsftime(Line, PATH_MAX, TEXT("%Y-%m-%d %H:%M:%S %z"), &LocalTime);
		WriteLine(ReportFile, Line);
		
		WriteUTF16String(ReportFile, TEXT("OS Version:\tMac OS X "));
		WriteUTF16String(ReportFile, *GMacAppInfo.OSVersion);
		WriteUTF16String(ReportFile, TEXT(" ("));
		WriteUTF16String(ReportFile, *GMacAppInfo.OSBuild);
		WriteLine(ReportFile, TEXT(")"));
		
		WriteLine(ReportFile, TEXT("Report Version:\t11"));
		
		WriteUTF16String(ReportFile, TEXT("Anonymous UUID:\t"));
		WriteUTF16String(ReportFile, *GMacAppInfo.MachineUUID);
										  
		WriteLine(ReportFile, TEXT(""));
		WriteLine(ReportFile, TEXT(""));
		
		WriteLine(ReportFile, TEXT("Crashed Thread:\t0"));
		WriteLine(ReportFile, TEXT(""));
		
		WriteUTF16String(ReportFile, TEXT("Exception Type:\t"));
		WriteUTF16String(ReportFile, ItoTCHAR(Info->si_errno, 10));
		WriteUTF16String(ReportFile, TEXT(" ("));
		WriteUTF16String(ReportFile, ItoTCHAR(Signal, 10));
		WriteLine(ReportFile, TEXT(")"));
		
		WriteUTF16String(ReportFile, TEXT("Exception Codes:\t"));
		WriteUTF16String(ReportFile, TEXT(" 0x"));
		WriteUTF16String(ReportFile, ItoTCHAR(Info->si_code, 16));
		WriteUTF16String(ReportFile, TEXT(", 0x"));
		WriteLine(ReportFile, ItoTCHAR(Info->si_band, 16));
		
		WriteLine(ReportFile, TEXT("Application Specific Information:"));
		FMemory::Memzero(Line, PATH_MAX * sizeof(TCHAR));
		FUTF8ToTCHAR_Convert::Convert(Line, PATH_MAX, SignalDescription, FCStringAnsi::Strlen(SignalDescription));
		WriteLine(ReportFile, Line);
		
		WriteLine(ReportFile, TEXT(""));
		WriteLine(ReportFile, TEXT("Thread 0:"));
		
		static const int MAX_DEPTH = 100;
		static uint64 StackTrace[MAX_DEPTH];
		FMemory::MemZero( StackTrace );
		FPlatformStackWalk::CaptureStackBackTrace( StackTrace, MAX_DEPTH, Context );
		
		// Skip the first two entries as they are inside the stack walking code.
		Dl_info Info;
		static const int32 IgnoreCount = 6;
		int32 CurrentDepth = IgnoreCount;
		// Allow the first entry to be NULL as the crash could have been caused by a call to a NULL function pointer,
		// which would mean the top of the callstack is NULL.
		while( StackTrace[CurrentDepth] || ( CurrentDepth == IgnoreCount ) )
		{
			WriteUTF16String(ReportFile, ItoTCHAR(CurrentDepth - IgnoreCount, 10));
			WriteUTF16String(ReportFile, TEXT("\t"));
			
			if(dladdr((void*)StackTrace[CurrentDepth], &Info) == 0)
			{
				Info.dli_fbase = nullptr;
				Info.dli_fname = nullptr;
				Info.dli_saddr = nullptr;
				Info.dli_sname = nullptr;
			}
			if(Info.dli_fname && FCStringAnsi::Strrchr(Info.dli_fname, '/'))
			{
				char const* Name = FCStringAnsi::Strrchr(Info.dli_fname, '/');
				Name++;
				FMemory::Memzero(Line, PATH_MAX * sizeof(TCHAR));
				FUTF8ToTCHAR_Convert::Convert(Line, PATH_MAX, Name, FCStringAnsi::Strlen(Name));
				WriteUTF16String(ReportFile, Line);
			}
			else
			{
				WriteUTF16String(ReportFile, TEXT("[Unknown]"));
			}
			WriteUTF16String(ReportFile, TEXT("\t0x"));
			WriteUTF16String(ReportFile, ItoTCHAR(StackTrace[CurrentDepth], 16));
			WriteUTF16String(ReportFile, TEXT(" "));
			if(Info.dli_sname && FCStringAnsi::Strlen(Info.dli_sname))
			{
				FMemory::Memzero(Line, PATH_MAX * sizeof(TCHAR));
				FUTF8ToTCHAR_Convert::Convert(Line, PATH_MAX, Info.dli_sname, FCStringAnsi::Strlen(Info.dli_sname));
				WriteUTF16String(ReportFile, Line);
			}
			else
			{
				WriteUTF16String(ReportFile, TEXT("[Unknown]"));
			}
			WriteUTF16String(ReportFile, TEXT(" + "));
			WriteLine(ReportFile, ItoTCHAR(StackTrace[CurrentDepth] - (uint64)Info.dli_saddr, 10));
			
			CurrentDepth++;
		}
		
		WriteLine(ReportFile, TEXT(""));
		
		WriteLine(ReportFile, TEXT("Binary Images:"));
		uint32 ModuleCount = _dyld_image_count();
		for(uint32 Index = 0; Index < ModuleCount; Index++)
		{
			char const* ModulePath = _dyld_get_image_name(Index);
			uint32 CompatVersion = 0;
			uint32 CurrentVersion = 0;
			uint8 UUID[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
			struct mach_header_64* Header = (struct mach_header_64*)_dyld_get_image_header(Index);
			struct load_command *CurrentCommand = (struct load_command *)( (char *)Header + sizeof(struct mach_header_64) );
			if( Header->magic == MH_MAGIC_64 )
			{
				uint8 Found = 0;
				for( int32 i = 0; i < Header->ncmds && Found < 2; i++ )
				{
					if( CurrentCommand->cmd == LC_LOAD_DYLIB )
					{
						struct dylib_command *DylibCommand = (struct dylib_command *) CurrentCommand;
						CompatVersion = DylibCommand->dylib.compatibility_version;
						CurrentVersion = DylibCommand->dylib.current_version;
						Found++;
					}
					else if ( CurrentCommand->cmd == LC_UUID )
					{
						struct uuid_command* UUIDCommand = (struct uuid_command*)CurrentCommand;
						FMemory::Memcpy(UUID, UUIDCommand->uuid, 16);
						Found++;
					}
					
					CurrentCommand = (struct load_command *)( (char *)CurrentCommand + CurrentCommand->cmdsize );
				}
			}
			
			WriteUTF16String(ReportFile, TEXT("0x"));
			WriteUTF16String(ReportFile, ItoTCHAR((uint64)Header, 16));
			WriteUTF16String(ReportFile, TEXT(" -\t0x"));
			WriteUTF16String(ReportFile, ItoTCHAR((uint64)Header + sizeof(struct mach_header_64) + Header->sizeofcmds, 16));
			WriteUTF16String(ReportFile, TEXT(" "));
			
			char const* Name = FCStringAnsi::Strrchr(ModulePath, '/');
            if ( Name )
            {
                Name++;
            }
            else
            {
                Name = ModulePath;
            }
			FMemory::Memzero(Line, PATH_MAX * sizeof(TCHAR));
			FUTF8ToTCHAR_Convert::Convert(Line, PATH_MAX, Name, FCStringAnsi::Strlen(Name));
			WriteUTF16String(ReportFile, Line);
			WriteUTF16String(ReportFile, TEXT(" ("));
			
			WriteUTF16String(ReportFile, ItoTCHAR(((CompatVersion >> 16) & 0xffff), 10));
			WriteUTF16String(ReportFile, TEXT("."));
			WriteUTF16String(ReportFile, ItoTCHAR(((CompatVersion >> 8) & 0xff), 10));
			WriteUTF16String(ReportFile, TEXT("."));
			WriteUTF16String(ReportFile, ItoTCHAR((CompatVersion & 0xff), 10));
			WriteUTF16String(ReportFile, TEXT(" - "));
			WriteUTF16String(ReportFile, ItoTCHAR(((CurrentVersion >> 16) & 0xffff), 10));
			WriteUTF16String(ReportFile, TEXT("."));
			WriteUTF16String(ReportFile, ItoTCHAR(((CurrentVersion >> 8) & 0xff), 10));
			WriteUTF16String(ReportFile, TEXT("."));
			WriteUTF16String(ReportFile, ItoTCHAR((CurrentVersion & 0xff), 10));
			WriteUTF16String(ReportFile, TEXT(") <"));
			WriteUTF16String(ReportFile, ItoTCHAR(UUID[0], 16, 2));
			WriteUTF16String(ReportFile, ItoTCHAR(UUID[1], 16, 2));
			WriteUTF16String(ReportFile, ItoTCHAR(UUID[2], 16, 2));
			WriteUTF16String(ReportFile, ItoTCHAR(UUID[3], 16, 2));
			WriteUTF16String(ReportFile, TEXT("-"));
			WriteUTF16String(ReportFile, ItoTCHAR(UUID[4], 16, 2));
			WriteUTF16String(ReportFile, ItoTCHAR(UUID[5], 16, 2));
			WriteUTF16String(ReportFile, TEXT("-"));
			WriteUTF16String(ReportFile, ItoTCHAR(UUID[6], 16, 2));
			WriteUTF16String(ReportFile, ItoTCHAR(UUID[7], 16, 2));
			WriteUTF16String(ReportFile, TEXT("-"));
			WriteUTF16String(ReportFile, ItoTCHAR(UUID[8], 16, 2));
			WriteUTF16String(ReportFile, ItoTCHAR(UUID[9], 16, 2));
			WriteUTF16String(ReportFile, TEXT("-"));
			WriteUTF16String(ReportFile, ItoTCHAR(UUID[10], 16, 2));
			WriteUTF16String(ReportFile, ItoTCHAR(UUID[11], 16, 2));
			WriteUTF16String(ReportFile, ItoTCHAR(UUID[12], 16, 2));
			WriteUTF16String(ReportFile, ItoTCHAR(UUID[13], 16, 2));
			WriteUTF16String(ReportFile, ItoTCHAR(UUID[14], 16, 2));
			WriteUTF16String(ReportFile, ItoTCHAR(UUID[15], 16, 2));
			WriteUTF16String(ReportFile, TEXT("> "));
			
			FMemory::Memzero(Line, PATH_MAX * sizeof(TCHAR));
			FUTF8ToTCHAR_Convert::Convert(Line, PATH_MAX, ModulePath, FCStringAnsi::Strlen(ModulePath));
			WriteLine(ReportFile, Line);
		}
		
		WriteLine(ReportFile, TEXT(""));
		
		WriteLine(ReportFile, TEXT("System Profile:"));
		WriteUTF16String(ReportFile, TEXT("Model: "));
		WriteUTF16String(ReportFile, *GMacAppInfo.MachineModel);
		WriteUTF16String(ReportFile, TEXT(", "));
		WriteUTF16String(ReportFile, ItoTCHAR(GMacAppInfo.NumCores, 10));
		WriteUTF16String(ReportFile, TEXT(" processors, "));
		WriteUTF16String(ReportFile, ItoTCHAR(GMacAppInfo.NumCores, 10));
		WriteUTF16String(ReportFile, TEXT("-Core "));

		FMemory::Memzero(Line, PATH_MAX * sizeof(TCHAR));
		FUTF8ToTCHAR_Convert::Convert(Line, PATH_MAX, GMacAppInfo.MachineCPUString, FCStringAnsi::Strlen(GMacAppInfo.MachineCPUString));
		WriteLine(ReportFile, Line);
		
		WriteUTF16String(ReportFile, TEXT("Graphics: "));
		WriteLine(ReportFile, *GMacAppInfo.PrimaryGPU);
		
		close(ReportFile);
	}
}

void FMacCrashContext::GenerateCrashInfoAndLaunchReporter() const
{
	// create a crash-specific directory
	char CrashInfoFolder[PATH_MAX] = {};
	FCStringAnsi::Strncpy(CrashInfoFolder, GMacAppInfo.CrashReportPath, PATH_MAX);
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, "/CrashReport-UE4-");
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, GMacAppInfo.AppNameUTF8);
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, "-pid-");
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(getpid(), 10));
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, "-");
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(GMacAppInfo.RunUUID.A, 16));
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(GMacAppInfo.RunUUID.B, 16));
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(GMacAppInfo.RunUUID.C, 16));
	FCStringAnsi::Strcat(CrashInfoFolder, PATH_MAX, ItoANSI(GMacAppInfo.RunUUID.D, 16));

	// Prevent CrashReportClient from spawning another CrashReportClient.
	const TCHAR* ExecutableName = FPlatformProcess::ExecutableName();
	const bool bCanRunCrashReportClient = FCString::Stristr( ExecutableName, TEXT( "CrashReportClient" ) ) == nullptr;

	if(!mkdir(CrashInfoFolder, 0766) && bCanRunCrashReportClient)
	{
		char FilePath[PATH_MAX] = {};
		FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
		FCStringAnsi::Strcat(FilePath, PATH_MAX, "/report.wer");
		int ReportFile = open(FilePath, O_CREAT|O_WRONLY, 0766);
		if (ReportFile != -1)
		{
			// write BOM
			static uint16 ByteOrderMarker = 0xFEFF;
			write(ReportFile, &ByteOrderMarker, sizeof(ByteOrderMarker));
			
			WriteUTF16String(ReportFile, TEXT("\r\nAppPath="));
			WriteUTF16String(ReportFile, *GMacAppInfo.AppPath);
			WriteLine(ReportFile, TEXT("\r\n"));
			
			close(ReportFile);
		}
		
		// generate "WER"
		FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
		FCStringAnsi::Strcat(FilePath, PATH_MAX, "/wermeta.xml");
		GenerateWindowsErrorReport(FilePath);
		
		// generate "minidump" (Apple crash log format)
		FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
		FCStringAnsi::Strcat(FilePath, PATH_MAX, "/minidump.dmp");
		GenerateMinidump(FilePath);
		
		// generate "info.txt" custom data for our server
		FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
		FCStringAnsi::Strcat(FilePath, PATH_MAX, "/info.txt");
		ReportFile = open(FilePath, O_CREAT|O_WRONLY, 0766);
		if (ReportFile != -1)
		{
			WriteUTF16String(ReportFile, TEXT("GameName UE4-"));
			WriteLine(ReportFile, *GMacAppInfo.AppName);
			
			WriteUTF16String(ReportFile, TEXT("BuildVersion 1.0."));
			WriteUTF16String(ReportFile, ItoTCHAR(ENGINE_VERSION >> 16, 10));
			WriteUTF16String(ReportFile, TEXT("."));
			WriteLine(ReportFile, ItoTCHAR(ENGINE_VERSION & 0xffff, 10));
			
			WriteUTF16String(ReportFile, TEXT("CommandLine "));
			WriteLine(ReportFile, *GMacAppInfo.CommandLine);
			
			WriteUTF16String(ReportFile, TEXT("BaseDir "));
			WriteLine(ReportFile, *GMacAppInfo.BranchBaseDir);
			
			WriteUTF16String(ReportFile, TEXT("MachineGuid "));
			WriteLine(ReportFile, *GMacAppInfo.MachineUUID);
			
			close(ReportFile);
		}
		
		// Introduces a new runtime crash context. Will replace all Windows related crash reporting.
		FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
		FCStringAnsi::Strcat(FilePath, PATH_MAX, "/" );
		FCStringAnsi::Strcat(FilePath, PATH_MAX, FGenericCrashContext::CrashContextRuntimeXMLNameA );
		//SerializeAsXML( FilePath ); @todo uncomment after verification - need to do a bit more work on this for OS X
		
		// copy log
		FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
		FCStringAnsi::Strcat(FilePath, PATH_MAX, "/");
		FCStringAnsi::Strcat(FilePath, PATH_MAX, (!GMacAppInfo.AppName.IsEmpty() ? GMacAppInfo.AppNameUTF8 : "UE4"));
		FCStringAnsi::Strcat(FilePath, PATH_MAX, ".log");
		int LogSrc = open(GMacAppInfo.AppLogPath, O_RDONLY);
		int LogDst = open(FilePath, O_CREAT|O_WRONLY, 0766);
		
		char Data[PATH_MAX] = {};
		int Bytes = 0;
		while((Bytes = read(LogSrc, Data, PATH_MAX)) > 0)
		{
			write(LogDst, Data, Bytes);
		}
		close(LogDst);
		close(LogSrc);
		// best effort, so don't care about result: couldn't copy -> tough, no log
		
		// copy crash video if there is one
		if ( access(GMacAppInfo.CrashReportVideo, R_OK|F_OK) == 0 )
		{
			FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
			FCStringAnsi::Strcat(FilePath, PATH_MAX, "/");
			FCStringAnsi::Strcat(FilePath, PATH_MAX, "CrashVideo.avi");
			int VideoSrc = open(GMacAppInfo.CrashReportVideo, O_RDONLY);
			int VideoDst = open(FilePath, O_CREAT|O_WRONLY, 0766);
			
			int Bytes = 0;
			while((Bytes = read(VideoSrc, Data, PATH_MAX)) > 0)
			{
				write(VideoDst, Data, Bytes);
			}
			close(VideoDst);
			close(VideoSrc);
		}
		
		// try launching the tool and wait for its exit, if at all
		// Use fork() & execl() as they are async-signal safe, CreateProc can fail in Cocoa
		int32 ReturnCode = 0;
		pid_t ForkPID = fork();
		if(ForkPID == 0)
		{
			// Child
			FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
			FCStringAnsi::Strcat(FilePath, PATH_MAX, "/");
			execl(GMacAppInfo.CrashReportClient, "CrashReportClient", FilePath, NULL);
		}
		else
		{
			// Parent
			int StatLoc = 0;
			waitpid(ForkPID, &StatLoc, 0);
		}
	}
	
	_Exit(0);
}
