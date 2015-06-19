// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
//
#include "SteamVRPrivatePCH.h"
#include "SteamVRHMD.h"
#include "Classes/SteamVRFunctionLibrary.h"

USteamVRFunctionLibrary::USteamVRFunctionLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

FSteamVRHMD* GetSteamVRHMD()
{
	if (GEngine->HMDDevice.IsValid() && (GEngine->HMDDevice->GetHMDDeviceType() == EHMDDeviceType::DT_SteamVR))
	{
		return static_cast<FSteamVRHMD*>(GEngine->HMDDevice.Get());
	}

	return nullptr;
}

void USteamVRFunctionLibrary::GetValidTrackedDeviceIds(TEnumAsByte<ESteamVRTrackedDeviceType::Type> DeviceType, TArray<int32>& OutTrackedDeviceIds)
{
	OutTrackedDeviceIds.Empty();

	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		SteamVRHMD->GetTrackedDeviceIds(DeviceType, OutTrackedDeviceIds);
	}
}

bool USteamVRFunctionLibrary::GetTrackedDevicePositionAndOrientation(int32 DeviceId, FVector& OutPosition, FRotator& OutOrientation)
{
	bool RetVal = false;

	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		FQuat DeviceOrientation = FQuat::Identity;
		RetVal = SteamVRHMD->GetTrackedObjectOrientationAndPosition(DeviceId, DeviceOrientation, OutPosition);
		OutOrientation = DeviceOrientation.Rotator();
	}

	return RetVal;
}

bool USteamVRFunctionLibrary::GetTrackedDeviceIdFromControllerIndex(int32 ControllerIndex, int32& OutDeviceId)
{
	OutDeviceId = -1;

	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		return SteamVRHMD->GetTrackedDeviceIdFromControllerIndex(ControllerIndex, OutDeviceId);
	}

	return false;
}

void USteamVRFunctionLibrary::SetTrackingSpace(TEnumAsByte<ESteamVRTrackingSpace::Type> NewSpace)
{
	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		SteamVRHMD->SetTrackingSpace(NewSpace);
	}
}

TEnumAsByte<ESteamVRTrackingSpace::Type> USteamVRFunctionLibrary::GetTrackingSpace()
{
	ESteamVRTrackingSpace::Type RetVal = ESteamVRTrackingSpace::Standing;

	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		RetVal = SteamVRHMD->GetTrackingSpace();
	}

	return RetVal;
}