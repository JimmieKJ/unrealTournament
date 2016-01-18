// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AndroidRuntimeSettingsPrivatePCH.h"

#include "AndroidRuntimeSettings.h"

#if WITH_EDITOR
#include "TargetPlatform.h"
#include "IAndroid_MultiTargetPlatformModule.h"
#endif

UAndroidRuntimeSettings::UAndroidRuntimeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Orientation(EAndroidScreenOrientation::Landscape)
	, bEnableGooglePlaySupport(false)
	, bMultiTargetFormat_ETC1(true)
	, bMultiTargetFormat_ETC2(true)
	, bMultiTargetFormat_DXT(true)
	, bMultiTargetFormat_PVRTC(true)
	, bMultiTargetFormat_ATC(true)
	, bMultiTargetFormat_ASTC(true)
{
}

#if WITH_EDITOR
void UAndroidRuntimeSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Ensure that at least one architecture is supported
	if (!bBuildForArmV7 && !bBuildForX86 && !bBuildForX8664)// && !bBuildForArm64)
	{
		bBuildForArmV7 = true;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, bBuildForArmV7)), GetDefaultConfigFilename());
	}

	// Ensure that at least one GPU architecture is supported
	if (!bBuildForES2 && !bBuildForES31)
	{
		bBuildForES2 = true;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, bBuildForES2)), GetDefaultConfigFilename());
	}

	if (PropertyChangedEvent.Property != nullptr && PropertyChangedEvent.Property->GetName().StartsWith(TEXT("bMultiTargetFormat")))
	{
		UpdateSinglePropertyInConfigFile(PropertyChangedEvent.Property, GetDefaultConfigFilename());

		// Ensure we have at least one format for Android_Multi
		if (!bMultiTargetFormat_ETC1 && !bMultiTargetFormat_ETC2 && !bMultiTargetFormat_DXT && !bMultiTargetFormat_PVRTC && !bMultiTargetFormat_ATC && !bMultiTargetFormat_ASTC)
		{
			bMultiTargetFormat_ETC1 = true;
			UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, bMultiTargetFormat_ETC1)), GetDefaultConfigFilename());
		}

		// Notify the Android_MultiTargetPlatform module if it's loaded
		IAndroid_MultiTargetPlatformModule* Module = FModuleManager::GetModulePtr<IAndroid_MultiTargetPlatformModule>("Android_MultiTargetPlatform");
		if (Module)
		{
			Module->NotifySelectedFormatsChanged();
		}
	}
}

void UAndroidRuntimeSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// If the config has an AdMobAdUnitID then we migrate it on load and clear the value
	if (!AdMobAdUnitID.IsEmpty())
	{
		AdMobAdUnitIDs.Add(AdMobAdUnitID);
		AdMobAdUnitID.Empty();
		UpdateDefaultConfigFile();
	}
}
#endif
