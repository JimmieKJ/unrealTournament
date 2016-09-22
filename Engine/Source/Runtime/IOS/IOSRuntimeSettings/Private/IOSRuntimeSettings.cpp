// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "IOSRuntimeSettingsPrivatePCH.h"

#include "IOSRuntimeSettings.h"

UIOSRuntimeSettings::UIOSRuntimeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bEnableGameCenterSupport = true;
	bEnableCloudKitSupport = false;
	bSupportsPortraitOrientation = true;
	BundleDisplayName = TEXT("UE4 Game");
	BundleName = TEXT("MyUE4Game");
	BundleIdentifier = TEXT("com.YourCompany.GameNameNoSpaces");
	VersionInfo = TEXT("1.0.0");
    FrameRateLock = EPowerUsageFrameRateLock::PUFRL_30;
	bSupportsIPad = true;
	bSupportsIPhone = true;
	MinimumiOSVersion = EIOSVersion::IOS_7;
	EnableRemoteShaderCompile = false;
	bGeneratedSYMFile = false;
	bGeneratedSYMBundle = false;
	bGenerateXCArchive = false;
	bDevForArmV7 = true;
	bDevForArm64 = false;
	bDevForArmV7S = false;
	bShipForArmV7 = true;
	bShipForArm64 = true;
	bShipForArmV7S = false;
	bShipForBitcode = false;
	bUseRSync = true;
	AdditionalPlistData = TEXT("");
	AdditionalLinkerFlags = TEXT("");
	AdditionalShippingLinkerFlags = TEXT("");
	bTreatRemoteAsSeparateController = false;
	bAllowRemoteRotation = true;
	bUseRemoteAsVirtualJoystick = true;
	bUseRemoteAbsoluteDpadValues = false;
}

#if WITH_EDITOR
void UIOSRuntimeSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Ensure that at least one orientation is supported
	if (!bSupportsPortraitOrientation && !bSupportsUpsideDownOrientation && !bSupportsLandscapeLeftOrientation && !bSupportsLandscapeRightOrientation)
	{
		bSupportsPortraitOrientation = true;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bSupportsPortraitOrientation)), GetDefaultConfigFilename());
	}

	// Ensure that at least one API is supported
	if (!bSupportsMetal && !bSupportsOpenGLES2 && !bSupportsMetalMRT)
	{
		bSupportsOpenGLES2 = true;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bSupportsOpenGLES2)), GetDefaultConfigFilename());
	}

	// Ensure that at least armv7 is selected for shipping and dev
	if (!bDevForArmV7 && !bDevForArm64 && !bDevForArmV7S)
	{
		bDevForArmV7 = true;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bDevForArmV7)), GetDefaultConfigFilename());
	}
	if (!bShipForArmV7 && !bShipForArm64 && !bShipForArmV7S)
	{
		bShipForArmV7 = true;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bShipForArmV7)), GetDefaultConfigFilename());
	}
}


void UIOSRuntimeSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// We can have a look for potential keys
	if (!RemoteServerName.IsEmpty() && !RSyncUsername.IsEmpty())
	{
		SSHPrivateKeyLocation = TEXT("");

		const FString DefaultKeyFilename = TEXT("RemoteToolChainPrivate.key");
		const FString RelativeFilePathLocation = FPaths::Combine(TEXT("SSHKeys"), *RemoteServerName, *RSyncUsername, *DefaultKeyFilename);
		TCHAR Path[4096];
		FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"), Path, ARRAY_COUNT(Path));

		TArray<FString> PossibleKeyLocations;
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::GameDir(), TEXT("Build"), TEXT("NotForLicensees"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::GameDir(), TEXT("Build"), TEXT("NoRedist"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::GameDir(), TEXT("Build"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::EngineDir(), TEXT("Build"), TEXT("NotForLicensees"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::EngineDir(), TEXT("Build"), TEXT("NoRedist"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::EngineDir(), TEXT("Build"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(Path, TEXT("Unreal Engine"), TEXT("UnrealBuildTool"), *RelativeFilePathLocation));

		// Find a potential path that we will use if the user hasn't overridden.
		// For information purposes only
		for (const FString& NextLocation : PossibleKeyLocations)
		{
			if (IFileManager::Get().FileSize(*NextLocation) > 0)
			{
				SSHPrivateKeyLocation = NextLocation;
				break;
			}
		}
	}

	// switch IOS_6.1 to IOS_7
	if (MinimumiOSVersion == EIOSVersion::IOS_61)
	{
		MinimumiOSVersion = EIOSVersion::IOS_7;
	}
}
#endif
