// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IGearVRPlugin.h"
#include "HeadMountedDisplay.h"
#include "IHeadMountedDisplay.h"

#if GEARVR_SUPPORTED_PLATFORMS

#include "SceneViewExtension.h"

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif
	
#include "OVRVersion.h"

#include "../Src/Kernel/OVR_Math.h"
#include "../Src/Kernel/OVR_Threads.h"
#include "../Src/OVR_CAPI.h"
#include "../Src/Kernel/OVR_Color.h"
#include "../Src/Kernel/OVR_Timer.h"

#include "OVR.h"
#include "VrApi.h"

#include <GLES2/gl2.h>

using namespace OVR;

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

#define OVR_DEFAULT_IPD 0.064f


/**
 * GearVR Head Mounted Display
 */
class FGearVR : public IHeadMountedDisplay, public ISceneViewExtension
{
public:
	static void PreInit();

	/** IHeadMountedDisplay interface */
	virtual bool IsHMDConnected() override { return true; }
	virtual bool IsHMDEnabled() const override;
	virtual void EnableHMD(bool allow = true) override;
	virtual EHMDDeviceType::Type GetHMDDeviceType() const override;
	virtual bool GetHMDMonitorInfo(MonitorInfo&) override;

	virtual bool DoesSupportPositionalTracking() const override;
	virtual bool HasValidTrackingPosition() const override;
	virtual void GetPositionalTrackingCameraProperties(FVector& OutOrigin, FRotator& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const override;

	virtual void SetInterpupillaryDistance(float NewInterpupillaryDistance) override;
	virtual float GetInterpupillaryDistance() const override;
    //virtual float GetFieldOfViewInRadians() const override;
	virtual void GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const override;

    virtual void GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition) override;
	virtual void ApplyHmdRotation(APlayerController* PC, FRotator& ViewRotation) override;
	virtual void UpdatePlayerCameraRotation(APlayerCameraManager*, struct FMinimalViewInfo& POV) override;

	virtual bool IsChromaAbCorrectionEnabled() const override;

	virtual class ISceneViewExtension* GetViewExtension() override;
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;
	virtual void OnScreenModeChange(EWindowMode::Type WindowMode) override;

	/** IStereoRendering interface */
	virtual bool IsStereoEnabled() const override;
	virtual bool EnableStereo(bool stereo = true) override;
    virtual void AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
	virtual void CalculateStereoViewOffset(const EStereoscopicPass StereoPassType, const FRotator& ViewRotation, 
										   const float MetersToWorld, FVector& ViewLocation) override;
	virtual FMatrix GetStereoProjectionMatrix(const EStereoscopicPass StereoPassType, const float FOV) const override;
	virtual void InitCanvasFromView(FSceneView* InView, UCanvas* Canvas) override;
	virtual void PushViewportCanvas(EStereoscopicPass StereoPass, FCanvas *InCanvas, UCanvas *InCanvasObject, FViewport *InViewport) const override;
	virtual void PushViewCanvas(EStereoscopicPass StereoPass, FCanvas *InCanvas, UCanvas *InCanvasObject, FSceneView *InView) const override;
	virtual void GetEyeRenderParams_RenderThread(EStereoscopicPass StereoPass, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const override;

	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& Viewport, SViewport* ViewportWidget) override;
	virtual void CalculateRenderTargetSize(uint32& InOutSizeX, uint32& InOutSizeY) const override;
	virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) const override;

	virtual bool ShouldUseSeparateRenderTarget() const override
	{
		check(IsInGameThread());
		return IsStereoEnabled();
	}

    /** ISceneViewExtension interface */
    virtual void ModifyShowFlags(FEngineShowFlags& ShowFlags) override;
    virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
    virtual void PreRenderView_RenderThread(FSceneView& InView) override;
	virtual void PreRenderViewFamily_RenderThread(FSceneViewFamily& InViewFamily) override;

	/** Positional tracking control methods */
	virtual bool IsPositionalTrackingEnabled() const override;
	virtual bool EnablePositionalTracking(bool enable) override;

	virtual bool IsHeadTrackingAllowed() const override;

	virtual bool IsInLowPersistenceMode() const override;
	virtual void EnableLowPersistenceMode(bool Enable = true) override;

	/** Resets orientation by setting roll and pitch to 0, 
	    assuming that current yaw is forward direction and assuming
		current position as 0 point. */
	virtual void ResetOrientationAndPosition(float yaw = 0.f) override;

	virtual void DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FSceneView& View, const FIntPoint& TextureSize) override;
	virtual void UpdateScreenSettings(const FViewport*) override;

	virtual void FinishRenderingFrame_RenderThread(FRHICommandListImmediate& RHICmdList) override;

	/** Constructor */
	FGearVR();

	/** Destructor */
	virtual ~FGearVR();

	/** @return	True if the HMD was initialized OK */
	bool IsInitialized() const;

	class FGearVRBridge : public FRHICustomPresent
	{
	public:
		FGearVRBridge(FGearVR* plugin, uint32 RenderTargetWidth, uint32 RenderTargetHeight, float FOV );

		// Returns true if it is initialized and used.
		bool IsInitialized() const { return bInitialized; }

		void BeginRendering();
		void FinishRendering();
		void UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI);

		void Reset();
		void Shutdown() { Reset(); }

		void Init();

		// Implementation of FRHICustomPresent
		// Resets Viewport-specific resources.
		virtual void OnBackBufferResize() override;
		// Returns true if Engine should perform its own Present.
		virtual bool Present(int SyncInterval) override;

	private:
		void DicedBlit(uint32 SourceX, uint32 SourceY, uint32 DestX, uint32 DestY, uint32 Width, uint32 Height, uint32 NumXSteps, uint32 NumYSteps);

	public:
		bool				bFirstTime;
		TimeWarpParms		SwapParms;

	private:
		FGearVR*			Plugin;
		bool				bInitialized;

		GLuint              CurrentSwapChainIndex;
		GLuint              SwapChainTextures[2][3];

		uint32				RenderTargetWidth;
		uint32				RenderTargetHeight;
		float				FOV;
	};

	TRefCountPtr<FGearVRBridge> pGearVRBridge;

	void ShutdownRendering();

private:
	FGearVR* getThis() { return this; }

	/**
	 * Starts up the GearVR device
	 */
	void Startup();

	/**
	 * Shuts down GearVR
	 */
	void Shutdown();

	void ApplicationPauseDelegate();
	void ApplicationResumeDelegate();

	/**
	 * Reads the device configuration, and sets up the stereoscopic rendering parameters
	 */
	void UpdateStereoRenderingParams();
	void UpdateHmdRenderInfo();

    /**
     * Updates the view point reflecting the current HMD orientation. 
     */
	static void UpdatePlayerViewPoint(const FQuat& CurrentOrientation, const FVector& CurrentPosition, const FVector& LastHmdPosition, const FQuat& DeltaControlOrientation, const FQuat& BaseViewOrientation, const FVector& BaseViewPosition, FRotator& ViewRotation, FVector& ViewLocation);

	/**
	 * Converts quat from Oculus ref frame to Unreal
	 */
	template <typename OVRQuat>
	FORCEINLINE FQuat ToFQuat(const OVRQuat& InQuat) const
	{
		return FQuat(float(-InQuat.z), float(InQuat.x), float(InQuat.y), float(-InQuat.w));
	}
	/**
	 * Converts vector from Oculus to Unreal
	 */
	template <typename OVRVector3>
	FORCEINLINE FVector ToFVector(const OVRVector3& InVec) const
	{
		return FVector(float(-InVec.z), float(InVec.x), float(InVec.y));
	}

	/**
	 * Converts vector from Oculus to Unreal, also converting meters to UU (Unreal Units)
	 */
	template <typename OVRVector3>
	FORCEINLINE FVector ToFVector_M2U(const OVRVector3& InVec) const
	{
		return FVector(float(-InVec.z * WorldToMetersScale), 
			           float(InVec.x  * WorldToMetersScale), 
					   float(InVec.y  * WorldToMetersScale));
	}
	/**
	 * Converts vector from Oculus to Unreal, also converting UU (Unreal Units) to meters.
	 */
	template <typename OVRVector3>
	FORCEINLINE OVRVector3 ToOVRVector_U2M(const FVector& InVec) const
	{
		return OVRVector3(float(InVec.Y * (1.f / WorldToMetersScale)), 
			              float(InVec.Z * (1.f / WorldToMetersScale)), 
					     float(-InVec.X * (1.f / WorldToMetersScale)));
	}
	/**
	 * Converts vector from Oculus to Unreal.
	 */
	template <typename OVRVector3>
	FORCEINLINE OVRVector3 ToOVRVector(const FVector& InVec) const
	{
		return OVRVector3(float(InVec.Y), float(InVec.Z), float(-InVec.X));
	}

	/** Converts vector from meters to UU (Unreal Units) */
	FORCEINLINE FVector MetersToUU(const FVector& InVec) const
	{
		return InVec * WorldToMetersScale;
	}
	/** Converts vector from UU (Unreal Units) to meters */
	FORCEINLINE FVector UUToMeters(const FVector& InVec) const
	{
		return InVec * (1.f / WorldToMetersScale);
	}

	FORCEINLINE FMatrix ToFMatrix(const OVR::Matrix4f& vtm) const
	{
		// Rows and columns are swapped between OVR::Matrix4f and FMatrix
		return FMatrix(
			FPlane(vtm.M[0][0], vtm.M[1][0], vtm.M[2][0], vtm.M[3][0]),
			FPlane(vtm.M[0][1], vtm.M[1][1], vtm.M[2][1], vtm.M[3][1]),
			FPlane(vtm.M[0][2], vtm.M[1][2], vtm.M[2][2], vtm.M[3][2]),
			FPlane(vtm.M[0][3], vtm.M[1][3], vtm.M[2][3], vtm.M[3][3]));
	}

	FORCEINLINE OVR::Matrix4f ToMatrix4f(const FMatrix mat) const
	{
		return OVR::Matrix4f(mat.M[0][0],mat.M[1][0],mat.M[2][0],mat.M[3][0],
							 mat.M[0][1],mat.M[1][1],mat.M[2][1],mat.M[3][1],
							 mat.M[0][2],mat.M[1][2],mat.M[2][2],mat.M[3][2],
							 mat.M[0][3],mat.M[1][3],mat.M[2][3],mat.M[3][3]);
	}

	/**
	 * Called when state changes from 'stereo' to 'non-stereo'. Suppose to distribute
	 * the event further to user's code (?).
	 */
	void OnOculusStateChange(bool bIsEnabledNow);

	/** Load/save settings */
	void LoadFromIni();

	/** Applies overrides on system when switched to stereo (such as VSync and ScreenPercentage) */
	void ApplySystemOverridesOnStereo(bool force = false);
	/** Saves system values before applying overrides. */
	void SaveSystemValues();
	/** Restores system values after overrides applied. */
	void RestoreSystemValues();

	void ResetControlRotation() const;

	void PoseToOrientationAndPosition(const ovrPosef& InPose, FQuat& OutOrientation, FVector& OutPosition) const;

private: // data

	/** Whether or not the Oculus was successfully initialized */
	enum EInitStatus
	{
		eNotInitialized   = 0x00,
		eStartupExecuted  = 0x01,
		eInitialized      = 0x02,
	};
	int InitStatus; // see bitmask EInitStatus

	/** Whether stereo is currently on or off. */
	bool bStereoEnabled;

	/** Whether or not switching to stereo is allowed */
	bool bHMDEnabled;

	/** Indicates if it is necessary to update stereo rendering params */
	bool bNeedUpdateStereoRenderingParams;

	/** Debugging:  Whether or not the stereo rendering settings have been manually overridden by an exec command.  They will no longer be auto-calculated */
    bool bOverrideStereo;

	/** Debugging:  Whether or not the IPD setting have been manually overridden by an exec command. */
	bool bOverrideIPD;

	/** Debugging:  Whether or not the distortion settings have been manually overridden by an exec command.  They will no longer be auto-calculated */
	bool bOverrideDistortion;

	/** Debugging: Allows changing internal params, such as screen size, eye-to-screen distance, etc */
	bool bDevSettingsEnabled;

	bool bOverrideFOV;

	/** Whether or not to override game VSync setting when switching to stereo */
	bool bOverrideVSync;
	/** Overridden VSync value */
	bool bVSync;

	/** Saved original values for VSync and ScreenPercentage. */
	bool bSavedVSync;
	float SavedScrPerc;

	/** Whether or not to override game ScreenPercentage setting when switching to stereo */
	bool bOverrideScreenPercentage;
	/** Overridden ScreenPercentage value */
	float ScreenPercentage;

	/** Ideal ScreenPercentage value for the HMD */
	float IdealScreenPercentage;

	/** Allows renderer to finish current frame. Setting this to 'true' may reduce the total 
	 *  framerate (if it was above vsync) but will reduce latency. */
	bool bAllowFinishCurrentFrame;

	/** Interpupillary distance, in meters (user configurable) */
	float InterpupillaryDistance;

	/** World units (UU) to Meters scale.  Read from the level, and used to transform positional tracking data */
	float WorldToMetersScale;
	/** Whether world-to-meters scale is overriden or not. */
	bool bWorldToMetersOverride; 

	/** User-tunable modification to the interpupillary distance */
	float UserDistanceToScreenModifier;

	/** The FOV to render at (radians), based on the physical characteristics of the device */
	float HFOVInRadians; // horizontal
	float VFOVInRadians; // vertical

	/** The width and height of the stereo render target */
	int32 RenderTargetWidth;
	int32 RenderTargetHeight;

	/** Motion prediction (in seconds). 0 - no prediction */
	double MotionPredictionInSeconds;

	/** Chromatic aberration correction on/off */
	bool bChromaAbCorrectionEnabled;

	/** Whether or not 2D stereo settings overridden. */
	bool bOverride2D;
	/** HUD stereo offset */
	float HudOffset;
	/** Screen center adjustment for 2d elements */
	float CanvasCenterOffset;

	/** Turns on/off updating view's orientation/position on a RenderThread. When it is on,
	    latency should be significantly lower. 
		See 'HMD UPDATEONRT ON|OFF' console command.
	*/
	bool				bUpdateOnRT;
	mutable OVR::Lock	UpdateOnRTLock;

	/** Enforces headtracking to work even in non-stereo mode (for debugging or screenshots). 
	    See 'MOTION ENFORCE' console command. */
	bool bHeadTrackingEnforced;

	/** Optional far clipping plane for projection matrix */
	float					NearClippingPlane;
	/** Optional far clipping plane for projection matrix */
	float					FarClippingPlane;

	/** Player's orientation tracking */
	FQuat					CurHmdOrientation;

	FRotator				DeltaControlRotation;    // same as DeltaControlOrientation but as rotator
	FQuat					DeltaControlOrientation; // same as DeltaControlRotation but as quat

	FVector					CurHmdPosition;

	FQuat					LastHmdOrientation; // contains last APPLIED ON GT HMD orientation
	FVector					LastHmdPosition;	// contains last APPLIED ON GT HMD position 

	/** HMD base values, specify forward orientation and zero pos offset */
	OVR::Vector3f			BaseOffset;      // base position, in Oculus coords
	FQuat					BaseOrientation; // base orientation

	uint32					OvrInited_RenderThread;

	ovrMobile*				OvrMobile;
	ovrModeParms			VrModeParms;

	ovrHmdInfo				HmdInfo;
	ovrSensorDesc			SensorDesc;

	FIntPoint				EyeViewportSize; // size of the viewport (for one eye). At the moment it is a half of RT.
	
	OVR::Lock				StereoParamsLock;

	// Params accessible from rendering thread. Should be filled at the beginning
	// of the rendering thread under the StereoParamsLock.
	struct FRenderParams
	{
		FVector					LastHmdPosition;	// contains last APPLIED ON GT HMD position 
		FQuat					DeltaControlOrientation;
		ovrPoseStatef			EyeRenderPose[2];

		FQuat					CurHmdOrientation;
		FVector					CurHmdPosition;
		bool					bFrameBegun;

		FEngineShowFlags		ShowFlags; // a copy of showflags

		FRenderParams(FGearVR* plugin);
	} RenderParams_RenderThread;


	/** True, if pos tracking is enabled */
	bool						bHmdPosTracking;
};

DEFINE_LOG_CATEGORY_STATIC(LogHMD, Log, All);

#endif //GEARVR_SUPPORTED_PLATFORMS
