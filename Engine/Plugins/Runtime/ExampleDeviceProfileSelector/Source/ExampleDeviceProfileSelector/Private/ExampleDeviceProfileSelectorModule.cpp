// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ExampleDeviceProfileSelectorPrivatePCH.h"


IMPLEMENT_MODULE(FExampleDeviceProfileSelectorModule, ExampleDeviceProfileSelector);


void FExampleDeviceProfileSelectorModule::StartupModule()
{
}


void FExampleDeviceProfileSelectorModule::ShutdownModule()
{
}


const FString FExampleDeviceProfileSelectorModule::GetRuntimeDeviceProfileName()
{
	FString ProfileName = FPlatformProperties::PlatformName();
	UE_LOG(LogInit, Log, TEXT("Selected Device Profile: [%s]"), *ProfileName);
	return ProfileName;
}
