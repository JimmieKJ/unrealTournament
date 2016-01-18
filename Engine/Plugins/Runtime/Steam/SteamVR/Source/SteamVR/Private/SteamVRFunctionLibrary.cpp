// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
//
#include "SteamVRPrivatePCH.h"
#include "Classes/SteamVRFunctionLibrary.h"
#include "SteamVRHMD.h"

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

void USteamVRFunctionLibrary::GetValidTrackedDeviceIds(TEnumAsByte<ESteamVRTrackedDeviceType> DeviceType, TArray<int32>& OutTrackedDeviceIds)
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

bool USteamVRFunctionLibrary::GetHandPositionAndOrientation(int32 ControllerIndex, EControllerHand Hand, FVector& OutPosition, FRotator& OutOrientation)
{
	bool RetVal = false;

	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		FQuat DeviceOrientation = FQuat::Identity;
		RetVal = SteamVRHMD->GetControllerHandPositionAndOrientation(ControllerIndex, Hand, OutPosition, DeviceOrientation);
		OutOrientation = DeviceOrientation.Rotator();
	}

	return RetVal;
}

void USteamVRFunctionLibrary::SetTrackingSpace(TEnumAsByte<ESteamVRTrackingSpace> NewSpace)
{
	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		SteamVRHMD->SetTrackingSpace(NewSpace);
	}
}

TEnumAsByte<ESteamVRTrackingSpace> USteamVRFunctionLibrary::GetTrackingSpace()
{
	ESteamVRTrackingSpace RetVal = ESteamVRTrackingSpace::Standing;

	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		RetVal = SteamVRHMD->GetTrackingSpace();
	}

	return RetVal;
}