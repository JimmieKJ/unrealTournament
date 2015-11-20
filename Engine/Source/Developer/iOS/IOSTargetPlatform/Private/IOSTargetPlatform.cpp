// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSTargetPlatform.cpp: Implements the FIOSTargetPlatform class.
=============================================================================*/

#include "IOSTargetPlatformPrivatePCH.h"
#include "IProjectManager.h"

/* FIOSTargetPlatform structors
 *****************************************************************************/

FIOSTargetPlatform::FIOSTargetPlatform()
{
#if WITH_ENGINE
	FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *PlatformName());
	TextureLODSettings = nullptr; // TextureLODSettings are registered by the device profile.
	StaticMeshLODSettings.Initialize(EngineSettings);
#endif // #if WITH_ENGINE

	// Initialize Ticker for device discovery
	TickDelegate = FTickerDelegate::CreateRaw(this, &FIOSTargetPlatform::HandleTicker);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate, 10.0f);
	
	// initialize the connected device detector
	DeviceHelper.OnDeviceConnected().AddRaw(this, &FIOSTargetPlatform::HandleDeviceConnected);
	DeviceHelper.OnDeviceDisconnected().AddRaw(this, &FIOSTargetPlatform::HandleDeviceDisconnected);
	DeviceHelper.Initialize();
}


FIOSTargetPlatform::~FIOSTargetPlatform()
{
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
}


/* ITargetPlatform interface
 *****************************************************************************/

void FIOSTargetPlatform::EnableDeviceCheck(bool OnOff)
{
	FIOSDeviceHelper::EnableDeviceCheck(OnOff);
}

void FIOSTargetPlatform::GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const
{
	OutDevices.Reset();

	for (auto Iter = Devices.CreateConstIterator(); Iter; ++Iter)
	{
		OutDevices.Add(Iter.Value());
	}
}


ITargetDevicePtr FIOSTargetPlatform::GetDefaultDevice() const
{
	if (Devices.Num() > 0)
	{
		// first device is the default
		auto Iter = Devices.CreateConstIterator();
		if(Iter)
		{
			return Iter.Value();
		}
	}

	return NULL;
}


ITargetDevicePtr FIOSTargetPlatform::GetDevice( const FTargetDeviceId& DeviceId )
{
	return Devices.FindRef(DeviceId);
}


bool FIOSTargetPlatform::IsSdkInstalled(bool bProjectHasCode, FString& OutTutorialPath) const
{
	bool biOSSDKInstalled = true; // @todo How do we check that the iOS SDK is installed when building from Windows? Is that even possible?
#if PLATFORM_MAC
	OutTutorialPath = FString("Shared/Tutorials/InstallingXCodeTutorial");
	biOSSDKInstalled = IFileManager::Get().DirectoryExists(TEXT("/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform"));
#else
	{
		HKEY hKey;
		LRESULT lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared"), 0, KEY_READ, &hKey);
		TCHAR dllPath[256];
		unsigned long pathSize = 256;
		if (lRes != ERROR_SUCCESS || RegQueryValueEx(hKey, TEXT("iTunesMobileDeviceDLL"), 0, NULL, (BYTE*)dllPath, &pathSize) != ERROR_SUCCESS || IFileManager::Get().FileSize(*FString(dllPath)) == INDEX_NONE)
		{
			if (RegQueryValueEx(hKey, TEXT("MobileDeviceDLL"), 0, NULL, (BYTE*)dllPath, &pathSize) != ERROR_SUCCESS || IFileManager::Get().FileSize(*FString(dllPath)) == INDEX_NONE)
			{
				OutTutorialPath = FString("/Engine/Tutorial/Mobile/InstallingiTunesTutorial.InstallingiTunesTutorial");
				biOSSDKInstalled = false;
			}
		}
	}

#endif
	return biOSSDKInstalled;
}

static FString OutputMessage;
static void OnOutput(FString Message)
{
	OutputMessage += Message;
	UE_LOG(LogTemp, Display, TEXT("%s\n"), *Message);
}

int32 FIOSTargetPlatform::CheckRequirements(const FString& ProjectPath, bool bProjectHasCode, FString& OutTutorialPath) const
{
	int32 bReadyToBuild = ETargetPlatformReadyStatus::Ready; // @todo How do we check that the iOS SDK is installed when building from Windows? Is that even possible?
	if (!IsSdkInstalled(bProjectHasCode, OutTutorialPath))
	{
		bReadyToBuild |= ETargetPlatformReadyStatus::SDKNotFound;
	}
#if PLATFORM_MAC
	OutTutorialPath = FString("/Engine/Tutorial/Installation/InstallingXCodeTutorial.InstallingXCodeTutorial");
    // shell to certtool
#else
	if (bProjectHasCode && FRocketSupport::IsRocket())
	{
		OutTutorialPath = FString("/Engine/Tutorial/Mobile/iOSonPCRestrictions.iOSonPCRestrictions");
		bReadyToBuild |= ETargetPlatformReadyStatus::CodeUnsupported;
	}
	if (FRocketSupport::IsRocket() && IProjectManager::Get().IsNonDefaultPluginEnabled())
	{
		OutTutorialPath = FString("/Engine/Tutorial/Mobile/iOSonPCValidPlugins.iOSonPCValidPlugins");
		bReadyToBuild |= ETargetPlatformReadyStatus::PluginsUnsupported;
	}
#endif

	// shell to IPP and get the status of the provision and cert

	FString BundleIdentifier;
	GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("BundleIdentifier"), BundleIdentifier, GEngineIni);
	BundleIdentifier = BundleIdentifier.Replace(TEXT("[PROJECT_NAME]"), FApp::GetGameName());
#if PLATFORM_MAC
    FString CmdExe = TEXT("/bin/sh");
    FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/Mac/RunMono.sh"));
    FString IPPPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNet/IOS/IPhonePackager.exe"));
	FString CommandLine = FString::Printf(TEXT("\"%s\" \"%s\" Validate Engine -project \"%s\" -bundlename \"%s\""), *ScriptPath, *IPPPath, *ProjectPath, *(BundleIdentifier));
#else
	FString CmdExe = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNet/IOS/IPhonePackager.exe"));
	FString CommandLine = FString::Printf(TEXT("Validate Engine -project \"%s\" -bundlename \"%s\""), *ProjectPath, *(BundleIdentifier));
#endif
	TSharedPtr<FMonitoredProcess> IPPProcess = MakeShareable(new FMonitoredProcess(CmdExe, CommandLine, true));
	OutputMessage = TEXT("");
	IPPProcess->OnOutput().BindStatic(&OnOutput);
	IPPProcess->Launch();
	while(IPPProcess->IsRunning())
	{
		FPlatformProcess::Sleep(0.01f);
	}
	int RetCode = IPPProcess->GetReturnCode();
    UE_LOG(LogTemp, Display, TEXT("%s"), *OutputMessage);
	if (RetCode == 14)
	{
		OutTutorialPath = FString("/Engine/Tutorial/Mobile/CreatingInfoPlist.CreatingInfoPlist");
		bReadyToBuild |= ETargetPlatformReadyStatus::ManifestNotFound;
	}
	else if (RetCode == 13)
	{
		OutTutorialPath = FString("/Engine/Tutorial/Mobile/CreatingSigningCertAndProvisionTutorial.CreatingSigningCertAndProvisionTutorial");
		bReadyToBuild |= ETargetPlatformReadyStatus::SigningKeyNotFound;
		bReadyToBuild |= ETargetPlatformReadyStatus::ProvisionNotFound;
	}
	else if (RetCode == 12)
	{
		OutTutorialPath = FString("/Engine/Tutorial/Mobile/CreatingSigningCertAndProvisionTutorial.CreatingSigningCertAndProvisionTutorial");
		bReadyToBuild |= ETargetPlatformReadyStatus::SigningKeyNotFound;
	}
	else if (RetCode == 11)
	{
		OutTutorialPath = FString("/Engine/Tutorial/Mobile/CreatingSigningCertAndProvisionTutorial.CreatingSigningCertAndProvisionTutorial");
		bReadyToBuild |= ETargetPlatformReadyStatus::ProvisionNotFound;
	}

	return bReadyToBuild;
}


/* FIOSTargetPlatform implementation
 *****************************************************************************/

void FIOSTargetPlatform::PingNetworkDevices()
{
	// disabled for now because we find IOS devices from the USB, this is a relic from ULD, but it may be needed in the future
/*	if (!MessageEndpoint.IsValid())
	{
		MessageEndpoint = FMessageEndpoint::Builder("FIOSTargetPlatform")
			.Handling<FIOSLaunchDaemonPong>(this, &FIOSTargetPlatform::HandlePongMessage);
	}

	if (MessageEndpoint.IsValid())
	{
		MessageEndpoint->Publish(new FIOSLaunchDaemonPing(), EMessageScope::Network);
	}

	// remove disconnected & timed out devices
	FDateTime Now = FDateTime::UtcNow();

	for (auto DeviceIt = Devices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		FIOSTargetDevicePtr Device = DeviceIt->Value;

		if (Now > Device->LastPinged + FTimespan::FromSeconds(60.0))
		{
			DeviceIt.RemoveCurrent();
			DeviceLostEvent.Broadcast(Device.ToSharedRef());
		}
	}*/
}


/* FIOSTargetPlatform callbacks
 *****************************************************************************/

void FIOSTargetPlatform::HandlePongMessage( const FIOSLaunchDaemonPong& Message, const IMessageContextRef& Context )
{
	FTargetDeviceId DeviceId;
	FTargetDeviceId::Parse(Message.DeviceID, DeviceId);

	FIOSTargetDevicePtr& Device = Devices.FindOrAdd(DeviceId);

	if (!Device.IsValid())
	{
		Device = MakeShareable(new FIOSTargetDevice(*this));

		Device->SetFeature(ETargetDeviceFeatures::Reboot, Message.bCanReboot);
		Device->SetFeature(ETargetDeviceFeatures::PowerOn, Message.bCanPowerOn);
		Device->SetFeature(ETargetDeviceFeatures::PowerOff, Message.bCanPowerOff);
		Device->SetDeviceId(DeviceId);
		Device->SetDeviceName(Message.DeviceName);
		Device->SetDeviceType(Message.DeviceType);
		Device->SetDeviceEndpoint(Context->GetSender());
		Device->SetIsSimulated(Message.DeviceID.Contains(TEXT("Simulator")));

		DeviceDiscoveredEvent.Broadcast(Device.ToSharedRef());
	}

	Device->LastPinged = FDateTime::UtcNow();
}

void FIOSTargetPlatform::HandleDeviceConnected(const FIOSLaunchDaemonPong& Message)
{
	FTargetDeviceId DeviceId;
	FTargetDeviceId::Parse(Message.DeviceID, DeviceId);
	
	FIOSTargetDevicePtr& Device = Devices.FindOrAdd(DeviceId);
	
	if (!Device.IsValid())
	{
		Device = MakeShareable(new FIOSTargetDevice(*this));
		
		Device->SetFeature(ETargetDeviceFeatures::Reboot, Message.bCanReboot);
		Device->SetFeature(ETargetDeviceFeatures::PowerOn, Message.bCanPowerOn);
		Device->SetFeature(ETargetDeviceFeatures::PowerOff, Message.bCanPowerOff);
		Device->SetDeviceId(DeviceId);
		Device->SetDeviceName(Message.DeviceName);
		Device->SetDeviceType(Message.DeviceType);
		Device->SetIsSimulated(Message.DeviceID.Contains(TEXT("Simulator")));
		
		DeviceDiscoveredEvent.Broadcast(Device.ToSharedRef());
	}
	
	// Add a very long time period to prevent the devices from getting disconnected due to a lack of pong messages
	Device->LastPinged = FDateTime::UtcNow() + FTimespan(100, 0, 0, 0, 0);
}


void FIOSTargetPlatform::HandleDeviceDisconnected(const FIOSLaunchDaemonPong& Message)
{
	FTargetDeviceId DeviceId;
	FTargetDeviceId::Parse(Message.DeviceID, DeviceId);
	
	FIOSTargetDevicePtr& Device = Devices.FindOrAdd(DeviceId);
	
	if (Device.IsValid())
	{
		DeviceLostEvent.Broadcast(Device.ToSharedRef());
		Devices.Remove(DeviceId);
	}
}

bool FIOSTargetPlatform::HandleTicker(float DeltaTime )
{
	PingNetworkDevices();

	return true;
}


/* ITargetPlatform interface
 *****************************************************************************/

static bool SupportsES2()
{
	// default to supporting ES2
	bool bSupportsOpenGLES2 = true;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsOpenGLES2"), bSupportsOpenGLES2, GEngineIni);
	return bSupportsOpenGLES2;
}

static bool SupportsMetal()
{
	// default to NOT supporting metal
	bool bSupportsMetal = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetal"), bSupportsMetal, GEngineIni);
	return bSupportsMetal;
}

static bool SupportsMetalMRT()
{
	// default to NOT supporting metal MRT
	bool bSupportsMetalMRT = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bSupportsMetalMRT, GEngineIni);
	return bSupportsMetalMRT;
}

static bool CookPVRTC()
{
	// default to using PVRTC
	bool bCookPVRTCTextures = true;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bCookPVRTCTextures"), bCookPVRTCTextures, GEngineIni);
	return bCookPVRTCTextures;
}

static bool CookASTC()
{
	// default to not using ASTC
	bool bCookASTCTextures = true;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bCookASTCTextures"), bCookASTCTextures, GEngineIni);
	return bCookASTCTextures;
}

bool FIOSTargetPlatform::SupportsFeature( ETargetPlatformFeatures Feature ) const
{
	switch (Feature)
	{
		case ETargetPlatformFeatures::Packaging:
			return true;
			
		case ETargetPlatformFeatures::LowQualityLightmaps:
			return SupportsES2() || SupportsMetal();
			
		case ETargetPlatformFeatures::HighQualityLightmaps:
			return SupportsMetalMRT();
		default:
			break;
	}
	
	return TTargetPlatformBase<FIOSPlatformProperties>::SupportsFeature(Feature);
}


#if WITH_ENGINE


void FIOSTargetPlatform::GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const
{
	static FName NAME_GLSL_ES2_IOS(TEXT("GLSL_ES2_IOS"));
	static FName NAME_SF_METAL(TEXT("SF_METAL"));
	static FName NAME_SF_METAL_MRT(TEXT("SF_METAL_MRT"));

	if (SupportsES2())
	{
		OutFormats.AddUnique(NAME_GLSL_ES2_IOS);
	}

	if (SupportsMetal())
	{
		OutFormats.AddUnique(NAME_SF_METAL);
	}

	if (SupportsMetalMRT())
	{
		OutFormats.AddUnique(NAME_SF_METAL_MRT);
	}
}

void FIOSTargetPlatform::GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const
{
	GetAllPossibleShaderFormats(OutFormats);
}

void FIOSTargetPlatform::GetTextureFormats( const UTexture* Texture, TArray<FName>& OutFormats ) const
{
	check(Texture);

	// we remap some of the defaults (with PVRTC and ASTC formats)
	static FName FormatRemap[] =
	{
		// original				PVRTC						ASTC
		FName(TEXT("DXT1")),	FName(TEXT("PVRTC2")),		FName(TEXT("ASTC_RGB")),
		FName(TEXT("DXT5")),	FName(TEXT("PVRTC4")),		FName(TEXT("ASTC_RGBA")),
		FName(TEXT("DXT5n")),	FName(TEXT("PVRTCN")),		FName(TEXT("ASTC_NormalAG")),
		FName(TEXT("BC5")),		FName(TEXT("PVRTCN")),		FName(TEXT("ASTC_NormalRG")),
		FName(TEXT("AutoDXT")),	FName(TEXT("AutoPVRTC")),	FName(TEXT("ASTC_RGBAuto")),
	};
	static FName NameBGRA8(TEXT("BGRA8"));
	static FName NamePOTERROR(TEXT("POTERROR"));

	FName TextureFormatName = NAME_None;

	// forward rendering only needs one channel for shadow maps
	if (Texture->LODGroup == TEXTUREGROUP_Shadowmap && !SupportsMetalMRT())
	{
		TextureFormatName = FName(TEXT("G8"));
	}

	// if we didn't assign anything specially, then use the defaults
	if (TextureFormatName == NAME_None)
	{
		TextureFormatName = GetDefaultTextureFormatName(Texture, EngineSettings, false);
	}

	// perform any remapping away from defaults
	bool bFoundRemap = false;
	bool bIncludePVRTC = CookPVRTC();
	bool bIncludeASTC = CookASTC();
	for (int32 RemapIndex = 0; RemapIndex < ARRAY_COUNT(FormatRemap); RemapIndex += 3)
	{
		if (TextureFormatName == FormatRemap[RemapIndex])
		{
			// we found a remapping
			bFoundRemap = true;
			// include the formats we want (use ASTC first so that it is preferred at runtime if they both exist and it's supported)
			if (bIncludeASTC)
			{
				OutFormats.AddUnique(FormatRemap[RemapIndex + 2]);
			}
			if (bIncludePVRTC)
			{
				// handle non-power of 2 textures
				if (!Texture->Source.IsPowerOfTwo())
				{
					// option 1: Uncompress, but users will get very large textures unknowningly
					// OutFormats.AddUnique(NameBGRA8);
					// option 2: Use an "error message" texture so they see it in game
					OutFormats.AddUnique(NamePOTERROR);
				}
				else
				{
					OutFormats.AddUnique(FormatRemap[RemapIndex + 1]);
				}
			}
		}
	}

	// if we didn't already remap above, add it now
	if (!bFoundRemap)
	{
		OutFormats.Add(TextureFormatName);
	}
}


const UTextureLODSettings& FIOSTargetPlatform::GetTextureLODSettings() const
{
	return *TextureLODSettings;
}


FName FIOSTargetPlatform::GetWaveFormat( const class USoundWave* Wave ) const
{
	static FName NAME_ADPCM(TEXT("ADPCM"));
	return NAME_ADPCM;
}

#endif // WITH_ENGINE

