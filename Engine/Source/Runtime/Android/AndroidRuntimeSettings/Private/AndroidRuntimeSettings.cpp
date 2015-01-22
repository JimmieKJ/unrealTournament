// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AndroidRuntimeSettingsPrivatePCH.h"

#include "AndroidRuntimeSettings.h"

UAndroidRuntimeSettings::UAndroidRuntimeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Orientation(EAndroidScreenOrientation::Landscape)
	, bEnableGooglePlaySupport(false)
{

}

#if WITH_EDITOR
void UAndroidRuntimeSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Ensure that at least one architecture is supported
	if (!bBuildForArmV7 && !bBuildForX86)// && !bBuildForArm64 && !bBuildForX8664)
	{
		bBuildForArmV7 = true;
	}

	// Ensure that at least one GPU architecture is supported
	if (!bBuildForES2 && !bBuildForES31)
	{
		bBuildForES2 = true;
	}
}
#endif
