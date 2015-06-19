// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
//
#include "SteamVRPrivatePCH.h"
#include "SteamVRHMD.h"
#include "Classes/SteamVRChaperoneComponent.h"

USteamVRChaperoneComponent::USteamVRChaperoneComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;

	bTickInEditor = true;
	bAutoActivate = true;

	bWasInsideSoftBounds = true;
}

void USteamVRChaperoneComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//@todo Make this safe once we can add something to the DeviceType enum.  For now, make the terrible assumption this is a SteamVR device.
 	FSteamVRHMD* SteamVRHMD = (FSteamVRHMD*)(GEngine->HMDDevice.Get());
 	if (SteamVRHMD && SteamVRHMD->IsStereoEnabled())
 	{
 		bool bInSoftBounds = SteamVRHMD->IsInsideSoftBounds();
 
		if (bInSoftBounds != bWasInsideSoftBounds)
		{
			if (bInSoftBounds)
			{
				OnReturnToSoftBounds.Broadcast();
			}
			else
			{
				OnLeaveSoftBounds.Broadcast();
			}
		}

		bWasInsideSoftBounds = bInSoftBounds;
 	}
}

TArray<FVector> USteamVRChaperoneComponent::GetSoftBounds() const
{
	TArray<FVector> RetValue;

	FSteamVRHMD* SteamVRHMD = (FSteamVRHMD*)(GEngine->HMDDevice.Get());
	if (SteamVRHMD && SteamVRHMD->IsStereoEnabled())
	{
		RetValue = SteamVRHMD->GetSoftBounds();
	}

	return RetValue;
}

TArray<FVector> USteamVRChaperoneComponent::GetHardBounds() const
{
	TArray<FVector> RetValue;

	FSteamVRHMD* SteamVRHMD = (FSteamVRHMD*)(GEngine->HMDDevice.Get());
	if (SteamVRHMD && SteamVRHMD->IsStereoEnabled())
	{
		RetValue = SteamVRHMD->GetHardBounds();
	}

	return RetValue;
}