// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StereoRendering.h"
#include "Layout/SlateRect.h"
#include "Kismet/HeadMountedDisplayFunctionLibrary.h"
#include "HeadMountedDisplayTypes.h"

/**
 * HMD device interface
 */

class HEADMOUNTEDDISPLAY_API IHeadMountedDisplay : public IModuleInterface, public IStereoRendering
{

public:
	IHeadMountedDisplay();

	virtual FName GetDeviceName() const
	{
		static FName DefaultName(TEXT("Unknown"));
		return DefaultName;
	}

	/**
	 * Returns true if HMD is currently connected.
	 */
	virtual bool IsHMDConnected() = 0;

	/**
	 * Whether or not switching to stereo is enabled; if it is false, then EnableStereo(true) will do nothing.
	 */
	virtual bool IsHMDEnabled() const = 0;

	/**
	* Returns EHMDWornState::Worn if we detect that the user is wearing the HMD, EHMDWornState::NotWorn if we detect the user is not wearing the HMD, and EHMDWornState::Unknown if we cannot detect the state.
	*/
	virtual EHMDWornState::Type GetHMDWornState() { return EHMDWornState::Unknown; };

	/**
	 * Enables or disables switching to stereo.
	 */
	virtual void EnableHMD(bool bEnable = true) = 0;

	/**
	 * Returns the family of HMD device implemented
	 */
	virtual EHMDDeviceType::Type GetHMDDeviceType() const = 0;

	struct MonitorInfo
	{
		FString MonitorName;
		size_t  MonitorId;
		int		DesktopX, DesktopY;
		int		ResolutionX, ResolutionY;
		int		WindowSizeX, WindowSizeY;

		MonitorInfo() : MonitorId(0)
			, DesktopX(0)
			, DesktopY(0)
			, ResolutionX(0)
			, ResolutionY(0)
			, WindowSizeX(0)
			, WindowSizeY(0)
		{
		}

	};

	/**
	 * Get the name or id of the display to output for this HMD.
	 */
	virtual bool	GetHMDMonitorInfo(MonitorInfo&) = 0;

	/**
	 * Calculates the FOV, based on the screen dimensions of the device. Original FOV is passed as params.
	 */
	virtual void	GetFieldOfView(float& InOutHFOVInDegrees, float& InOutVFOVInDegrees) const = 0;

	/**
	 * Whether or not the HMD supports positional tracking (either via sensor or other means)
	 */
	virtual bool	DoesSupportPositionalTracking() const = 0;

	/**
	 * If the device has positional tracking, whether or not we currently have valid tracking
	 */
	virtual bool	HasValidTrackingPosition() = 0;

	/**
	 * If the HMD supports positional tracking via a sensor, this returns the frustum properties (all in game-world space) of the sensor.
	 * Returns false, if the sensor at the specified index is not available.
	 */
	//DEPRECATED(4.13, "Please use GetNumOfTrackingSensors / GetTrackingSensorProperties functions")
	virtual void	GetPositionalTrackingCameraProperties(FVector& OutOrigin, FQuat& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const
	{
		GetTrackingSensorProperties(0, OutOrigin, OutOrientation, OutHFOV, OutVFOV, OutCameraDistance, OutNearPlane, OutFarPlane);
	}

	/**
	 *  Returns total number of tracking sensors supported by the HMD. To be used along with GetP
	 */
	virtual uint32	GetNumOfTrackingSensors() const { return 0; }

	/**
	 * If the HMD supports positional tracking via a sensor, this returns the frustum properties (all in game-world space) of the sensor.
	 * Returns false, if the sensor at the specified index is not available.
	 */
	virtual bool	GetTrackingSensorProperties(uint8 InSensorIndex, FVector& OutOrigin, FQuat& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const { return false; }

	/**
	 * Accessors to modify the interpupillary distance (meters)
	 */
	virtual void	SetInterpupillaryDistance(float NewInterpupillaryDistance) = 0;
	virtual float	GetInterpupillaryDistance() const = 0;

	/**
	 * Get the current orientation and position reported by the HMD.
	 */
	virtual void GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition) = 0;

	/**
	 * Rebase the input position and orientation to that of the HMD's base
	 */
	virtual void RebaseObjectOrientationAndPosition(FVector& Position, FQuat& Orientation) const = 0;

	/**
	 * Get the offset, in device space, of the reported device (screen / eye) position to the center of the head, if supported
	 */
	virtual FVector GetAudioListenerOffset() const { return FVector(0.f); }

	/**
	 * Get the ISceneViewExtension for this HMD, or none.
	 */
	virtual TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> GetViewExtension() = 0;

	/**
	 * Apply the orientation of the headset to the PC's rotation.
	 * If this is not done then the PC will face differently than the camera,
	 * which might be good (depending on the game).
	 */
	virtual void ApplyHmdRotation(class APlayerController* PC, FRotator& ViewRotation) = 0;

	/**
	 * Apply the orientation and position of the headset to the Camera.
	 */
	virtual bool UpdatePlayerCamera(FQuat& CurrentOrientation, FVector& CurrentPosition) = 0;

	/**
	 * Gets the scaling factor, applied to the post process warping effect
	 */
	virtual float GetDistortionScalingFactor() const { return 0; }

	/**
	 * Gets the offset (in clip coordinates) from the center of the screen for the lens position
	 */
	virtual float GetLensCenterOffset() const { return 0; }

	/**
	 * Gets the barrel distortion shader warp values for the device
	 */
	virtual void GetDistortionWarpValues(FVector4& K) const  { }

	/**
	 * Returns 'false' if chromatic aberration correction is off.
	 */
	virtual bool IsChromaAbCorrectionEnabled() const = 0;

	/**
	 * Gets the chromatic aberration correction shader values for the device.
	 * Returns 'false' if chromatic aberration correction is off.
	 */
	virtual bool GetChromaAbCorrectionValues(FVector4& K) const  { return false; }

	/**
	 * Exec handler to allow console commands to be passed through to the HMD for debugging
	 */
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) = 0;

	/** Returns true if positional tracking enabled and working. */
	virtual bool IsPositionalTrackingEnabled() const = 0;

	/**
	 * Tries to enable positional tracking.
	 * Returns the actual status of positional tracking.
	 */
	virtual bool EnablePositionalTracking(bool enable) = 0;

	/**
	 * Returns true, if head tracking is allowed. Most common case: it returns true when GEngine->IsStereoscopic3D() is true,
	 * but some overrides are possible.
	 */
	virtual bool IsHeadTrackingAllowed() const = 0;

	/**
	 * Returns true, if HMD is in low persistence mode. 'false' otherwise.
	 */
	virtual bool IsInLowPersistenceMode() const = 0;

	/**
	 * Switches between low and full persistence modes.
	 *
	 * @param Enable			(in) 'true' to enable low persistence mode; 'false' otherwise
	 */
	virtual void EnableLowPersistenceMode(bool Enable = true) = 0;

	/**
	 * Resets orientation by setting roll and pitch to 0, assuming that current yaw is forward direction and assuming
	 * current position as a 'zero-point' (for positional tracking).
	 *
	 * @param Yaw				(in) the desired yaw to be set after orientation reset.
	 */
	virtual void ResetOrientationAndPosition(float Yaw = 0.f) = 0;

	/**
	 * Resets orientation by setting roll and pitch to 0, assuming that current yaw is forward direction. Position is not changed.
	 *
	 * @param Yaw				(in) the desired yaw to be set after orientation reset.
	 */
	virtual void ResetOrientation(float Yaw = 0.f) {}

	/**
	 * Resets position, assuming current position as a 'zero-point'.
	 */
	virtual void ResetPosition() {}

	/**
	 * Sets base orientation by setting yaw, pitch, roll, assuming that this is forward direction.
	 * Position is not changed.
	 *
	 * @param BaseRot			(in) the desired orientation to be treated as a base orientation.
	 */
	virtual void SetBaseRotation(const FRotator& BaseRot) {}

	/**
	 * Returns current base orientation of HMD as yaw-pitch-roll combination.
	 */
	virtual FRotator GetBaseRotation() const { return FRotator::ZeroRotator; }

	/**
	 * Sets base orientation, assuming that this is forward direction.
	 * Position is not changed.
	 *
	 * @param BaseOrient		(in) the desired orientation to be treated as a base orientation.
	 */
	virtual void SetBaseOrientation(const FQuat& BaseOrient) {}

	/**
	 * Returns current base orientation of HMD as a quaternion.
	 */
	virtual FQuat GetBaseOrientation() const { return FQuat::Identity; }

	/**
	 * Scales the HMD position that gets added to the virtual camera position.
	 *
	 * @param PosScale3D	(in) the scale to apply to the HMD position.
	 */
	virtual void SetPositionScale3D(FVector PosScale3D) {}

	/**
	 * Returns current position scale of HMD.
	 */
	virtual FVector GetPositionScale3D() const { return FVector::ZeroVector; }

	/**
	* @return true if a hidden area mesh is available for the device.
	*/
	virtual bool HasHiddenAreaMesh() const { return false; }

	/**
	* @return true if a visible area mesh is available for the device.
	*/
	virtual bool HasVisibleAreaMesh() const { return false; }

	/**
	* Optional method to draw a view's hidden area mesh where supported.
	* This can be used to avoid rendering pixels which are not included as input into the final distortion pass.
	*/
	virtual void DrawHiddenAreaMesh_RenderThread(class FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const {};

	/**
	* Optional method to draw a view's visible area mesh where supported.
	* This can be used instead of a full screen quad to avoid rendering pixels which are not included as input into the final distortion pass.
	*/
	virtual void DrawVisibleAreaMesh_RenderThread(class FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const {};

	virtual void DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FIntPoint& TextureSize) {}

	/**
	 * This method is able to change screen settings right before any drawing occurs. 
	 * It is called at the beginning of UGameViewportClient::Draw() method.
	 * We might remove this one as UpdatePostProcessSettings should be able to capture all needed cases
	 */
	virtual void UpdateScreenSettings(const FViewport* InViewport) {}

	/**
	 * Allows to override the PostProcessSettings in the last moment e.g. allows up sampled 3D rendering
	 */
	virtual void UpdatePostProcessSettings(FPostProcessSettings*) {}

	/**
	 * Draw desired debug information related to the HMD system.
	 * @param Canvas The canvas on which to draw.
	 */
	virtual void DrawDebug(UCanvas* Canvas) {}

	/**
	 * Passing key events to HMD.
	 * If returns 'false' then key will be handled by PlayerController;
	 * otherwise, key won't be handled by the PlayerController.
	 */
	virtual bool HandleInputKey(class UPlayerInput*, const struct FKey& Key, enum EInputEvent EventType, float AmountDepressed, bool bGamepad) { return false; }

	/**
	 * Passing touch events to HMD.
	 * If returns 'false' then touch will be handled by PlayerController;
	 * otherwise, touch won't be handled by the PlayerController.
	 */
	virtual bool HandleInputTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, FDateTime DeviceTimestamp, uint32 TouchpadIndex) { return false; }

	/**
	 * This method is called when playing begins. Useful to reset all runtime values stored in the plugin.
	 */
	virtual void OnBeginPlay(FWorldContext& InWorldContext) {}

	/**
	 * This method is called when playing ends. Useful to reset all runtime values stored in the plugin.
	 */
	virtual void OnEndPlay(FWorldContext& InWorldContext) {}

	/**
	 * This method is called when new game frame begins (called on a game thread).
	 */
	virtual bool OnStartGameFrame( FWorldContext& WorldContext ) { return false; }

	/**
	 * This method is called when game frame ends (called on a game thread).
	 */
	virtual bool OnEndGameFrame( FWorldContext& WorldContext ) { return false; }

	/** 
	 * Additional optional distorion rendering parameters
	 * @todo:  Once we can move shaders into plugins, remove these!
	 */	
	virtual FTexture* GetDistortionTextureLeft() const {return NULL;}
	virtual FTexture* GetDistortionTextureRight() const {return NULL;}
	virtual FVector2D GetTextureOffsetLeft() const {return FVector2D::ZeroVector;}
	virtual FVector2D GetTextureOffsetRight() const {return FVector2D::ZeroVector;}
	virtual FVector2D GetTextureScaleLeft() const {return FVector2D::ZeroVector;}
	virtual FVector2D GetTextureScaleRight() const {return FVector2D::ZeroVector;}
	virtual const float* GetRedDistortionParameters() const { return nullptr; }
	virtual const float* GetGreenDistortionParameters() const { return nullptr; }
	virtual const float* GetBlueDistortionParameters() const { return nullptr; }

	virtual bool NeedsUpscalePostProcessPass()  { return false; }

	/**
	 * Record analytics
	 */
	virtual void RecordAnalytics() {}
	/**
	 * Returns version string.
	 */
	virtual FString GetVersionString() const { return FString(TEXT("GenericHMD")); }

	/**
	 * Sets tracking origin (either 'eye'-level or 'floor'-level).
	 */
	virtual void SetTrackingOrigin(EHMDTrackingOrigin::Type NewOrigin) {};

	/**
	 * Returns current tracking origin.
	 */
	virtual EHMDTrackingOrigin::Type GetTrackingOrigin() { return EHMDTrackingOrigin::Eye; }

	/**
	 * Returns true, if the App is using VR focus. This means that the app may handle lifecycle events differently from
	 * the regular desktop apps. In this case, FCoreDelegates::ApplicationWillEnterBackgroundDelegate and FCoreDelegates::ApplicationHasEnteredForegroundDelegate
	 * reflect the state of VR focus (either the app should be rendered in HMD or not).
	 */
	virtual bool DoesAppUseVRFocus() const;

	/**
	 * Returns true, if the app has VR focus, meaning if it is rendered in the HMD.
	 */
	virtual bool DoesAppHaveVRFocus() const;

	/** Setup state for applying the render thread late update */
	virtual void SetupLateUpdate(const FTransform& ParentToWorld, USceneComponent* Component);

	/** Apply the late update delta to the cached compeonents */
	virtual void ApplyLateUpdate(FSceneInterface* Scene, const FTransform& OldRelativeTransform, const FTransform& NewRelativeTransform);
	
private:

	/*
	 *	Late update primitive info for accessing valid scene proxy info. From the time the info is gathered
	 *  to the time it is later accessed the render proxy can be deleted. To ensure we only access a proxy that is
	 *  still valid we cache the primitive's scene info AND a pointer to it's own cached index. If the primitive
	 *  is deleted or removed from the scene then attempting to access it via it's index will result in a different
	 *  scene info than the cached scene info.
	 */
	struct LateUpdatePrimitiveInfo
	{
		const int32*			IndexAddress;
		FPrimitiveSceneInfo*	SceneInfo;
	};

	void GatherLateUpdatePrimitives(USceneComponent* Component, TArray<LateUpdatePrimitiveInfo>& Primitives);

	/** Primitives that need late update before rendering */
	TArray<LateUpdatePrimitiveInfo> LateUpdatePrimitives[2];

	int32 LateUpdateGameWriteIndex;
	int32 LateUpdateRenderReadIndex;

	/** Parent world transform used to reconstruct new world transforms for late update scene proxies */
	FTransform LateUpdateParentToWorld;
};
