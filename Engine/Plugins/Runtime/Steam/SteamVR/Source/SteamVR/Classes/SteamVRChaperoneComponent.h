// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SteamVRChaperoneComponent.generated.h"

/**
 * SteamVR Extensions Function Library
 */
UCLASS(MinimalAPI, meta = (BlueprintSpawnableComponent), ClassGroup = SteamVR)
class USteamVRChaperoneComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSteamVRChaperoneEvent);

	UPROPERTY(BlueprintAssignable, Category = "SteamVR")
	FSteamVRChaperoneEvent OnLeaveSoftBounds;

	UPROPERTY(BlueprintAssignable, Category = "SteamVR")
	FSteamVRChaperoneEvent OnReturnToSoftBounds;

	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/**
	 * Returns the soft bounds from the Chaperone, in Unreal-scale HMD-space coordinates, centered around the HMD's calibration origin (0,0,0).  Note that the Z component is always zero
	 */
	UFUNCTION(BlueprintPure, Category = "SteamVR")
	TArray<FVector> GetSoftBounds() const;

	/**
	* Returns the hard bounds from the Chaperone, in Unreal-scale HMD-space coordinates, centered around the HMD's calibration origin (0,0,0).  Each set of four bounds will form a quad to define a set of hard bounds
	*/
	UFUNCTION(BlueprintPure, Category = "SteamVR")
	TArray<FVector> GetHardBounds() const;

private:
	/** Whether or not we were inside the soft bounds last time, so we can detect changes */
	bool bWasInsideSoftBounds;
};
