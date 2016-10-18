// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// VRNotificationsComponent.cpp: Component to handle receiving notifications from VR HMD

#include "HeadMountedDisplayPrivate.h"
#include "VRNotificationsComponent.h"
#include "CallbackDevice.h"

UVRNotificationsComponent::UVRNotificationsComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UVRNotificationsComponent::OnRegister()
{
	Super::OnRegister();

	FCoreDelegates::VRHeadsetRecenter.AddUObject(this, &UVRNotificationsComponent::HMDRecenteredDelegate_Handler);
	FCoreDelegates::VRHeadsetLost.AddUObject(this, &UVRNotificationsComponent::HMDLostDelegate_Handler);
	FCoreDelegates::VRHeadsetReconnected.AddUObject(this, &UVRNotificationsComponent::HMDReconnectedDelegate_Handler);
}

void UVRNotificationsComponent::OnUnregister()
{
	Super::OnUnregister();
	
	FCoreDelegates::VRHeadsetRecenter.RemoveAll(this);
	FCoreDelegates::VRHeadsetLost.RemoveAll(this);
	FCoreDelegates::VRHeadsetReconnected.RemoveAll(this);
}
