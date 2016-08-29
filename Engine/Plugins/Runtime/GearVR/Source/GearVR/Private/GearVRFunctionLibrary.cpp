// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//

#include "GearVRPrivatePCH.h"
#include "GearVR.h"
#include "../Public/GearVRFunctionLibrary.h"

UGearVRFunctionLibrary::UGearVRFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}
#if GEARVR_SUPPORTED_PLATFORMS

FGearVR* GetHMD()
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->GetVersionString().Contains(TEXT("GearVR")))
	{
		return static_cast<FGearVR*>(GEngine->HMDDevice.Get());
	}

	return nullptr;
}

#endif

void UGearVRFunctionLibrary::SetCPUAndGPULevels(int CPULevel, int GPULevel)
{
#if GEARVR_SUPPORTED_PLATFORMS
	FGearVR* HMD = GetHMD();
	if (HMD)
	{
		HMD->SetCPUAndGPULevels(CPULevel, GPULevel);
	}
#endif
}

bool UGearVRFunctionLibrary::IsPowerLevelStateMinimum()
{
#if GEARVR_SUPPORTED_PLATFORMS

	FGearVR* HMD = GetHMD();
	if (HMD)
	{
		return HMD->IsPowerLevelStateMinimum();
	}
#endif
	return false;
}

bool UGearVRFunctionLibrary::IsPowerLevelStateThrottled()
{
#if GEARVR_SUPPORTED_PLATFORMS

	FGearVR* HMD = GetHMD();
	if (HMD)
	{
		return HMD->IsPowerLevelStateThrottled();
	}
#endif

	return false;
}

bool UGearVRFunctionLibrary::AreHeadPhonesPluggedIn()
{
#if GEARVR_SUPPORTED_PLATFORMS

	FGearVR* HMD = GetHMD();
	if (HMD)
	{
		return HMD->AreHeadPhonesPluggedIn();
	}
#endif

	return false;
}

float UGearVRFunctionLibrary::GetTemperatureInCelsius()
{
#if GEARVR_SUPPORTED_PLATFORMS

	FGearVR* HMD = GetHMD();
	if (HMD)
	{
		return HMD->GetTemperatureInCelsius();
	}
#endif

	return 0;
}

float UGearVRFunctionLibrary::GetBatteryLevel()
{
#if GEARVR_SUPPORTED_PLATFORMS

	FGearVR* HMD = GetHMD();
	if (HMD)
	{
		return HMD->GetBatteryLevel();
	}
#endif

	return 0;
}