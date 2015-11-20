// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "LeapMotionControllerPrivatePCH.h"
#include "LeapMotionFunctionLibrary.h"

#include "LeapMotionControllerPlugin.h"
#include "LeapMotionDevice.h"


ULeapMotionFunctionLibrary::ULeapMotionFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool ULeapMotionFunctionLibrary::IsConnected()
{
	FLeapMotionDevice* LeapDevice = FLeapMotionControllerPlugin::GetLeapDeviceSafe();
	if (LeapDevice)
	{
		return LeapDevice->IsConnected();
	}

	return false;
}

bool ULeapMotionFunctionLibrary::GetAllHandsIds(TArray<int32>& OutAllHandIds)
{
	FLeapMotionDevice* LeapDevice = FLeapMotionControllerPlugin::GetLeapDeviceSafe();
	if (LeapDevice)
	{
		LeapDevice->SetReferenceFrameOncePerTick();
		return LeapDevice->GetAllHandsIds(OutAllHandIds);
	}
	return false;
}

bool ULeapMotionFunctionLibrary::GetOldestLeftOrRightHandId(ELeapSide LeapSide, int32& OutHandId)
{
	FLeapMotionDevice* LeapDevice = FLeapMotionControllerPlugin::GetLeapDeviceSafe();
	if (LeapDevice)
	{
		LeapDevice->SetReferenceFrameOncePerTick();
		return LeapDevice->GetOldestLeftOrRightHandId(LeapSide, OutHandId);
	}
	return false;
}

bool ULeapMotionFunctionLibrary::GetBonePostionAndOrientation(int32 HandId, ELeapBone LeapBone, FVector& OutPosition, FRotator& OutOrientation)
{
	OutPosition = FVector::ZeroVector;
	OutOrientation = FRotator::ZeroRotator;

	FLeapMotionDevice* LeapDevice = FLeapMotionControllerPlugin::GetLeapDeviceSafe();
	if (LeapDevice)
	{
		LeapDevice->SetReferenceFrameOncePerTick();
		return LeapDevice->GetBonePostionAndOrientation(HandId, LeapBone, OutPosition, OutOrientation);
	}

	return false;
}

bool ULeapMotionFunctionLibrary::GetBoneWidthAndLength(int32 HandId, ELeapBone LeapBone, float& OutWidth, float& OutLength)
{
	OutWidth = 0.0f;
	OutLength = 0.0f;

	FLeapMotionDevice* LeapDevice = FLeapMotionControllerPlugin::GetLeapDeviceSafe();
	if (LeapDevice)
	{
		LeapDevice->SetReferenceFrameOncePerTick();
		return LeapDevice->GetBoneWidthAndLength(HandId, LeapBone, OutWidth, OutLength);
	}

	return false;
}

bool ULeapMotionFunctionLibrary::GetGrabStrength(int32 HandId, float& GrabStrength)
{
	FLeapMotionDevice* LeapDevice = FLeapMotionControllerPlugin::GetLeapDeviceSafe();
	if (LeapDevice)
	{
		LeapDevice->SetReferenceFrameOncePerTick();
		return LeapDevice->GetGrabStrength(HandId, GrabStrength);
	}
	return false;
}

bool ULeapMotionFunctionLibrary::GetPinchStrength(int32 HandId, float& PinchStrength)
{
	FLeapMotionDevice* LeapDevice = FLeapMotionControllerPlugin::GetLeapDeviceSafe();
	if (LeapDevice)
	{
		LeapDevice->SetReferenceFrameOncePerTick();
		return LeapDevice->GetPinchStrength(HandId, PinchStrength);
	}
	return false;
}


bool ULeapMotionFunctionLibrary::SetHmdPolicy(bool UseHmdPolicy)
{
	FLeapMotionDevice* LeapDevice = FLeapMotionControllerPlugin::GetLeapDeviceSafe();
	if (LeapDevice)
	{
		return LeapDevice->SetHmdPolicy(UseHmdPolicy);
	}
	return false;
}

bool ULeapMotionFunctionLibrary::SetImagePolicy(bool UseImagePolicy)
{
	FLeapMotionDevice* LeapDevice = FLeapMotionControllerPlugin::GetLeapDeviceSafe();
	if (LeapDevice)
	{
		return LeapDevice->SetImagePolicy(UseImagePolicy);
	}
	return false;
}
