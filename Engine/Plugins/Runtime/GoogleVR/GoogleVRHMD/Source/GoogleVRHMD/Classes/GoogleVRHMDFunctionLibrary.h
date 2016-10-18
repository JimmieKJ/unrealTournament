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

#include "GoogleVRHMDFunctionLibrary.generated.h"

/** Enum to specify distortion mesh size */
UENUM(BlueprintType)
enum class EDistortionMeshSizeEnum : uint8
{
	DMS_VERYSMALL	UMETA(DisplayName = "Distortion Mesh Size Very Small 20x20"),
	DMS_SMALL		UMETA(DisplayName = "Distortion Mesh Size Small 40x40"),
	DMS_MEDIUM		UMETA(DisplayName = "Distortion Mesh Size Medium 60x60"),
	DMS_LARGE		UMETA(DisplayName = "Distortion Mesh Size Large 80x80"),
	DMS_VERYLARGE	UMETA(DisplayName = "Distortion Mesh Size Very Large 100x100")
};

/**
 * GoogleVRHMD Extensions Function Library
 */
UCLASS()
class GOOGLEVRHMD_API UGoogleVRHMDFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	/** Enable/disable chromatic aberration correction */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetChromaticAberrationCorrectionEnabled(bool bEnable);

	/** Enable/disable distortion correction */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetDistortionCorrectionEnabled(bool bEnable);

	/** Change the default viewer profile */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool SetDefaultViewerProfile(const FString& ViewerProfileURL);

	/** Change the size of Distortion mesh */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetDistortionMeshSize(EDistortionMeshSizeEnum MeshSize);

	/** Check if distortion correction is enabled */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool GetDistortionCorrectionEnabled();

	/** Get the currently set viewer model */
	UFUNCTION(BlueprintPure, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static FString GetViewerModel();

	/** Get the currently set viewer vendor */
	UFUNCTION(BlueprintPure, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static FString GetViewerVendor();
};
