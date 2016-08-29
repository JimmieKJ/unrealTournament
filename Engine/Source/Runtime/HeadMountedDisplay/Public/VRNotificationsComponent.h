// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// VRNotificationsComponent.h: Component to handle receiving notifications from VR HMD

#pragma once

#include "VRNotificationsComponent.generated.h"

UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent), ClassGroup = HeadMountedDisplay)
class HEADMOUNTEDDISPLAY_API UVRNotificationsComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVRNotificationsDelegate);

	// This will be called when the application is asked for VR headset recenter.  
	UPROPERTY(BlueprintAssignable)
	FVRNotificationsDelegate HMDRecenteredDelegate;

	// This will be called when connection to HMD is lost.  
	UPROPERTY(BlueprintAssignable)
	FVRNotificationsDelegate HMDLostDelegate;

	// This will be called when connection to HMD is restored.  
	UPROPERTY(BlueprintAssignable)
	FVRNotificationsDelegate HMDReconnectedDelegate;

public:
	void OnRegister() override;
	void OnUnregister() override;

private:
	/** Native handlers that get registered with the actual FCoreDelegates, and then proceed to broadcast to the delegates above */
	void HMDRecenteredDelegate_Handler()	{ HMDRecenteredDelegate.Broadcast(); }
	void HMDLostDelegate_Handler()			{ HMDLostDelegate.Broadcast(); }
	void HMDReconnectedDelegate_Handler()	{ HMDReconnectedDelegate.Broadcast(); }
};



