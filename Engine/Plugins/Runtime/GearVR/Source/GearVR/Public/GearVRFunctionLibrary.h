// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GearVRFunctionLibrary.generated.h"

UCLASS()
class UGearVRFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	/** Set Gear VR CPU and GPU Levels */
	UFUNCTION(BlueprintCallable, Category = "GearVR")
	static void SetCPUAndGPULevels(int CPULevel, int GPULevel);

	/** Set Gear VR CPU and GPU Levels */
	UFUNCTION(BlueprintCallable, Category = "GearVR")
	static bool IsPowerLevelStateMinimum();

	/** Set Gear VR CPU and GPU Levels */
	UFUNCTION(BlueprintCallable, Category = "GearVR")
	static bool IsPowerLevelStateThrottled();

	/** Set Gear VR CPU and GPU Levels */
	UFUNCTION(BlueprintCallable, Category = "GearVR")
	static bool AreHeadPhonesPluggedIn();

	/** Set Gear VR CPU and GPU Levels */
	UFUNCTION(BlueprintCallable, Category = "GearVR")
	static float GetTemperatureInCelsius();

	/** Set Gear VR CPU and GPU Levels */
	UFUNCTION(BlueprintCallable, Category = "GearVR")
	static float GetBatteryLevel();
};
