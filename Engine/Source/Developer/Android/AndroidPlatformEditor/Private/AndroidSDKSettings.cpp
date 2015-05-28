// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AndroidPlatformEditorPrivatePCH.h"

//#include "EngineTypes.h"
#include "AndroidSDKSettings.h"

DEFINE_LOG_CATEGORY_STATIC(AndroidSDKSettings, Log, All);

UAndroidSDKSettings::UAndroidSDKSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

#if WITH_EDITOR
void UAndroidSDKSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateTargetModulePaths();	
}

void UAndroidSDKSettings::SetTargetModule(ITargetPlatformManagerModule * InTargetManagerModule)
{
	TargetManagerModule = InTargetManagerModule;
}

void UAndroidSDKSettings::SetDeviceDetection(IAndroidDeviceDetection * InAndroidDeviceDetection)
{
	AndroidDeviceDetection = InAndroidDeviceDetection;
}

void UAndroidSDKSettings::UpdateTargetModulePaths()
{
	TArray<FString> Keys;
	TArray<FString> Values;
	
	if (!SDKPath.Path.IsEmpty())
	{
		FPaths::NormalizeFilename(SDKPath.Path);
		Keys.Add(TEXT("ANDROID_HOME"));
		Values.Add(SDKPath.Path);
	}
	
	if (!NDKPath.Path.IsEmpty())
	{
		FPaths::NormalizeFilename(NDKPath.Path);
		Keys.Add(TEXT("NDKROOT"));
		Values.Add(NDKPath.Path);
	}
	
	if (!ANTPath.Path.IsEmpty())
	{
		FPaths::NormalizeFilename(ANTPath.Path);
		Keys.Add(TEXT("ANT_HOME"));
		Values.Add(ANTPath.Path);
	}

#if PLATFORM_MAC == 0
	if (!JavaPath.Path.IsEmpty())
	{
		FPaths::NormalizeFilename(JavaPath.Path);
		Keys.Add(TEXT("JAVA_HOME"));
		Values.Add(JavaPath.Path);
	}
#endif

	SaveConfig();
	
	if (Keys.Num() != 0)
	{
		TargetManagerModule->UpdatePlatformEnvironment(TEXT("Android"), Keys, Values);
		AndroidDeviceDetection->UpdateADBPath();
	}
}

#endif
