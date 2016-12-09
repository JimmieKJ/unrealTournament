// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
#include "Classes/SteamVRFunctionLibrary.h"
#include "SteamVRPrivate.h"
#include "SteamVRHMD.h"

USteamVRFunctionLibrary::USteamVRFunctionLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

#if STEAMVR_SUPPORTED_PLATFORMS
FSteamVRHMD* GetSteamVRHMD()
{
	if (GEngine->HMDDevice.IsValid() && (GEngine->HMDDevice->GetHMDDeviceType() == EHMDDeviceType::DT_SteamVR))
	{
		return static_cast<FSteamVRHMD*>(GEngine->HMDDevice.Get());
	}

	return nullptr;
}
#endif // STEAMVR_SUPPORTED_PLATFORMS

void USteamVRFunctionLibrary::GetValidTrackedDeviceIds(ESteamVRTrackedDeviceType DeviceType, TArray<int32>& OutTrackedDeviceIds)
{
#if STEAMVR_SUPPORTED_PLATFORMS
	OutTrackedDeviceIds.Empty();

	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		SteamVRHMD->GetTrackedDeviceIds(DeviceType, OutTrackedDeviceIds);
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS
}

bool USteamVRFunctionLibrary::GetTrackedDevicePositionAndOrientation(int32 DeviceId, FVector& OutPosition, FRotator& OutOrientation)
{
	bool RetVal = false;

#if STEAMVR_SUPPORTED_PLATFORMS
	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		FQuat DeviceOrientation = FQuat::Identity;
		RetVal = SteamVRHMD->GetTrackedObjectOrientationAndPosition(DeviceId, DeviceOrientation, OutPosition);
		OutOrientation = DeviceOrientation.Rotator();
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS

	return RetVal;
}

bool USteamVRFunctionLibrary::GetHandPositionAndOrientation(int32 ControllerIndex, EControllerHand Hand, FVector& OutPosition, FRotator& OutOrientation)
{
	bool RetVal = false;

#if STEAMVR_SUPPORTED_PLATFORMS
	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		FQuat DeviceOrientation = FQuat::Identity;
		RetVal = SteamVRHMD->GetControllerHandPositionAndOrientation(ControllerIndex, Hand, OutPosition, DeviceOrientation);
		OutOrientation = DeviceOrientation.Rotator();
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS

	return RetVal;
}
