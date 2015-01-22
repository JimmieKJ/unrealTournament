// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "IOSDeviceProfileSelectorPrivatePCH.h"


IMPLEMENT_MODULE(FIOSDeviceProfileSelectorModule, IOSDeviceProfileSelector);


void FIOSDeviceProfileSelectorModule::StartupModule()
{
}


void FIOSDeviceProfileSelectorModule::ShutdownModule()
{
}


FString const FIOSDeviceProfileSelectorModule::GetRuntimeDeviceProfileName()
{
	FString ProfileName = FPlatformMisc::GetDefaultDeviceProfileName();
	if( ProfileName.IsEmpty() )
	{
		ProfileName = FPlatformProperties::PlatformName();
	}
	UE_LOG(LogIOS, Log, TEXT("Selected Device Profile: [%s]"), *ProfileName);

	// If no type was obtained, just select IOS as default
	return ProfileName;
}
