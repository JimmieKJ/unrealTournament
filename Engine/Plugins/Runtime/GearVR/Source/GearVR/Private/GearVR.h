// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IGearVRPlugin.h"
#include "HeadMountedDisplay.h"
#include "IHeadMountedDisplay.h"
#include "HeadMountedDisplayCommon.h"

#if GEARVR_SUPPORTED_PLATFORMS

#include "SceneViewExtension.h"

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif
	
#include "OVRVersion.h"

#include "../Src/Kernel/OVR_Math.h"
#include "../Src/Kernel/OVR_Threads.h"
#include "../Src/Kernel/OVR_Color.h"
#include "../Src/Kernel/OVR_Timer.h"

#include "OVR.h"
#include "VrApi.h"
#include "VrApi_Android.h"

#include <GLES2/gl2.h>

using namespace OVR;

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

#define OVR_DEFAULT_IPD 0.064f

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
	int32			RenderTargetWidth;
	int32			RenderTargetHeight;

	/** Clamp warpswap to once every N vsyncs.  1 = 60Hz, 2 = 30Hz, etc. */
	int32			MinimumVsyncs;

	/** Motion prediction (in seconds). 0 - no prediction */
	double			MotionPredictionInSeconds;

	/** Vector defining center eye offset for head neck model in meters */
	FVector			HeadModel;

	OVR::Vector3f	HmdToEyeViewOffset[2]; 

	ovrModeParms	VrModeParms;

	FSettings();
	virtual ~FSettings() override {}

	virtual TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> Clone() const override;
};

class FGameFrame : public FHMDGameFrame
{
public:
	ovrPosef				CurEyeRenderPose[2];// eye render pose read at the beginning of the frame
	ovrSensorState			CurSensorState;	    // sensor state read at the beginning of the frame

	ovrPosef				EyeRenderPose[2];	// eye render pose actually used
	ovrPosef				HeadPose;			// position of head actually used
	ovrMatrix4f				TanAngleMatrix;

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

 	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
 	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;

	FGameFrame* GetRenderFrame() const { return static_cast<FGameFrame*>(RenderFrame.Get()); }
	FSettings* GetFrameSetting() const { return static_cast<FSettings*>(RenderFrame->GetSettings()); }
public:
	class FGearVRCustomPresent* pPresentBridge;
	ovrMobile*			OvrMobile;
	ovrPosef			CurEyeRenderPose[2];// most recent eye render poses
	ovrPosef			CurHeadPose;		// current position of head

	FEngineShowFlags	ShowFlags; // a copy of showflags
	bool				bFrameBegun : 1;
};

class FGearVRCustomPresent : public FRHICustomPresent
{
	friend class FViewExtension;
public:
	FGearVRCustomPresent(int32 MinimumVsyncs);

	// Returns true if it is initialized and used.
	bool IsInitialized() const { return bInitialized; }

	void UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI, FGameFrame* InRenderFrame);
	FGameFrame* GetRenderFrame() const { check(IsInRenderingThread()); return static_cast<FGameFrame*>(RenderContext->RenderFrame.Get()); }
	FViewExtension* GetRenderContext() const { return static_cast<FViewExtension*>(RenderContext.Get()); }
	FSettings* GetFrameSetting() const { check(IsInRenderingThread()); return static_cast<FSettings*>(RenderContext->RenderFrame->GetSettings()); }

	void BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT);
	void FinishRendering();
	void UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI);

	void Reset();
	void Shutdown() { Reset(); }

	void Init();

	// Implementation of FRHICustomPresent
	// Resets Viewport-specific resources.
	virtual void OnBackBufferResize() override;
	// Returns true if Engine should perform its own Present.
	virtual bool Present(int& SyncInterval) override;

private:
	void DicedBlit(uint32 SourceX, uint32 SourceY, uint32 DestX, uint32 DestY, uint32 Width, uint32 Height, uint32 NumXSteps, uint32 NumYSteps);

protected:
	void SetRenderContext(FHMDViewExtension* InRenderContext);

protected: // data
	TSharedPtr<FViewExtension, ESPMode::ThreadSafe> RenderContext;
	bool											bInitialized;

	// should be accessed only on a RenderThread!
	ovrTimeWarpParms	SwapParms;
	GLuint              CurrentSwapChainIndex;
	GLuint              SwapChainTextures[2][3];
	GLuint				RTTexId;
	int32				MinimumVsyncs;
};
}  // namespace GearVR

using namespace GearVR;

/**
 * GearVR Head Mounted Display
 */
class FGearVR : public FHeadMountedDisplay
{
	friend class FViewExtension;
public:
	static void PreInit();

	/** IHeadMountedDisplay interface */
	virtual bool OnStartGameFrame() override;
	virtual bool IsHMDConnected() override { return true; }
	virtual EHMDDeviceType::Type GetHMDDeviceType() const override;
	virtual bool GetHMDMonitorInfo(MonitorInfo&) override;

    virtual void GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition) override;
	virtual FVector GetNeckPosition(const FQuat& CurrentOrientation, const FVector& CurrentPosition, const FVector& PositionScale) override;

	virtual TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> GetViewExtension() override;
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;
	virtual void OnScreenModeChange(EWindowMode::Type WindowMode) override;

	/** IStereoRendering interface */
	virtual void CalculateStereoViewOffset(const EStereoscopicPass StereoPassType, const FRotator& ViewRotation, 
										   const float MetersToWorld, FVector& ViewLocation) override;
	virtual FMatrix GetStereoProjectionMatrix(const EStereoscopicPass StereoPassType, const float FOV) const override;
	virtual void InitCanvasFromView(FSceneView* InView, UCanvas* Canvas) override;

	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& Viewport, SViewport* ViewportWidget) override;
	virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;
	virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) override;

	virtual bool ShouldUseSeparateRenderTarget() const override
	{
		check(IsInGameThread());
		return IsStereoEnabled();
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

	virtual FString GetVersionString() const override;

	virtual void DrawDebug(UCanvas* Canvas) override;

	/** Constructor */
	FGearVR();

	/** Destructor */
	virtual ~FGearVR();

	TRefCountPtr<FGearVRCustomPresent> pGearVRBridge;

	void ShutdownRendering();
	void StartOVRGlobalMenu();

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
	bool GetEyePoses(const FGameFrame& InFrame, ovrPosef outEyePoses[2], ovrSensorState& outSensorState);

	FGameFrame* GetFrame() const;

	void EnterVrMode(const ovrModeParms& InVrModeParms);
private: // data

	FRotator			DeltaControlRotation;    // same as DeltaControlOrientation but as rotator

	ovrMobile*			OvrMobile_RT; // to be accessed only on RenderThread (or, when RT is suspended)
	ovrHmdInfo			HmdInfo_RT;   // to be accessed only on RenderThread (or, when RT is suspended)
	FCriticalSection	OvrMobileLock;// used to access OvrMobile_RT/HmdInfo_RT on a game thread

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
