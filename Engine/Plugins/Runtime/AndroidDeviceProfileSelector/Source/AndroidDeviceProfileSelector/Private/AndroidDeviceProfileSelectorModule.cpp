// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AndroidDeviceProfileSelectorPrivatePCH.h"
#include "Regex.h"

IMPLEMENT_MODULE(FAndroidDeviceProfileSelectorModule, AndroidDeviceProfileSelector);

UAndroidDeviceProfileMatchingRules::UAndroidDeviceProfileMatchingRules(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void FAndroidDeviceProfileSelectorModule::StartupModule()
{
}

void FAndroidDeviceProfileSelectorModule::ShutdownModule()
{
}

FString const FAndroidDeviceProfileSelectorModule::GetRuntimeDeviceProfileName()
{
	static FString ProfileName; 
	
	if (ProfileName.IsEmpty())
	{
		// Fallback profiles in case we do not match any rules
		ProfileName = FPlatformMisc::GetDefaultDeviceProfileName();
		if (ProfileName.IsEmpty())
		{
			ProfileName = FPlatformProperties::PlatformName();
		}

		FString GPUFamily = FAndroidMisc::GetGPUFamily();
		FString GLVersion = FAndroidMisc::GetGLVersion();
		FString AndroidVersion = FAndroidMisc::GetAndroidVersion();
		FString DeviceMake = FAndroidMisc::GetDeviceMake();
		FString DeviceModel = FAndroidMisc::GetDeviceModel();
		
		// We need to initialize the class early as device profiles need to be evaluated before ProcessNewlyLoadedUObjects can be called.
		extern UClass* Z_Construct_UClass_UAndroidDeviceProfileMatchingRules();
		Z_Construct_UClass_UAndroidDeviceProfileMatchingRules();

		// Get the default object which will has the values from DeviceProfiles.ini
		UAndroidDeviceProfileMatchingRules* Rules = Cast<UAndroidDeviceProfileMatchingRules>(UAndroidDeviceProfileMatchingRules::StaticClass()->GetDefaultObject());
		check(Rules);

		UE_LOG(LogAndroid, Log, TEXT("Checking %d rules from DeviceProfile ini file."), Rules->MatchProfile.Num());
		UE_LOG(LogAndroid, Log, TEXT("  Default profile: %s"), *ProfileName);
		UE_LOG(LogAndroid, Log, TEXT("  GpuFamily: %s"), *GPUFamily);
		UE_LOG(LogAndroid, Log, TEXT("  GlVersion: %s"), *GLVersion);
		UE_LOG(LogAndroid, Log, TEXT("  AndroidVersion: %s"), *AndroidVersion);
		UE_LOG(LogAndroid, Log, TEXT("  DeviceMake: %s"), *DeviceMake);
		UE_LOG(LogAndroid, Log, TEXT("  DeviceModel: %s"), *DeviceModel);

		for (const FProfileMatch& Profile : Rules->MatchProfile)
		{
			FString PreviousRegexMatch;
			bool bFoundMatch = true;
			for (const FProfileMatchItem& Item : Profile.Match)
			{
				FString* SourceString = nullptr;
				switch (Item.SourceType)
				{
				case SRC_PreviousRegexMatch:
					SourceString = &PreviousRegexMatch;
					break;
				case SRC_GpuFamily:
					SourceString = &GPUFamily;
					break;
				case SRC_GlVersion:
					SourceString = &GLVersion;
					break;
				case SRC_AndroidVersion:
					SourceString = &AndroidVersion;
					break;
				case SRC_DeviceMake:
					SourceString = &DeviceMake;
					break;
				case SRC_DeviceModel:
					SourceString = &DeviceModel;
					break;
				default:
					continue;
				}

				switch (Item.CompareType)
				{
				case CMP_Equal:
					if (*SourceString != Item.MatchString)
					{
						bFoundMatch = false;
					}
					break;
				case CMP_Less:
					if (FPlatformString::Atoi(**SourceString) >= FPlatformString::Atoi(*Item.MatchString))
					{
						bFoundMatch = false;
					}
					break;
				case CMP_LessEqual:
					if (FPlatformString::Atoi(**SourceString) > FPlatformString::Atoi(*Item.MatchString))
					{
						bFoundMatch = false;
					}
					break;
				case CMP_Greater:
					if (FPlatformString::Atoi(**SourceString) <= FPlatformString::Atoi(*Item.MatchString))
					{
						bFoundMatch = false;
					}
					break;
				case CMP_GreaterEqual:
					if (FPlatformString::Atoi(**SourceString) < FPlatformString::Atoi(*Item.MatchString))
					{
						bFoundMatch = false;
					}
					break;
				case CMP_NotEqual:
					if (*SourceString == Item.MatchString)
					{
						bFoundMatch = false;
					}
					break;
				case CMP_Regex:
					{
						const FRegexPattern RegexPattern(Item.MatchString);
						FRegexMatcher RegexMatcher(RegexPattern, *SourceString);
						if (RegexMatcher.FindNext())
						{
							PreviousRegexMatch = RegexMatcher.GetCaptureGroup(1);
						}
						else
						{
							bFoundMatch = false;
						}
					}
					break;
				default:
					bFoundMatch = false;
				}

				if (!bFoundMatch)
				{
					break;
				}
			}

			if (bFoundMatch)
			{
				ProfileName = Profile.Profile;
				break;
			}
		}

		UE_LOG(LogAndroid, Log, TEXT("Selected Device Profile: [%s]"), *ProfileName);
	}

	return ProfileName;
}