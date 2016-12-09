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

#include "CoreMinimal.h"
#include "IHeadMountedDisplay.h"
#include "IGoogleVRHMDPlugin.h"
#include "Kismet/BlueprintFunctionLibrary.h"
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
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool IsGoogleVRHMDEnabled();

	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool IsGoogleVRStereoRenderingEnabled();

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

	/** Was the application launched in Vr. */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool IsVrLaunch();

	/** Get the RenderTarget size GoogleVRHMD is using for rendering the scene.
	 *  @return The render target size that is used when rendering the scene.
	*/
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static FIntPoint GetGVRHMDRenderTargetSize();

	/** Set the GoogleVR render target size to default value.
	 *  @return The default render target size.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static FIntPoint SetRenderTargetSizeToDefault();

	/** Set the RenderTarget size with a scale factor.
	 *  The scale factor will be multiplied by the maximal effective render target size based on the window size and the viewer.
	 *
	 *  @param ScaleFactor - A float number that is within [0.1, 1.0].
	 *  @param OutRenderTargetSize - The actual render target size it is set to.
	 *  @return true if the render target size changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool SetGVRHMDRenderTargetScale(float ScaleFactor, FIntPoint& OutRenderTargetSize);

	/** Set the RenderTargetSize with the desired resolution.
	 *  @param DesiredWidth - The width of the render target.
	 *  @param DesiredHeight - The height of the render target.
	 *  @param OutRenderTargetSize - The actually render target size it is set to.
	 *  @return true if the render target size changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool SetGVRHMDRenderTargetSize(int DesiredWidth, int DesiredHeight, FIntPoint& OutRenderTargetSize);

	/** A scaling factor for the neck model offset, clamped from 0 to 1.
	 * This should be 1 for most scenarios, while 0 will effectively disable
	 * neck model application. This value can be animated to smoothly
	 * interpolate between alternative (client-defined) neck models.
	 *  @param ScaleFactor The new neck model scale.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetNeckModelScale(float ScaleFactor);

	/** A scaling factor for the neck model offset, clamped from 0 to 1.
	 * This should be 1 for most scenarios, while 0 will effectively disable
	 * neck model application. This value can be animated to smoothly
	 * interpolate between alternative (client-defined) neck models.
	 *  @return the current neck model scale.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static float GetNeckModelScale();

	/**
	 * Returns the string representation of the data URI on which this activity's intent is operating.
	 * See Intent.getDataString() in the Android documentation.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static FString GetIntentData();
	
	/**
	 * Set whether to enable the loading splash screen in daydream app.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetDaydreamLoadingSplashScreenEnable(bool enable);

	/**
	 * Set the loading splash screen texture the daydream app wil be using.
	 * Note that this function only works for daydream app.
	 *
	 * @param Texture		A texture asset to be used for rendering the splash screen.
	 * @param UVOffset		A 2D vector for offset the splash screen texture. Default value is (0.0, 0.0)
	 * @param UVSize		A 2D vector specifies which part of the splash texture will be rendered on the screen. Default value is (1.0, 1.0)
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetDaydreamLoadingSplashScreenTexture(class UTexture2D* Texture, FVector2D UVOffset = FVector2D(0.0f, 0.0f), FVector2D UVSize = FVector2D(1.0f, 1.0f));

	/**
	 * Clear the loading splash texture it is current using. This will make the loading screen to black if the loading splash screen is still enabled.
	 * Note that this function only works for daydream app.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static void ClearDaydreamLoadingSplashScreenTexture();
};
