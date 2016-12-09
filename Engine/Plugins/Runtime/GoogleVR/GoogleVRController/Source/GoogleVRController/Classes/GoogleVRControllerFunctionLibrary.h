/* Copyright 2016 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "GoogleVRControllerEventManager.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GoogleVRControllerFunctionLibrary.generated.h"

UENUM(BlueprintType)
enum class EGoogleVRControllerHandedness : uint8
{
	RightHanded,
	LeftHanded,
	Unknown
};

/**
 * GoogleVRController Extensions Function Library
 */
UCLASS()
class GOOGLEVRCONTROLLER_API UGoogleVRControllerFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Get user's handedness preference from GVRSDK
	 * @return A EGoogleVRControllerHandedness indicates the user's handedness preference in GoogleVR Settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static EGoogleVRControllerHandedness GetGoogleVRControllerHandedness();

	/** 
	 * This function return the controller acceleration in gvr controller space. 
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static FVector GetGoogleVRControllerRawAccel();

	/** 
	 * This function return the controller angular velocity about each axis (positive means clockwise when sighting along axis) in gvr controller space. 
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static FVector GetGoogleVRControllerRawGyro();

	/** 
	 * This function return the orientation of the controller in unreal space. 
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static FRotator GetGoogleVRControllerOrientation();

	/** 
	 * Return a pointer to the UGoogleVRControllerEventManager to hook up GoogleVR Controller specific event.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRController", meta = (Keywords = "Cardboard AVR GVR"))
	static UGoogleVRControllerEventManager* GetGoogleVRControllerEventManager();

private:
	static UGoogleVRControllerEventManager* ControllerEventManager;

};