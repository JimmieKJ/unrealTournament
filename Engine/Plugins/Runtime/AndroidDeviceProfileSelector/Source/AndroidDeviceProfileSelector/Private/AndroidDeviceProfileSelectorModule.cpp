// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AndroidDeviceProfileSelectorPrivatePCH.h"

IMPLEMENT_MODULE(FAndroidDeviceProfileSelectorModule, AndroidDeviceProfileSelector);


void FAndroidDeviceProfileSelectorModule::StartupModule()
{
}


void FAndroidDeviceProfileSelectorModule::ShutdownModule()
{
}


FString const FAndroidDeviceProfileSelectorModule::GetRuntimeDeviceProfileName()
{
	FString ProfileName = FPlatformMisc::GetDefaultDeviceProfileName();
	if( ProfileName.IsEmpty() )
	{
		ProfileName = FPlatformProperties::PlatformName();
	}

	FString GPUFamily = FAndroidMisc::GetGPUFamily();

	UE_LOG(LogAndroid, Log,TEXT("Default profile:%s GPUFamily:%s"),*ProfileName,*GPUFamily);

	if (FCString::Stricmp(*GPUFamily, TEXT("PowerVR SGX 540")) == 0)
	{
		ProfileName = TEXT("Android_PowerVR540");
	}
	else if (FCString::Stricmp(*GPUFamily, TEXT("Adreno (TM) 200")) == 0)
	{
		ProfileName = TEXT("Android_Adreno200");
	}
	else if (FCString::Stricmp(*GPUFamily, TEXT("Adreno (TM) 203")) == 0)
	{
		ProfileName = TEXT("Android_Adreno203");
	}
	else if (FCString::Stricmp(*GPUFamily, TEXT("Adreno (TM) 205")) == 0)
	{
		ProfileName = TEXT("Android_Adreno205");
	}
	else if (FCString::Stricmp(*GPUFamily, TEXT("Adreno (TM) 220")) == 0)
	{
		ProfileName = TEXT("Android_Adreno220");
	}
	else if (FCString::Stricmp(*GPUFamily, TEXT("Adreno (TM) 225")) == 0)
	{
		ProfileName = TEXT("Android_Adreno225");
	}
	else if (FCString::Stricmp(*GPUFamily, TEXT("Adreno (TM) 320")) == 0)
	{
		ProfileName = TEXT("Android_Adreno320");
	}
	else if (FCString::Stricmp(*GPUFamily, TEXT("Adreno (TM) 330")) == 0)
	{
		FString GLVersion = FAndroidMisc::GetGLVersion();
		const TCHAR* Version = FCString::Stristr(*GLVersion, TEXT("ES 3.0 V@"));
		int32 VersionNumber = 0;
		if ( Version )
		{
			VersionNumber = FCString::Atoi(Version + 9);
		}
		if ( VersionNumber >= 53 )
		{
			ProfileName = TEXT("Android_Adreno330_Ver53");
		}
		else
		{
			ProfileName = TEXT("Android_Adreno330");
		}
	}
	else if (FCString::Stricmp(*GPUFamily, TEXT("NVIDIA Tegra")) == 0)
	{
		FString GLVersion = FAndroidMisc::GetGLVersion();

		if(GLVersion.StartsWith(TEXT("4.")))
		{
			ProfileName = TEXT("Android_TegraK1_GL4");
		}
		else if (GLVersion.StartsWith(TEXT("OpenGL ES 3.")))
		{
			ProfileName = TEXT("Android_TegraK1");
		}
		else if (GLVersion.StartsWith(TEXT("OpenGL ES 2.")))
		{
			ProfileName = TEXT("Android_Tegra4");
		}
	}


	UE_LOG(LogAndroid, Log, TEXT("Selected Device Profile: [%s]"), *ProfileName);

	// If no type was obtained, just select IOS as default
	return ProfileName;
}