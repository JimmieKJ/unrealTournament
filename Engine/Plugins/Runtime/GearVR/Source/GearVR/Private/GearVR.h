// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HeadMountedDisplay.h"
#include "IHeadMountedDisplay.h"
#include "HeadMountedDisplayCommon.h"

#if GEARVR_SUPPORTED_PLATFORMS

#include "SceneViewExtension.h"

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif
	
PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
#include "OVR.h"
#include "VrApi.h"
#include "VrApi_Android.h"
PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS

#include <GLES2/gl2.h>

using namespace OVR;

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

#define OVR_DEFAULT_IPD 0.064f
#define OVR_DEFAULT_EYE_RENDER_TARGET_WIDTH		1024
#define OVR_DEFAULT_EYE_RENDER_TARGET_HEIGHT	1024

//#define OVR_DEBUG_DRAW

class FGearVR;

namespace GearVR
{
/**
 * Converts quat from Oculus ref frame to Unreal
 */
template <typename OVRQuat>
FORCEINLINE FQuat ToFQuat(const OVRQuat& InQuat)
{
	return FQuat(float(-InQuat.z), float(InQuat.x), float(InQuat.y), float(-InQuat.w));
}
/**
 * Converts vector from Oculus to Unreal
 */
template <typename OVRVector3>
FORCEINLINE FVector ToFVector(const OVRVector3& InVec)
{
	return FVector(float(-InVec.z), float(InVec.x), float(InVec.y));
}

/**
 * Converts vector from Oculus to Unreal, also converting meters to UU (Unreal Units)
 */
template <typename OVRVector3>
FORCEINLINE FVector ToFVector_M2U(const OVRVector3& InVec, float WorldToMetersScale)
{
	return FVector(float(-InVec.z * WorldToMetersScale), 
			        float(InVec.x  * WorldToMetersScale), 
					float(InVec.y  * WorldToMetersScale));
}
/**
 * Converts vector from Oculus to Unreal, also converting UU (Unreal Units) to meters.
 */
template <typename OVRVector3>
FORCEINLINE OVRVector3 ToOVRVector_U2M(const FVector& InVec, float WorldToMetersScale)
{
	return OVRVector3(float(InVec.Y * (1.f / WorldToMetersScale)), 
			            float(InVec.Z * (1.f / WorldToMetersScale)), 
					    float(-InVec.X * (1.f / WorldToMetersScale)));
}
/**
 * Converts vector from Oculus to Unreal.
 */
template <typename OVRVector3>
FORCEINLINE OVRVector3 ToOVRVector(const FVector& InVec)
{
	return OVRVector3(float(InVec.Y), float(InVec.Z), float(-InVec.X));
}

FORCEINLINE FMatrix ToFMatrix(const OVR::Matrix4f& vtm)
{
	// Rows and columns are swapped between OVR::Matrix4f and FMatrix
	return FMatrix(
		FPlane(vtm.M[0][0], vtm.M[1][0], vtm.M[2][0], vtm.M[3][0]),
		FPlane(vtm.M[0][1], vtm.M[1][1], vtm.M[2][1], vtm.M[3][1]),
		FPlane(vtm.M[0][2], vtm.M[1][2], vtm.M[2][2], vtm.M[3][2]),
		FPlane(vtm.M[0][3], vtm.M[1][3], vtm.M[2][3], vtm.M[3][3]));
}

FORCEINLINE OVR::Matrix4f ToMatrix4f(const FMatrix mat)
{
	return OVR::Matrix4f(mat.M[0][0],mat.M[1][0],mat.M[2][0],mat.M[3][0],
						mat.M[0][1],mat.M[1][1],mat.M[2][1],mat.M[3][1],
						mat.M[0][2],mat.M[1][2],mat.M[2][2],mat.M[3][2],
						mat.M[0][3],mat.M[1][3],mat.M[2][3],mat.M[3][3]);
}

class FSettings : public FHMDSettings
{
public:
	/** The width and height of the stereo render target */
	FIntPoint		RenderTargetSize;

	/** Clamp warpswap to once every N vsyncs.  1 = 60Hz, 2 = 30Hz, etc. */
	int32			MinimumVsyncs;

	int				CpuLevel;
	int				GpuLevel;

	/** Motion prediction (in seconds). 0 - no prediction */
	double			MotionPredictionInSeconds;

	/** Vector defining center eye offset for head neck model in meters */
	FVector			HeadModel;

	OVR::Vector3f	HmdToEyeViewOffset[2]; 

	FSettings();
	virtual ~FSettings() override {}

	virtual TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> Clone() const override;
};

class FGameFrame : public FHMDGameFrame
{
public:
	ovrPosef				CurEyeRenderPose[2];// eye render pose read at the beginning of the frame
	ovrTracking				CurSensorState;	    // sensor state read at the beginning of the frame

	ovrPosef				EyeRenderPose[2];	// eye render pose actually used
	ovrRigidBodyPosef		HeadPose;			// position of head actually used
	ovrMatrix4f				TanAngleMatrix;
	
	pid_t					GameThreadId;

	FGameFrame();
	virtual ~FGameFrame() {}

	FSettings* GetSettings()
	{
		return (FSettings*)(Settings.Get());
	}

	const FSettings* GetSettings() const
	{
		return (FSettings*)(Settings.Get());
	}

	virtual TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> Clone() const override;

	void PoseToOrientationAndPosition(const ovrPosef& InPose, FQuat& OutOrientation, FVector& OutPosition) const;
};

class FViewExtension : public FHMDViewExtension
{
public:
	FViewExtension(FHeadMountedDisplay* InDelegate);

	//virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
 	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;

	FGameFrame* GetRenderFrame() const { return static_cast<FGameFrame*>(RenderFrame.Get()); }
	FSettings* GetFrameSetting() const { return static_cast<FSettings*>(RenderFrame->GetSettings()); }
	FGearVR* GetGearVR() const;
public:
	class FGearVRCustomPresent* pPresentBridge;
	ovrPosef			NewEyeRenderPose[2];// most recent eye render poses
	ovrRigidBodyPosef	CurHeadPose;		// current position of head
	ovrTracking			NewTracking;		// current tracking

	FEngineShowFlags	ShowFlags;			// a copy of showflags
	bool				bFrameBegun : 1;
};

class FOvrMobileSynced
{
	friend class FGearVRCustomPresent;
protected:
	FOvrMobileSynced(ovrMobile* InMobile, FCriticalSection* InLock) :Mobile(InMobile), pLock(InLock) 
	{
		pLock->Lock();
	}
public:
	~FOvrMobileSynced()
	{
		pLock->Unlock();
	}
	bool IsValid() const { return Mobile != nullptr; }
public:
	ovrMobile* operator->() const { return Mobile; }
	ovrMobile* operator*() const { return Mobile; }

protected:
	ovrMobile*			Mobile;
	FCriticalSection*	pLock;		// used to access OvrMobile on a game thread
};

class FGearVRCustomPresent : public FRHICustomPresent
{
	friend class FViewExtension;
	friend class ::FGearVR;
public:
	FGearVRCustomPresent(jobject InActivityObject, int32 InMinimumVsyncs);

	// Returns true if it is initialized and used.
	bool IsInitialized() const { return bInitialized; }
	bool IsTextureSetCreated() const { return TextureSet;  }

	void UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI, FGameFrame* InRenderFrame);
	FGameFrame* GetRenderFrame() const { check(IsInRenderingThread()); return static_cast<FGameFrame*>(RenderContext->RenderFrame.Get()); }
	FViewExtension* GetRenderContext() const { return static_cast<FViewExtension*>(RenderContext.Get()); }
	FSettings* GetFrameSetting() const { check(IsInRenderingThread()); return static_cast<FSettings*>(RenderContext->RenderFrame->GetSettings()); }

	void BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT);
	void FinishRendering();
	void UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI);

	void Reset();
	void Shutdown();

	void Init();

	// Implementation of FRHICustomPresent
	// Resets Viewport-specific resources.
	virtual void OnBackBufferResize() override;
	// Returns true if Engine should perform its own Present.
	virtual bool Present(int& SyncInterval) override;

	// Called when rendering thread is acquired
	virtual void OnAcquireThreadOwnership() override;
	// Called when rendering thread is released
	virtual void OnReleaseThreadOwnership() override;

	// Allocates render target texture
	// If returns false then a default RT texture will be used.
	bool AllocateRenderTargetTexture(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples);

	FOvrMobileSynced GetMobileSynced() { return FOvrMobileSynced(OvrMobile, &OvrMobileLock); }
	void DoEnterVRMode();
	void DoLeaveVRMode();

	pid_t GetRenderThreadId() const { return RenderThreadId; }
private:
	void DicedBlit(uint32 SourceX, uint32 SourceY, uint32 DestX, uint32 DestY, uint32 Width, uint32 Height, uint32 NumXSteps, uint32 NumYSteps);

protected:
	void SetRenderContext(FHMDViewExtension* InRenderContext);

protected: // data
	TSharedPtr<FViewExtension, ESPMode::ThreadSafe> RenderContext;
	bool											bInitialized;

	// should be accessed only on a RenderThread!
	ovrFrameParms							FrameParms;
	ovrPerformanceParms						DefaultPerfParms;
	TRefCountPtr<class FOpenGLTexture2DSet>	TextureSet;
	int32									MinimumVsyncs;

	ovrMobile*								OvrMobile;		// to be accessed only on RenderThread (or, when RT is suspended)
	pid_t									RenderThreadId; // the rendering thread id where EnterVrMode was called.
	FCriticalSection						OvrMobileLock;	// used to access OvrMobile_RT/HmdInfo_RT on a game thread
	ovrJava									JavaRT;			// Rendering thread Java obj
	jobject									ActivityObject;
};
}  // namespace GearVR

using namespace GearVR;

/**
 * GearVR Head Mounted Display
 */
class FGearVR : public FHeadMountedDisplay
{
	friend class FViewExtension;
	friend class FGearVRCustomPresent;
public:
	/** IHeadMountedDisplay interface */
	virtual bool OnStartGameFrame( FWorldContext& WorldContext ) override;
	virtual bool IsHMDConnected() override;
	virtual EHMDDeviceType::Type GetHMDDeviceType() const override;
	virtual bool GetHMDMonitorInfo(MonitorInfo&) override;

	virtual TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> GetViewExtension() override;
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;
	virtual void OnScreenModeChange(EWindowMode::Type WindowMode) override;

	/** IStereoRendering interface */
	virtual bool EnableStereo(bool bStereo) override;
	virtual void CalculateStereoViewOffset(const EStereoscopicPass StereoPassType, const FRotator& ViewRotation,
										   const float MetersToWorld, FVector& ViewLocation) override;
	virtual FMatrix GetStereoProjectionMatrix(const EStereoscopicPass StereoPassType, const float FOV) const override;
	virtual void InitCanvasFromView(FSceneView* InView, UCanvas* Canvas) override;

	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& Viewport, SViewport* ViewportWidget) override;
	virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;
	virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) override;

	virtual uint32 GetNumberOfBufferedFrames() const override { return 1; }
	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override;

	virtual bool ShouldUseSeparateRenderTarget() const override
	{
		check(IsInGameThread());
#if OVR_DEBUG_DRAW
		return false;
#else
		return IsStereoEnabled();
#endif
	}
	virtual void GetOrthoProjection(int32 RTWidth, int32 RTHeight, float OrthoDistance, FMatrix OrthoProjection[2]) const override;

    /** ISceneViewExtension interface */
    virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
    virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;

	/** Positional tracking control methods */
	virtual bool IsPositionalTrackingEnabled() const override;
	virtual bool EnablePositionalTracking(bool enable) override;

	virtual bool IsInLowPersistenceMode() const override;

	/** Resets orientation by setting roll and pitch to 0, 
	    assuming that current yaw is forward direction and assuming
		current position as 0 point. */
	virtual void ResetOrientationAndPosition(float yaw = 0.f) override;

	void RebaseObjectOrientationAndPosition(FVector& OutPosition, FQuat& OutOrientation) const override;

	virtual FString GetVersionString() const override;

	virtual void DrawDebug(UCanvas* Canvas) override;

	/** Constructor */
	FGearVR();

	/** Destructor */
	virtual ~FGearVR();

	TRefCountPtr<FGearVRCustomPresent> pGearVRBridge;

	void StartOVRGlobalMenu();
	void StartOVRQuitMenu();

	void SetCPUAndGPULevels(int32 CPULevel, int32 GPULevel);
	bool IsPowerLevelStateMinimum() const;
	bool IsPowerLevelStateThrottled() const;
	bool AreHeadPhonesPluggedIn() const;
	float GetTemperatureInCelsius() const;
	float GetBatteryLevel() const;

private:
	FGearVR* getThis() { return this; }

	void ShutdownRendering();

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

	/** Load/save settings */
	void LoadFromIni();

	/** Applies overrides on system when switched to stereo (such as VSync and ScreenPercentage) */
	void ApplySystemOverridesOnStereo(bool force = false);
	/** Saves system values before applying overrides. */
	void SaveSystemValues();
	/** Restores system values after overrides applied. */
	void RestoreSystemValues();

	void PoseToOrientationAndPosition(const ovrPosef& InPose, FQuat& OutOrientation, FVector& OutPosition) const;

protected:
	virtual TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> CreateNewGameFrame() const override;
	virtual TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> CreateNewSettings() const override;

	virtual bool DoEnableStereo(bool bStereo, bool bApplyToHmd) override;
	virtual void ResetStereoRenderingParams() override;

	virtual void GetCurrentPose(FQuat& CurrentHmdOrientation, FVector& CurrentHmdPosition, bool bUseOrienationForPlayerCamera = false, bool bUsePositionForPlayerCamera = false) override;

	// Returns eye poses instead of head pose.
	bool GetEyePoses(const FGameFrame& InFrame, ovrPosef outEyePoses[2], ovrTracking& outTracking);

	FGameFrame* GetFrame() const;

	void EnterVRMode();
	void LeaveVRMode();

	FOvrMobileSynced GetMobileSynced() 
	{ 
		check(pGearVRBridge);
		return pGearVRBridge->GetMobileSynced();
	}
private: // data

	FRotator			DeltaControlRotation;    // same as DeltaControlOrientation but as rotator

	ovrHmdInfo			HmdInfo;	// to be accessed only on RenderThread (or, when RT is suspended)
	ovrJava				JavaGT;		// game thread (main) Java VM obj	

	float				ResetToYaw; // used for delayed orientation/position reset

	union
	{
		struct
		{
			/** To perform delayed orientation and position reset */
			uint64	NeedResetOrientationAndPosition : 1;
		};
		uint64 Raw;
	} OCFlags;

	FGameFrame* GetGameFrame()
	{
		return (FGameFrame*)(Frame.Get());
	}

	const FGameFrame* GetGameFrame() const
	{
		return (const FGameFrame*)(Frame.Get());
	}

	FSettings* GetSettings()
	{
		return (FSettings*)(Settings.Get());
	}

	const FSettings* GetSettings() const
	{
		return (const FSettings*)(Settings.Get());
	}
};

#endif //GEARVR_SUPPORTED_PLATFORMS
