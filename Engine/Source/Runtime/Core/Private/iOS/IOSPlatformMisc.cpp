// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSPlatformMisc.mm: iOS implementations of misc functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "ExceptionHandling.h"
#include "SecureHash.h"
#include "IOSApplication.h"
#include "IOSAppDelegate.h"
#include "IOSView.h"
#include "IOSChunkInstaller.h"
#include "IOSInputInterface.h"

#include "Apple/ApplePlatformCrashContext.h"

/** Amount of free memory in MB reported by the system at starup */
CORE_API int32 GStartupFreeMemoryMB;

/** Global pointer to memory warning handler */
void (* GMemoryWarningHandler)(const FGenericMemoryWarningContext& Context) = NULL;

/** global for showing the splash screen */
bool GShowSplashScreen = true;

static int32 GetFreeMemoryMB()
{
	// get free memory
	vm_size_t PageSize;
	host_page_size(mach_host_self(), &PageSize);

	// get memory stats
	vm_statistics Stats;
	mach_msg_type_number_t StatsSize = sizeof(Stats);
	host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&Stats, &StatsSize);
	return (Stats.free_count * PageSize) / 1024 / 1024;
}

FIOSApplication* FPlatformMisc::CachedApplication = nullptr;

void FIOSPlatformMisc::PlatformInit()
{
	FAppEntry::PlatformInit();

	// Increase the maximum number of simultaneously open files
	struct rlimit Limit;
	Limit.rlim_cur = OPEN_MAX;
	Limit.rlim_max = RLIM_INFINITY;
	int32 Result = setrlimit(RLIMIT_NOFILE, &Limit);
	check(Result == 0);

	// Identity.
	UE_LOG(LogInit, Log, TEXT("Computer: %s"), FPlatformProcess::ComputerName() );
	UE_LOG(LogInit, Log, TEXT("User: %s"), FPlatformProcess::UserName() );

	
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	UE_LOG(LogInit, Log, TEXT("CPU Page size=%i, Cores=%i"), MemoryConstants.PageSize, FPlatformMisc::NumberOfCores() );

	// Timer resolution.
	UE_LOG(LogInit, Log, TEXT("High frequency timer resolution =%f MHz"), 0.000001 / FPlatformTime::GetSecondsPerCycle() );
	GStartupFreeMemoryMB = GetFreeMemoryMB();
	UE_LOG(LogInit, Log, TEXT("Free Memory at startup: %d MB"), GStartupFreeMemoryMB);
}

void FIOSPlatformMisc::PlatformPostInit(bool ShowSplashScreen)
{
    GShowSplashScreen = ShowSplashScreen;
}

GenericApplication* FIOSPlatformMisc::CreateApplication()
{
	CachedApplication = FIOSApplication::CreateIOSApplication();
	return CachedApplication;
}

void FIOSPlatformMisc::GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength)
{
	ANSICHAR *AnsiResult = getenv(TCHAR_TO_ANSI(VariableName));
	if (AnsiResult)
	{
		wcsncpy(Result, ANSI_TO_TCHAR(AnsiResult), ResultLength);
	}
	else
	{
		*Result = 0;
	}
}

// Make sure that SetStoredValue and GetStoredValue generate the same key
static NSString* MakeStoredValueKeyName(const FString& SectionName, const FString& KeyName)
{
	return [NSString stringWithFString:(SectionName + "/" + KeyName)];
}

bool FIOSPlatformMisc::SetStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName, const FString& InValue)
{
	NSUserDefaults* UserSettings = [NSUserDefaults standardUserDefaults];

	// convert input to an NSString
	NSString* StoredValue = [NSString stringWithFString:InValue];

	// store it
	[UserSettings setObject:StoredValue forKey:MakeStoredValueKeyName(InSectionName, InKeyName)];

	return true;
}

bool FIOSPlatformMisc::GetStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName, FString& OutValue)
{
	NSUserDefaults* UserSettings = [NSUserDefaults standardUserDefaults];
	
	// get the stored NSString
	NSString* StoredValue = [UserSettings objectForKey:MakeStoredValueKeyName(InSectionName, InKeyName)];

	// if it was there, convert back to FString
	if (StoredValue != nil)
	{
		OutValue = StoredValue;
		return true;
	}

	return false;
}

bool FIOSPlatformMisc::DeleteStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName)
{
	// No Implementation (currently only used by editor code so not needed on iOS)
	return false;
}

//void FIOSPlatformMisc::LowLevelOutputDebugStringf(const TCHAR *Fmt, ... )
//{
//
//}

void FIOSPlatformMisc::LowLevelOutputDebugString( const TCHAR *Message )
{
	//NsLog will out to all iOS output consoles, instead of just the Xcode console.
	NSLog(@"%@", [NSString stringWithUTF8String:TCHAR_TO_UTF8(Message)]);
}

const TCHAR* FIOSPlatformMisc::GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error)
{
	// There's no iOS equivalent for GetLastError()
	check(OutBuffer && BufferCount);
	*OutBuffer = TEXT('\0');
	return OutBuffer;
}

void FIOSPlatformMisc::ClipboardCopy(const TCHAR* Str)
{
#if !PLATFORM_TVOS
	CFStringRef CocoaString = FPlatformString::TCHARToCFString(Str);
	UIPasteboard* Pasteboard = [UIPasteboard generalPasteboard];
	[Pasteboard setString:(NSString*)CocoaString];
#endif
}

void FIOSPlatformMisc::ClipboardPaste(class FString& Result)
{
#if !PLATFORM_TVOS
	UIPasteboard* Pasteboard = [UIPasteboard generalPasteboard];
	NSString* CocoaString = [Pasteboard string];
	if(CocoaString)
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
#endif
}


FString FIOSPlatformMisc::GetDefaultLocale()
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

EAppReturnType::Type FIOSPlatformMisc::MessageBoxExt( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption )
{
#if UE_BUILD_SHIPPING || PLATFORM_TVOS
	return FGenericPlatformMisc::MessageBoxExt(MsgType, Text, Caption);
#else
	NSString* CocoaText = (NSString*)FPlatformString::TCHARToCFString(Text);
	NSString* CocoaCaption = (NSString*)FPlatformString::TCHARToCFString(Caption);

	NSMutableArray* StringArray = [NSMutableArray arrayWithCapacity:7];

	[StringArray addObject:CocoaCaption];
	[StringArray addObject:CocoaText];

	// Figured that the order of all of these should be the same as their enum name.
	switch (MsgType)
	{
		case EAppMsgType::YesNo:
			[StringArray addObject:@"Yes"];
			[StringArray addObject:@"No"];
			break;
		case EAppMsgType::OkCancel:
			[StringArray addObject:@"Ok"];
			[StringArray addObject:@"Cancel"];
			break;
		case EAppMsgType::YesNoCancel:
			[StringArray addObject:@"Yes"];
			[StringArray addObject:@"No"];
			[StringArray addObject:@"Cancel"];
			break;
		case EAppMsgType::CancelRetryContinue:
			[StringArray addObject:@"Cancel"];
			[StringArray addObject:@"Retry"];
			[StringArray addObject:@"Continue"];
			break;
		case EAppMsgType::YesNoYesAllNoAll:
			[StringArray addObject:@"Yes"];
			[StringArray addObject:@"No"];
			[StringArray addObject:@"Yes To All"];
			[StringArray addObject:@"No To All"];
			break;
		case EAppMsgType::YesNoYesAllNoAllCancel:
			[StringArray addObject:@"Yes"];
			[StringArray addObject:@"No"];
			[StringArray addObject:@"Yes To All"];
			[StringArray addObject:@"No To All"];
			[StringArray addObject:@"Cancel"];
			break;
		default:
			[StringArray addObject:@"Ok"];
			break;
	}

	IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
	
	// reset our response to unset
	AppDelegate.AlertResponse = -1;

	[AppDelegate performSelectorOnMainThread:@selector(ShowAlert:) withObject:StringArray waitUntilDone:NO];

	while (AppDelegate.AlertResponse == -1)
	{
		FPlatformProcess::Sleep(.1);
	}

	EAppReturnType::Type Result = (EAppReturnType::Type)AppDelegate.AlertResponse;

	// Need to remap the return type to the correct one, since AlertResponse actually returns a button index.
	switch (MsgType)
	{
	case EAppMsgType::YesNo:
		Result = (Result == EAppReturnType::No ? EAppReturnType::Yes : EAppReturnType::No);
		break;
	case EAppMsgType::OkCancel:
		// return 1 for Ok, 0 for Cancel
		Result = (Result == EAppReturnType::No ? EAppReturnType::Ok : EAppReturnType::Cancel);
		break;
	case EAppMsgType::YesNoCancel:
		// return 0 for Yes, 1 for No, 2 for Cancel
		if(Result == EAppReturnType::No)
		{
			Result = EAppReturnType::Yes;
		}
		else if(Result == EAppReturnType::Yes)
		{
			Result = EAppReturnType::No;
		}
		else
		{
			Result = EAppReturnType::Cancel;
		}
		break;
	case EAppMsgType::CancelRetryContinue:
		// return 0 for Cancel, 1 for Retry, 2 for Continue
		if(Result == EAppReturnType::No)
		{
			Result = EAppReturnType::Cancel;
		}
		else if(Result == EAppReturnType::Yes)
		{
			Result = EAppReturnType::Retry;
		}
		else
		{
			Result = EAppReturnType::Continue;
		}
		break;
	case EAppMsgType::YesNoYesAllNoAll:
		// return 0 for No, 1 for Yes, 2 for YesToAll, 3 for NoToAll
		break;
	case EAppMsgType::YesNoYesAllNoAllCancel:
		// return 0 for No, 1 for Yes, 2 for YesToAll, 3 for NoToAll, 4 for Cancel
		break;
	default:
		Result = EAppReturnType::Ok;
		break;
	}

	CFRelease((CFStringRef)CocoaCaption);
	CFRelease((CFStringRef)CocoaText);

	return Result;
#endif
}

uint32 FIOSPlatformMisc::GetCharKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings)
{
	return FGenericPlatformMisc::GetStandardPrintableKeyMap(KeyCodes, KeyNames, MaxMappings, true, true);
}

uint32 FIOSPlatformMisc::GetKeyMap( uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings )
{
#define ADDKEYMAP(KeyCode, KeyName)		if (NumMappings<MaxMappings) { KeyCodes[NumMappings]=KeyCode; KeyNames[NumMappings]=KeyName; ++NumMappings; };
	
	uint32 NumMappings = 0;
	
	// we only handle a few "fake" keys from the IOS keyboard delegate stuff in IOSView.cpp
	if (KeyCodes && KeyNames && (MaxMappings > 0))
	{
		ADDKEYMAP(KEYCODE_ENTER, TEXT("Enter"));
		ADDKEYMAP(KEYCODE_BACKSPACE, TEXT("BackSpace"));
		ADDKEYMAP(KEYCODE_ESCAPE, TEXT("Escape"));
	}
	return NumMappings;
}

bool FIOSPlatformMisc::ControlScreensaver(EScreenSaverAction Action)
{
	IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
	[AppDelegate EnableIdleTimer : (Action == FGenericPlatformMisc::Enable)];
	return true;
}

int32 FIOSPlatformMisc::NumberOfCores()
{
	// cache the number of cores
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

#include "ModuleManager.h"

void FIOSPlatformMisc::LoadPreInitModules()
{
	FModuleManager::Get().LoadModule(TEXT("OpenGLDrv"));
	FModuleManager::Get().LoadModule(TEXT("IOSAudio"));
}

void* FIOSPlatformMisc::CreateAutoreleasePool()
{
	return [[NSAutoreleasePool alloc] init];
}

void FIOSPlatformMisc::ReleaseAutoreleasePool(void *Pool)
{
	[(NSAutoreleasePool*)Pool release];
}

void* FIOSPlatformMisc::GetHardwareWindow()
{
	IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
    return AppDelegate.IOSView; 
}

bool FIOSPlatformMisc::HasPlatformFeature(const TCHAR* FeatureName)
{
	if (FCString::Stricmp(FeatureName, TEXT("Metal")) == 0)
	{
		return [IOSAppDelegate GetDelegate].IOSView->bIsUsingMetal;
	}

	return FGenericPlatformMisc::HasPlatformFeature(FeatureName);
}

FIOSPlatformMisc::EIOSDevice FIOSPlatformMisc::GetIOSDeviceType()
{
	// default to unknown
	static EIOSDevice DeviceType = IOS_Unknown;

	// if we've already figured it out, return it
	if (DeviceType != IOS_Unknown)
	{
		return DeviceType;
	}

	// get the device hardware type string length
	size_t DeviceIDLen;
	sysctlbyname("hw.machine", NULL, &DeviceIDLen, NULL, 0);

	// get the device hardware type
	char* DeviceID = (char*)malloc(DeviceIDLen);
	sysctlbyname("hw.machine", DeviceID, &DeviceIDLen, NULL, 0);

	// convert to NSStringt
	NSString *DeviceIDString= [NSString stringWithCString:DeviceID encoding:NSUTF8StringEncoding];
    FString DeviceIDFstring = FString(DeviceIDString);
	free(DeviceID);

	// iPods
	if ([DeviceIDString hasPrefix:@"iPod"])
	{
		// get major revision number
        int Major = FCString::Atoi(&DeviceIDFstring[4]);

		if (Major == 5)
		{
			DeviceType = IOS_IPodTouch5;
		}
		else if (Major >= 7)
		{
			DeviceType = IOS_IPodTouch6;
		}
	}
	// iPads
	else if ([DeviceIDString hasPrefix:@"iPad"])
	{
		// get major revision number
		int Major = FCString::Atoi(&DeviceIDFstring[4]);
		int Minor = FCString::Atoi(&DeviceIDFstring[([DeviceIDString rangeOfString:@","].location + 1)]);

		// iPad2,[1|2|3] is iPad 2 (1 - wifi, 2 - gsm, 3 - cdma)
		if (Major == 2)
		{
			// iPad2,5+ is the new iPadMini, anything higher will use these settings until released
			if (Minor >= 5)
			{
				DeviceType = IOS_IPadMini;
			}
			else
			{
				DeviceType = IOS_IPad2;
			}
		}
		// iPad3,[1|2|3] is iPad 3 and iPad3,4+ is iPad (4th generation)
		else if (Major == 3)
		{
			if (Minor <= 3)
			{
				DeviceType = IOS_IPad3;
			}
			// iPad3,4+ is the new iPad, anything higher will use these settings until released
			else if (Minor >= 4)
			{
				DeviceType = IOS_IPad4;
			}
		}
		// iPadAir and iPad Mini 2nd Generation
		else if (Major == 4)
		{
			if (Minor >= 4)
			{
				DeviceType = IOS_IPadMini2;
			}
			else
			{
				DeviceType = IOS_IPadAir;
			}
		}
		// iPad Air 2 and iPadMini 4
		else if (Major == 5)
		{
			if (Minor == 1 || Minor == 2)
			{
				DeviceType = IOS_IPadMini4;
			}
			else
			{
				DeviceType = IOS_IPadAir2;
			}
		}
		else if (Major == 6)
		{
			if (Minor == 3 || Minor == 4)
			{
				DeviceType = IOS_IPadPro_97;
			}
			else
			{
				DeviceType = IOS_IPadPro_129;
			}
		}
		// Default to highest settings currently available for any future device
		else if (Major > 6)
		{
			DeviceType = IOS_IPadPro;
		}
	}
	// iPhones
	else if ([DeviceIDString hasPrefix:@"iPhone"])
	{
        int Major = FCString::Atoi(&DeviceIDFstring[6]);
        int Minor = FCString::Atoi(&DeviceIDFstring[([DeviceIDString rangeOfString:@","].location + 1)]);

		if (Major == 3)
		{
			DeviceType = IOS_IPhone4;
		}
		else if (Major == 4)
		{
			DeviceType = IOS_IPhone4S;
		}
		else if (Major == 5)
		{
			DeviceType = IOS_IPhone5;
		}
		else if (Major == 6)
		{
			DeviceType = IOS_IPhone5S;
		}
		else if (Major == 7)
		{
			if (Minor == 1)
			{
				DeviceType = IOS_IPhone6Plus;
			}
			else if (Minor == 2)
			{
				DeviceType = IOS_IPhone6;
			}
		}
		else if (Major == 8)
		{
			// note that Apple switched the minor order around between 6 and 6S (gotta keep us on our toes!)
			if (Minor == 1)
			{
				DeviceType = IOS_IPhone6S;
			}
			else if (Minor == 2)
			{
				DeviceType = IOS_IPhone6SPlus;
			}
			else if (Minor == 4)
			{
				DeviceType = IOS_IPhoneSE;
			}
		}
		else if (Major >= 9)
		{
			// for going forward into unknown devices (like 7/7+?), we can't use Minor,
			// so treat devices with a scale > 2.5 to be 6SPlus type devices, < 2.5 to be 6S type devices
			if ([UIScreen mainScreen].scale > 2.5f)
			{
				DeviceType = IOS_IPhone6SPlus;
			}
			else
			{
				DeviceType = IOS_IPhone6S;
			}
		}
	}
	// tvOS
	else if ([DeviceIDString hasPrefix:@"AppleTV"])
	{
		DeviceType = IOS_AppleTV;
	}
	// simulator
	else if ([DeviceIDString hasPrefix:@"x86"])
	{
		// iphone
		if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone)
		{
			CGSize result = [[UIScreen mainScreen] bounds].size;
			if(result.height >= 586)
			{
				DeviceType = IOS_IPhone5;
			}
			else
			{
				DeviceType = IOS_IPhone4S;
			}
		}
		else
		{
			if ([[UIScreen mainScreen] scale] > 1.0f)
			{
				DeviceType = IOS_IPad4;
			}
			else
			{
				DeviceType = IOS_IPad2;
			}
		}
	}


	// if this is unknown at this point, we have a problem
	if (DeviceType == IOS_Unknown)
	{
		UE_LOG(LogInit, Fatal, TEXT("This IOS device type is not supported by UE4 [%s]\n"), *FString(DeviceIDString));
	}

	return DeviceType;
}

void FIOSPlatformMisc::SetMemoryWarningHandler(void (* InHandler)(const FGenericMemoryWarningContext& Context))
{
	GMemoryWarningHandler = InHandler;
}

void FIOSPlatformMisc::HandleLowMemoryWarning()
{
	UE_LOG(LogInit, Log, TEXT("Free Memory at Startup: %d MB"), GStartupFreeMemoryMB);
	UE_LOG(LogInit, Log, TEXT("Free Memory Now       : %d MB"), GetFreeMemoryMB());

	if(GMemoryWarningHandler != NULL)
	{
		FGenericMemoryWarningContext Context;
		GMemoryWarningHandler(Context);
	}
}

bool FIOSPlatformMisc::IsPackagedForDistribution()
{
#if !UE_BUILD_SHIPPING
	static bool PackagingModeCmdLine = FParse::Param(FCommandLine::Get(), TEXT("PACKAGED_FOR_DISTRIBUTION"));
	if (PackagingModeCmdLine)
	{
		return true;
	}
#endif
	NSString* PackagingMode = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"EpicPackagingMode"];
	return PackagingMode != nil && [PackagingMode isEqualToString : @"Distribution"];
}

/**
 * Returns a unique string for device identification
 * 
 * @return the unique string generated by this platform for this device
 */
FString FIOSPlatformMisc::GetUniqueDeviceId()
{
	// Check to see if this OS has this function
	if ([[UIDevice currentDevice] respondsToSelector:@selector(identifierForVendor)])
	{
		NSUUID* Id = [[UIDevice currentDevice] identifierForVendor];
		if (Id != nil)
		{
			NSString* IdfvString = [Id UUIDString];
			FString IDFV(IdfvString);
			return IDFV;
		}
	}
	return FPlatformMisc::GetHashedMacAddressString();
}

class IPlatformChunkInstall* FIOSPlatformMisc::GetPlatformChunkInstall()
{
    static FIOSChunkInstall Singleton;
    return &Singleton;
}


struct FFontHeader
{
	int32 Version;
	uint16 NumTables;
	uint16 SearchRange;
	uint16 EntrySelector;
	uint16 RangeShift;
};

struct FFontTableEntry
{
	uint32 Tag;
	uint32 CheckSum;
	uint32 Offset;
	uint32 Length;
};


static uint32 CalcTableCheckSum(const uint32 *Table, uint32 NumberOfBytesInTable)
{
	uint32 Sum = 0;
	uint32 NumLongs = (NumberOfBytesInTable + 3) / 4;
	while (NumLongs-- > 0)
	{
		Sum += CFSwapInt32HostToBig(*Table++);
	}
	return Sum;
}

static uint32 CalcTableDataRefCheckSum(CFDataRef DataRef)
{
	const uint32 *DataBuff = (const uint32 *)CFDataGetBytePtr(DataRef);
	uint32 DataLength = (uint32)CFDataGetLength(DataRef);
	return CalcTableCheckSum(DataBuff, DataLength);
}

/**
 * In order to get a system font from IOS we need to build one from the data we can gather from a CGFontRef
 * @param InFontName - The name of the system font we are seeking to load.
 * @param OutBytes - The data we have built for the font.
 */
void GetBytesForFont(const NSString* InFontName, OUT TArray<uint8>& OutBytes)
{
	CGFontRef cgFont = CGFontCreateWithFontName((CFStringRef)InFontName);

	if (cgFont)
	{
		CFRetain(cgFont);

		// Gather information on the font tags
		CFArrayRef Tags = CGFontCopyTableTags(cgFont);
		int TableCount = CFArrayGetCount(Tags);

		// Collate the table sizes
		TArray<size_t> TableSizes;

		bool bContainsCFFTable = false;

		size_t TotalSize = sizeof(FFontHeader)+sizeof(FFontTableEntry)* TableCount;
		for (int TableIndex = 0; TableIndex < TableCount; ++TableIndex)
		{
			size_t TableSize = 0;
			
			uint64 aTag = (uint64)CFArrayGetValueAtIndex(Tags, TableIndex);
			if (aTag == 'CFF ' && !bContainsCFFTable)
			{
				bContainsCFFTable = true;
			}

			CFDataRef TableDataRef = CGFontCopyTableForTag(cgFont, aTag);
			if (TableDataRef != NULL)
			{
				TableSize = CFDataGetLength(TableDataRef);
				CFRelease(TableDataRef);
			}

			TotalSize += (TableSize + 3) & ~3;
			TableSizes.Add( TableSize );
		}

		OutBytes.Reserve( TotalSize );
		OutBytes.AddZeroed( TotalSize );

		// Start copying the table data into our buffer
		uint8* DataStart = OutBytes.GetData();
		uint8* DataPtr = DataStart;

		// Compute font header entries
		uint16 EntrySelector = 0;
		uint16 SearchRange = 1;
		while (SearchRange < TableCount >> 1)
		{
			EntrySelector++;
			SearchRange <<= 1;
		}
		SearchRange <<= 4;

		uint16 RangeShift = (TableCount << 4) - SearchRange;

		// Write font header (also called sfnt header, offset subtable)
		FFontHeader* OffsetTable = (FFontHeader*)DataPtr;

		// OpenType Font contains CFF Table use 'OTTO' as version, and with .otf extension
		// otherwise 0001 0000
		OffsetTable->Version = bContainsCFFTable ? 'OTTO' : CFSwapInt16HostToBig(1);
		OffsetTable->NumTables = CFSwapInt16HostToBig((uint16)TableCount);
		OffsetTable->SearchRange = CFSwapInt16HostToBig((uint16)SearchRange);
		OffsetTable->EntrySelector = CFSwapInt16HostToBig((uint16)EntrySelector);
		OffsetTable->RangeShift = CFSwapInt16HostToBig((uint16)RangeShift);

		DataPtr += sizeof(FFontHeader);

		// Write tables
		FFontTableEntry* CurrentTableEntry = (FFontTableEntry*)DataPtr;
		DataPtr += sizeof(FFontTableEntry) * TableCount;

		for (int TableIndex = 0; TableIndex < TableCount; ++TableIndex)
		{
			uint64 aTag = (uint64)CFArrayGetValueAtIndex(Tags, TableIndex);
			CFDataRef TableDataRef = CGFontCopyTableForTag(cgFont, aTag);
			uint32 TableSize = CFDataGetLength(TableDataRef);

			FMemory::Memcpy(DataPtr, CFDataGetBytePtr(TableDataRef), TableSize);

			CurrentTableEntry->Tag = CFSwapInt32HostToBig((uint32_t)aTag);
			CurrentTableEntry->CheckSum = CFSwapInt32HostToBig(CalcTableCheckSum((uint32 *)DataPtr, TableSize));

			uint32 Offset = DataPtr - DataStart;
			CurrentTableEntry->Offset = CFSwapInt32HostToBig((uint32)Offset);
			CurrentTableEntry->Length = CFSwapInt32HostToBig((uint32)TableSize);

			DataPtr += (TableSize + 3) & ~3;
			++CurrentTableEntry;

			CFRelease(TableDataRef);
		}

		CFRelease(cgFont);
	}
}


TArray<uint8> FIOSPlatformMisc::GetSystemFontBytes()
{
#if PLATFORM_TVOS
	NSString* SystemFontName = [UIFont preferredFontForTextStyle:UIFontTextStyleBody].fontName;
#else
	// Gather some details about the system font
	uint32 SystemFontSize = [UIFont systemFontSize];
	NSString* SystemFontName = [UIFont systemFontOfSize:SystemFontSize].fontName;
#endif

	TArray<uint8> FontBytes;
	GetBytesForFont(SystemFontName, FontBytes);

	return FontBytes;
}

TArray<FString> FIOSPlatformMisc::GetPreferredLanguages()
{
	TArray<FString> Results;

	NSArray* Languages = [NSLocale preferredLanguages];
	for (NSString* Language in Languages)
	{
		Results.Add(FString(Language));
	}
	return Results;
}

FString FIOSPlatformMisc::GetLocalCurrencyCode()
{
	return FString([[NSLocale currentLocale] objectForKey:NSLocaleCurrencyCode]);
}

FString FIOSPlatformMisc::GetLocalCurrencySymbol()
{
	return FString([[NSLocale currentLocale] objectForKey:NSLocaleCurrencySymbol]);
}

void FIOSPlatformMisc::RegisterForRemoteNotifications()
{
#if !PLATFORM_TVOS
	UIApplication* application = [UIApplication sharedApplication];
	if ([application respondsToSelector : @selector(registerUserNotifcationSettings:)])
	{
#ifdef __IPHONE_8_0
		UIUserNotificationSettings * settings = [UIUserNotificationSettings settingsForTypes : (UIUserNotificationTypeBadge | UIUserNotificationTypeSound | UIUserNotificationTypeAlert) categories:nil];
		[application registerUserNotificationSettings : settings];
#endif
	}
	else
	{
        
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_8_0
		UIRemoteNotificationType myTypes = UIRemoteNotificationTypeBadge | UIRemoteNotificationTypeAlert | UIRemoteNotificationTypeSound;
		[application registerForRemoteNotificationTypes : myTypes];
#endif
	}
#endif
}

void FIOSPlatformMisc::GetValidTargetPlatforms(TArray<FString>& TargetPlatformNames)
{
	// this is only used to cook with the proper TargetPlatform with COTF, it's not the runtime platform (which is just IOS for both)
#if PLATFORM_TVOS
	TargetPlatformNames.Add(TEXT("TVOS"));
#else
	TargetPlatformNames.Add(FIOSPlatformProperties::PlatformName());
#endif
}

void FIOSPlatformMisc::ResetGamepadAssignments()
{
	UE_LOG(LogIOS, Warning, TEXT("Restting gamepad assignments is not allowed in IOS"))
}

void FIOSPlatformMisc::ResetGamepadAssignmentToController(int32 ControllerId)
{
	
}

bool FIOSPlatformMisc::IsControllerAssignedToGamepad(int32 ControllerId)
{
	FIOSInputInterface* InputInterface = (FIOSInputInterface*)CachedApplication->GetInputInterface();
	return InputInterface->IsControllerAssignedToGamepad(ControllerId);
}
