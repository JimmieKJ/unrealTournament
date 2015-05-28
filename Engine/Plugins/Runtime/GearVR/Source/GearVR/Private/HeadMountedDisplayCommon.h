// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IHeadMountedDisplay.h"
#include "SceneViewExtension.h"

class FHeadMountedDisplay;

/** Converts vector from meters to UU (Unreal Units) */
FORCEINLINE FVector MetersToUU(const FVector& InVec, float WorldToMetersScale)
{
	return InVec * WorldToMetersScale;
}
/** Converts vector from UU (Unreal Units) to meters */
FORCEINLINE FVector UUToMeters(const FVector& InVec, float WorldToMetersScale)
{
	return InVec * (1.f / WorldToMetersScale);
}

class FHMDSettings : public TSharedFromThis<FHMDSettings, ESPMode::ThreadSafe>
{
public:
	/** Whether or not the Oculus was successfully initialized */
	enum EInitStatus
	{
		eNotInitialized = 0x00,
		eStartupExecuted = 0x01,
		eInitialized = 0x02,
	};

	union
	{
		struct
		{
			uint64 InitStatus : 2; // see bitmask EInitStatus

			/** Whether stereo is currently on or off. */
			uint64 bStereoEnabled : 1;

			/** Whether or not switching to stereo is allowed */
			uint64 bHMDEnabled : 1;

			/** Debugging:  Whether or not the stereo rendering settings have been manually overridden by an exec command.  They will no longer be auto-calculated */
			uint64 bOverrideStereo : 1;

			/** Debugging:  Whether or not the IPD setting have been manually overridden by an exec command. */
			uint64 bOverrideIPD : 1;

			/** Debugging:  Whether or not the distortion settings have been manually overridden by an exec command.  They will no longer be auto-calculated */
			uint64 bOverrideDistortion : 1;

			/** Debugging: Allows changing internal params, such as screen size, eye-to-screen distance, etc */
			uint64 bDevSettingsEnabled : 1;

			uint64 bOverrideFOV : 1;

			/** Whether or not to override game VSync setting when switching to stereo */
			uint64 bOverrideVSync : 1;

			/** Overridden VSync value */
			uint64 bVSync : 1;

			/** Saved original values for VSync and ScreenPercentage. */
			uint64 bSavedVSync : 1;

			/** Whether or not to override game ScreenPercentage setting when switching to stereo */
			uint64 bOverrideScreenPercentage : 1;

			/** Allows renderer to finish current frame. Setting this to 'true' may reduce the total 
			 *  framerate (if it was above vsync) but will reduce latency. */
			uint64 bAllowFinishCurrentFrame : 1;

			/** Whether world-to-meters scale is overriden or not. */
			uint64 bWorldToMetersOverride : 1;

			/** Distortion on/off */
			uint64 bHmdDistortion : 1;

			/** Chromatic aberration correction on/off */
			uint64 bChromaAbCorrectionEnabled : 1;

			/** Yaw drift correction on/off */
			uint64 bYawDriftCorrectionEnabled : 1;

			/** Low persistence mode */
			uint64 bLowPersistenceMode : 1;

			/** Turns on/off updating view's orientation/position on a RenderThread. When it is on,
				latency should be significantly lower. 
				See 'HMD UPDATEONRT ON|OFF' console command.
			*/
			uint64 bUpdateOnRT : 1;

			/** Overdrive brightness transitions to reduce artifacts on DK2+ displays */
			uint64 bOverdrive : 1;

			/** High-quality sampling of distortion buffer for anti-aliasing */
			uint64 bHQDistortion : 1;

			/** Enforces headtracking to work even in non-stereo mode (for debugging or screenshots). 
				See 'MOTION ENFORCE' console command. */
			uint64 bHeadTrackingEnforced : 1;

			/** Is mirroring enabled or not (see 'HMD MIRROR' console cmd) */
			uint64 bMirrorToWindow : 1;

			/** Whether timewarp is enabled or not */
			uint64 bTimeWarp : 1;

			/** True, if pos tracking is enabled */
			uint64				bHmdPosTracking : 1;

			/** True, if Far/Mear clipping planes got overriden */
			uint64				bClippingPlanesOverride : 1;

			/** True, if PlayerController follows HMD orient/pos; false, if this behavior should be disabled. */
			uint64				bPlayerControllerFollowsHmd : 1;

			/** True, if PlayerCameraManager should follow HMD orientation */
			uint64				bPlayerCameraManagerFollowsHmdOrientation : 1;

			/** True, if PlayerCameraManager should follow HMD position */
			uint64				bPlayerCameraManagerFollowsHmdPosition : 1;
#if !UE_BUILD_SHIPPING
			/** Draw tracking camera frustum, for debugging purposes. 
			 *  See 'HMDPOS SHOWCAMERA ON|OFF' console command.
			 */
			uint64				bDrawTrackingCameraFrustum : 1;

			uint64				bDrawCubes : 1;

			/** Turns off updating of orientation/position on game thread. See 'hmd updateongt' cmd */
			uint64				bDoNotUpdateOnGT : 1;

			/** Show status / statistics on screen. See 'hmd stats' cmd */
			uint64				bShowStats : 1;

			/** Draw lens centered grid */
			uint64				bDrawGrid : 1;

			/** Profiling mode, removed extra waits in Present (Direct Rendering). See 'hmd profile' cmd */
			uint64				bProfiling : 1;
#endif
		};
		uint64 Raw;
	} Flags;

	/** Saved original value for ScreenPercentage. */
	float SavedScrPerc;

	/** Overridden ScreenPercentage value */
	float ScreenPercentage;

	/** Ideal ScreenPercentage value for the HMD */
	float IdealScreenPercentage;

	/** Interpupillary distance, in meters (user configurable) */
	float InterpupillaryDistance;

	/** World units (UU) to Meters scale.  Read from the level, and used to transform positional tracking data */
	float WorldToMetersScale;

	/** User-tunable modification to the interpupillary distance */
	float UserDistanceToScreenModifier;

	/** The FOV to render at (radians), based on the physical characteristics of the device */
	float HFOVInRadians; // horizontal
	float VFOVInRadians; // vertical

	/** Optional far clipping plane for projection matrix */
	float NearClippingPlane;

	/** Optional far clipping plane for projection matrix */
	float FarClippingPlane;

	/** Size of mirror window; {0,0} if size is the default one */
	FIntPoint	MirrorWindowSize;

	/** HMD base values, specify forward orientation and zero pos offset */
	FVector2D				NeckToEyeInMeters;  // neck-to-eye vector, in meters (X - horizontal, Y - vertical)
	FVector					BaseOffset;			// base position, in meters, relatively to the tracker //@todo hmd: clients need to stop using oculus space
	FQuat					BaseOrientation;	// base orientation

	/** Viewports for each eye, in render target texture coordinates */
	FIntRect				EyeRenderViewport[2];

	FHMDSettings();
	virtual ~FHMDSettings() {}

	bool IsStereoEnabled() const { return Flags.bStereoEnabled && Flags.bHMDEnabled; }

	virtual void SetEyeRenderViewport(int OneEyeVPw, int OneEyeVPh);
	virtual FIntPoint GetTextureSize() const;
	
	virtual float GetActualScreenPercentage() const;

	virtual TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> Clone() const;
};

class FHMDGameFrame : public TSharedFromThis<FHMDGameFrame, ESPMode::ThreadSafe>
{
public:
	uint32					FrameNumber; // current frame number.
	TSharedPtr<FHMDSettings, ESPMode::ThreadSafe>	Settings;

	/** World units (UU) to Meters scale.  Read from the level, and used to transform positional tracking data */
	float					WorldToMetersScale;
	FVector					CameraScale3D;

	FRotator				CachedViewRotation[2]; // cached view rotations

	FQuat					LastHmdOrientation; // contains last APPLIED ON GT HMD orientation
	FVector					LastHmdPosition;    // contains last APPLIED ON GT HMD position

	FIntPoint				ViewportSize;		// full final viewport size (window size, backbuffer size)

	union
	{
		struct
		{
			/** True, if game thread is finished */
			uint64			bOutOfFrame : 1;

			/** Whether ScreenPercentage usage is enabled */
			uint64			bScreenPercentageEnabled : 1;

			/** True, if vision is acquired at the moment */
			uint64			bHaveVisionTracking : 1;

			/** True, if HMD orientation was applied during the game thread */
			uint64			bOrientationChanged : 1;
			/** True, if HMD position was applied during the game thread */
			uint64			bPositionChanged : 1;
			/** True, if ApplyHmdRotation was used */
			uint64			bPlayerControllerFollowsHmd : 1;

			/** Indicates if CameraScale3D was already set by GetCurrentOrientAndPos */
			uint64			bCameraScale3DAlreadySet : 1;
		};
		uint64 Raw;
	} Flags;

	FHMDGameFrame();
	virtual ~FHMDGameFrame() {}

	FHMDSettings* GetSettings()
	{
		return Settings.Get();
	}

	virtual TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> Clone() const;
};

class FHMDViewExtension : public ISceneViewExtension, public TSharedFromThis<FHMDViewExtension, ESPMode::ThreadSafe>
{
public:
	FHMDViewExtension(FHeadMountedDisplay* InDelegate);
	virtual ~FHMDViewExtension();

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;

public: // data
	FHeadMountedDisplay* Delegate;
	TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> RenderFrame;
};

/**
 * HMD device interface
 */
class FHeadMountedDisplay : public IHeadMountedDisplay
{
public:
	FHeadMountedDisplay();

	/** @return	True if the HMD was initialized OK */
	virtual bool IsInitialized() const;

    /**
     * Get the ISceneViewExtension for this HMD, or none.
     */
    virtual TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> GetViewExtension() override;

	/**
	 * Get the FStereoRendering for this HMD, or none.
	 */
	virtual class FStereoRendering* GetStereoRendering()
	{
		return nullptr;
	}

	/**
	 * This method is called when new game frame begins (called on a game thread).
	 */
	virtual bool OnStartGameFrame() override;

	/**
	 * This method is called when game frame ends (called on a game thread).
	 */
	virtual bool OnEndGameFrame() override;

	virtual void SetupViewFamily(class FSceneViewFamily& InViewFamily) {}
	virtual void SetupView(class FSceneViewFamily& InViewFamily, FSceneView& InView) {}

	TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> PassRenderFrameOwnership();

	virtual bool IsHMDEnabled() const override;
	virtual void EnableHMD(bool allow = true) override;

	virtual bool DoesSupportPositionalTracking() const override;
	virtual bool HasValidTrackingPosition() override;
	virtual void GetPositionalTrackingCameraProperties(FVector& OutOrigin, FQuat& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const override;

	virtual bool IsInLowPersistenceMode() const override;
	virtual void EnableLowPersistenceMode(bool Enable = true) override;
	virtual bool IsHeadTrackingAllowed() const override;

	virtual void SetInterpupillaryDistance(float NewInterpupillaryDistance) override;
	virtual float GetInterpupillaryDistance() const override;
	virtual void GetFieldOfView(float& InOutHFOVInDegrees, float& InOutVFOVInDegrees) const override;

	virtual bool IsChromaAbCorrectionEnabled() const override;
	virtual void OnScreenModeChange(EWindowMode::Type WindowMode) override;

	virtual bool IsPositionalTrackingEnabled() const override;
	virtual bool EnablePositionalTracking(bool enable) override;

	virtual bool EnableStereo(bool stereo = true) override;
	virtual bool IsStereoEnabled() const override;
	virtual void AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;

	virtual void SetClippingPlanes(float NCP, float FCP) override;

	virtual void SetBaseRotation(const FRotator& BaseRot) override;
	virtual FRotator GetBaseRotation() const override;

	virtual void SetBaseOrientation(const FQuat& BaseOrient) override;
	virtual FQuat GetBaseOrientation() const override;

	/**
	* A helper function that calculates the estimated neck position using the specified orientation and position
	* (for example, reported by GetCurrentOrientationAndPosition()).
	*
	* @param CurrentOrientation	(in) The device's current rotation
	* @param CurrentPosition		(in) The device's current position, in its own tracking space
	* @param PositionScale			(in) The 3D scale that will be applied to position.
	* @return			The estimated neck position, calculated using NeckToEye vector from User Profile. Same coordinate space as CurrentPosition.
	*/
	virtual FVector GetNeckPosition(const FQuat& CurrentOrientation, const FVector& CurrentPosition, const FVector& PositionScale) { return FVector::ZeroVector; }

	/**
	* Sets base position offset (in meters). The base position offset is the distance from the physical (0, 0, 0) position
	* to current HMD position (bringing the (0, 0, 0) point to the current HMD position)
	* Note, this vector is set by ResetPosition call; use this method with care.
	* The axis of the vector are the same as in Unreal: X - forward, Y - right, Z - up.
	*
	* @param BaseOffset			(in) the vector to be set as base offset, in meters.
	*/
	virtual void SetBaseOffsetInMeters(const FVector& BaseOffset);

	/**
	* Returns the currently used base position offset, previously set by the
	* ResetPosition or SetBasePositionOffset calls. It represents a vector that translates the HMD's position
	* into (0,0,0) point, in meters.
	* The axis of the vector are the same as in Unreal: X - forward, Y - right, Z - up.
	*
	* @return Base position offset, in meters.
	*/
	virtual FVector GetBaseOffsetInMeters() const;

    virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	virtual void SetScreenPercentage(float InScreenPercentage) override;
	virtual float GetScreenPercentage() const override;

	virtual void ApplyHmdRotation(APlayerController* PC, FRotator& ViewRotation) override;
	virtual void UpdatePlayerCameraRotation(APlayerCameraManager*, struct FMinimalViewInfo& POV) override;

protected:
	virtual TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> CreateNewGameFrame() const = 0;
	virtual TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> CreateNewSettings() const = 0;

	virtual bool DoEnableStereo(bool bStereo, bool bApplyToHmd) = 0;
	virtual void GetCurrentPose(FQuat& CurrentHmdOrientation, FVector& CurrentHmdPosition, bool bUseOrienationForPlayerCamera = false, bool bUsePositionForPlayerCamera = false) = 0;

	virtual void ResetStereoRenderingParams();

	virtual FHMDGameFrame* GetCurrentFrame() const;

	/**
	 * Reads the device configuration, and sets up the stereoscopic rendering parameters
	 */
	virtual void UpdateStereoRenderingParams() {}
	virtual void UpdateHmdRenderInfo() {}
	virtual void UpdateDistortionCaps() {}
	virtual void UpdateHmdCaps() {}
	/** Applies overrides on system when switched to stereo (such as VSync and ScreenPercentage) */
	virtual void ApplySystemOverridesOnStereo(bool force = false) {}

	virtual void ResetControlRotation() const;

	virtual float GetActualScreenPercentage() const;

#if !UE_BUILD_SHIPPING
	void DrawDebugTrackingCameraFrustum(class UWorld* InWorld, const FRotator& ViewRotation, const FVector& ViewLocation);
	void DrawSeaOfCubes(UWorld* World, FVector ViewLocation);
#endif // #if !UE_BUILD_SHIPPING

protected:
	TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> Settings;
	TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> Frame;
	TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> RenderFrame;

	FRotator		DeltaControlRotation;  // used from ApplyHmdRotation

	union
	{
		struct
		{
			/** True, if game frame has started */
			uint64	bFrameStarted : 1;

			/** Indicates if it is necessary to update stereo rendering params */
			uint64	bNeedUpdateStereoRenderingParams : 1;
			uint64  bEnableStereoToHmd : 1;
			uint64	bApplySystemOverridesOnStereo : 1;

			uint64	bNeedEnableStereo : 1;
			uint64	bNeedDisableStereo : 1;

			uint64  bNeedUpdateDistortionCaps : 1;
			uint64  bNeedUpdateHmdCaps : 1;

			/** True, if vision was acquired at previous frame */
			uint64	bHadVisionTracking : 1;
		};
		uint64 Raw;
	} Flags;


	FHMDGameFrame* GetGameFrame()
	{
		return Frame.Get();
	}

	FHMDGameFrame* GetRenderFrame()
	{
		return RenderFrame.Get();
	}

	const FHMDGameFrame* GetGameFrame() const
	{
		return Frame.Get();
	}

	const FHMDGameFrame* GetRenderFrame() const
	{
		return RenderFrame.Get();
	}

#if !UE_BUILD_SHIPPING
	TWeakObjectPtr<AStaticMeshActor> SeaOfCubesActorPtr;
	FVector							 CachedViewLocation;
	FString							 CubeMeshName, CubeMaterialName;
#endif
};

DEFINE_LOG_CATEGORY_STATIC(LogHMD, Log, All);


