// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IHeadMountedDisplay.h"
#include "HeadMountedDisplayCommon.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#include "SceneViewExtension.h"

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif

#if PLATFORM_WINDOWS
	#define OVR_D3D_VERSION 11
	#define OVR_GL
#elif PLATFORM_MAC
	#define OVR_VISION_ENABLED
    #define OVR_GL
#endif

#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	#include <OVR_Version.h>
	#include <OVR_CAPI_0_7_0.h>
	#include <OVR_CAPI_Keys.h>
	#include <Extras/OVR_Math.h>
#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

struct FDistortionVertex;
class FOculusRiftHMD;

namespace OculusRift 
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

	FORCEINLINE OVR::Recti ToOVRRecti(const FIntRect& rect)
	{
		return OVR::Recti(rect.Min.X, rect.Min.Y, rect.Size().X, rect.Size().Y);
	}

class FSettings : public FHMDSettings
{
public:
	const int TexturePaddingPerEye = 12; // padding, in pixels, per eye (total padding will be doubled)
	enum EQueueAheadStatus
	{
		EQA_Default = 0,
		EQA_Enabled = 1,
		EQA_Disabled = 2
	};

	ovrEyeRenderDesc		EyeRenderDesc[2];			// 0 - left, 1 - right, same as Views
	ovrMatrix4f				EyeProjectionMatrices[2];	// 0 - left, 1 - right, same as Views
	ovrMatrix4f				PerspectiveProjection[2];	// used for calc ortho projection matrices
	ovrFovPort				EyeFov[2];					// 0 - left, 1 - right, same as Views
	ovrLayer_Union			EyeLayer;

	FIntPoint				RenderTargetSize;
	float					PixelDensity;

	EQueueAheadStatus		QueueAheadStatus;

	unsigned				SupportedTrackingCaps;
	unsigned				SupportedHmdCaps;

	unsigned				TrackingCaps;
	unsigned				HmdCaps;

	enum MirrorWindowModeType
	{
		eMirrorWindow_Distorted,
		eMirrorWindow_Undistorted,
		eMirrorWindow_SingleEye,

		eMirrorWindow_Total_
	};
	MirrorWindowModeType	MirrorWindowMode;

	FSettings();
	virtual ~FSettings() override {}

	virtual TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> Clone() const override;

	float GetTexturePaddingPerEye() const { return TexturePaddingPerEye * GetActualScreenPercentage()/100.f; }
};

class FGameFrame : public FHMDGameFrame
{
public:
	ovrPosef				CurEyeRenderPose[2];// eye render pose read at the beginning of the frame
	ovrTrackingState		CurTrackingState;	// tracking state read at the beginning of the frame

	ovrPosef				EyeRenderPose[2];	// eye render pose actually used
	ovrPosef				HeadPose;			// position of head actually used

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

	// Returns eye poses calculated from head pose. Head pose is available via ovrTrackingState.
	void GetEyePoses(ovrHmd Hmd, ovrPosef outEyePoses[2], ovrTrackingState& outTrackingState) const;
};

// View extension class that contains all the rendering-specific data (also referred as 'RenderContext').
// Each call to RT will use an unique instance of this class, attached to ViewFamily.
class FViewExtension : public FHMDViewExtension
{
public:
	FViewExtension(FHeadMountedDisplay* InDelegate);

 	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
 	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;

	FGameFrame* GetRenderFrame() const { return static_cast<FGameFrame*>(RenderFrame.Get()); }
	FSettings* GetFrameSettings() const { return static_cast<FSettings*>(RenderFrame->GetSettings()); }

public:
	class FCustomPresent* pPresentBridge;
	IRendererModule*	RendererModule;
	ovrHmd				Hmd;
	ovrPosef			CurEyeRenderPose[2];// most recent eye render poses
	ovrPosef			CurHeadPose;		// current position of head

	FEngineShowFlags	ShowFlags; // a copy of showflags
	bool				bFrameBegun : 1;
};

class FCustomPresent : public FRHICustomPresent
{
public:
	FCustomPresent()
		: FRHICustomPresent(nullptr)
		, Hmd(nullptr)
		, MirrorTexture(nullptr)
		, NeedToKillHmd(0)
		, bInitialized(false)
		, bNeedReAllocateTextureSet(true)
		, bNeedReAllocateMirrorTexture(true)
	{}

	// Returns true if it is initialized and used.
	bool IsInitialized() const { return bInitialized; }

	virtual void BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT) = 0;
	virtual void FinishRendering() = 0;

	// Resets Viewport-specific pointers (BackBufferRT, SwapChain).
	virtual void OnBackBufferResize() override;

	virtual void Reset() = 0;
	virtual void Shutdown() = 0;

	// Returns true if Engine should perform its own Present.
	virtual bool Present(int32& SyncInterval) override;

	virtual void UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI, FGameFrame* InRenderFrame);
	FGameFrame* GetRenderFrame() const { check(IsInRenderingThread()); return static_cast<FGameFrame*>(RenderContext->RenderFrame.Get()); }
	FViewExtension* GetRenderContext() const { return static_cast<FViewExtension*>(RenderContext.Get()); }
	FSettings* GetFrameSetting() const { check(IsInRenderingThread()); return static_cast<FSettings*>(RenderContext->RenderFrame->GetSettings()); }

	virtual void SetHmd(ovrHmd InHmd) { Hmd = InHmd; }

	// marking textures invalid, that will force re-allocation of ones
	void MarkTexturesInvalid();

	bool AreTexturesMarkedAsInvalid() const { return bNeedReAllocateTextureSet; }

	virtual FTexture2DRHIRef GetMirrorTexture() { return MirrorTextureRHI; }

	// Allocates render target texture
	// If returns false then a default RT texture will be used.
	virtual bool AllocateRenderTargetTexture(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples) = 0;

	// Can be called on any thread. Returns true, if HMD is marked as invalid and it must be killed.
	// This function resets the flag.
	bool NeedsToKillHmd()
	{
		return FPlatformAtomics::InterlockedExchange(&NeedToKillHmd, 0) != 0;
	}

protected:
	void SetRenderContext(FHMDViewExtension* InRenderContext);

protected: // data
	ovrHmd				Hmd;
	TSharedPtr<FViewExtension, ESPMode::ThreadSafe> RenderContext;

	// Mirror texture
	ovrTexture*			MirrorTexture;
	FTexture2DRHIRef	MirrorTextureRHI;

	volatile int32		NeedToKillHmd;
	bool				bInitialized : 1;
	bool				bNeedReAllocateTextureSet : 1;
	bool				bNeedReAllocateMirrorTexture : 1;
};

} // namespace OculusRift

using namespace OculusRift;

/**
 * Oculus Rift Head Mounted Display
 */
class FOculusRiftHMD : public FHeadMountedDisplay
{
	friend class FViewExtension;
	friend class UOculusFunctionLibrary;
	friend class FOculusRiftPlugin;
public:
	/** IHeadMountedDisplay interface */
	virtual bool OnStartGameFrame( FWorldContext& WorldContext ) override;

	virtual bool IsHMDConnected() override;
	virtual EHMDDeviceType::Type GetHMDDeviceType() const override;
	virtual bool GetHMDMonitorInfo(MonitorInfo&) override;

	virtual bool DoesSupportPositionalTracking() const override;
	virtual bool HasValidTrackingPosition() override;
	virtual void GetPositionalTrackingCameraProperties(FVector& OutOrigin, FQuat& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const override;
	virtual void RebaseObjectOrientationAndPosition(FVector& OutPosition, FQuat& OutOrientation) const override;

	virtual TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> GetViewExtension() override;
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;

	virtual bool IsFullscreenAllowed() override;
	virtual void RecordAnalytics() override;

	virtual bool HasHiddenAreaMesh() const override { return HiddenAreaMeshes[0].IsValid() && HiddenAreaMeshes[1].IsValid(); }
	virtual void DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;

	virtual bool HasVisibleAreaMesh() const override { return VisibleAreaMeshes[0].IsValid() && VisibleAreaMeshes[1].IsValid(); }
	virtual void DrawVisibleAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;

	FHMDViewMesh HiddenAreaMeshes[2];
	FHMDViewMesh VisibleAreaMeshes[2];

	/** IStereoRendering interface */
	virtual void RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture) const override;
	virtual void CalculateStereoViewOffset(const EStereoscopicPass StereoPassType, const FRotator& ViewRotation, 
										   const float MetersToWorld, FVector& ViewLocation) override;
	virtual FMatrix GetStereoProjectionMatrix(const EStereoscopicPass StereoPassType, const float FOV) const override;
	virtual void GetOrthoProjection(int32 RTWidth, int32 RTHeight, float OrthoDistance, FMatrix OrthoProjection[2]) const override;
	virtual void InitCanvasFromView(FSceneView* InView, UCanvas* Canvas) override;

	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& Viewport, SViewport*) override;

	virtual void CalculateRenderTargetSize(const FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;
	virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) override;

	virtual bool ShouldUseSeparateRenderTarget() const override
	{
		check(IsInGameThread());
		return IsStereoEnabled();
	}

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
    virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual FRHICustomPresent* GetCustomPresent() override { return pCustomPresent; }
	virtual uint32 GetNumberOfBufferedFrames() const override { return 1; }
	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 InFlags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override;

	/** Positional tracking control methods */
	virtual bool IsHeadTrackingAllowed() const override;

	virtual bool IsInLowPersistenceMode() const override;
	virtual void EnableLowPersistenceMode(bool Enable = true) override;

	/** Resets orientation by setting roll and pitch to 0, 
	    assuming that current yaw is forward direction and assuming
		current position as 0 point. */
	virtual void ResetOrientationAndPosition(float yaw = 0.f) override;
	virtual void ResetOrientation(float Yaw = 0.f) override;
	virtual void ResetPosition() override;

	virtual bool HandleInputKey(class UPlayerInput*, const FKey& Key, EInputEvent EventType, float AmountDepressed, bool bGamepad) override;

	virtual void OnBeginPlay() override;
	virtual void OnEndPlay() override;

	virtual void DrawDebug(UCanvas* Canvas) override;

	/**
	* Reports raw sensor data. If HMD doesn't support any of the parameters then it should be set to zero.
	*
	* @param OutData	(out) SensorData structure to be filled in.
	*/
	virtual void GetRawSensorData(SensorData& OutData) override;

	virtual bool GetUserProfile(UserProfile& OutProfile) override;

	virtual FString GetVersionString() const override;

	virtual bool IsHMDActive() override { return Hmd != nullptr; }

protected:
	virtual TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> CreateNewGameFrame() const override;
	virtual TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> CreateNewSettings() const override;
	
	virtual bool DoEnableStereo(bool bStereo, bool bApplyToHmd) override;
	virtual void ResetStereoRenderingParams() override;

	virtual void GetCurrentPose(FQuat& CurrentHmdOrientation, FVector& CurrentHmdPosition, bool bUseOrienationForPlayerCamera = false, bool bUsePositionForPlayerCamera = false) override;

public:

#if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)
	class D3D11Bridge : public FCustomPresent
	{
	public:
		D3D11Bridge(ovrHmd Hmd);

		// Implementation of FCustomPresent, called by Plugin itself
		virtual void BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT) override;
		virtual void FinishRendering() override;
		virtual void Reset() override;
		virtual void Shutdown() override
		{
			Reset();
		}

		virtual void SetHmd(ovrHmd InHmd) override;

		virtual bool AllocateRenderTargetTexture(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 InFlags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples);
	protected:
		void Init(ovrHmd InHmd);
		void Reset_RenderThread();
	protected: // data
		TRefCountPtr<class FD3D11Texture2DSet>	ColorTextureSet;
		TRefCountPtr<class FD3D11Texture2DSet>	DepthTextureSet;
	};

#endif

#ifdef OVR_GL
	class OGLBridge : public FCustomPresent
	{
	public:
		OGLBridge(ovrHmd Hmd);

		// Implementation of FCustomPresent, called by Plugin itself
		virtual void BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT) override;
		virtual void FinishRendering() override;
		virtual void Reset() override;
		virtual void Shutdown() override
		{
			Reset();
		}

		virtual void SetHmd(ovrHmd InHmd) override;

		virtual bool AllocateRenderTargetTexture(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 InFlags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples);

	protected:
		void Init(ovrHmd InHmd);
		void Reset_RenderThread();
	protected: // data
		TRefCountPtr<class FOpenGLTexture2DSet>	ColorTextureSet;
		TRefCountPtr<class FOpenGLTexture2DSet>	DepthTextureSet;
	};
#endif // OVR_GL

	void ShutdownRendering();

	/** Constructor */
	FOculusRiftHMD();

	/** Destructor */
	virtual ~FOculusRiftHMD();

private:
	FOculusRiftHMD* getThis() { return this; }

	/**
	 * Starts up the Oculus Rift device
	 */
	void Startup();

	/**
	 * Shuts down Oculus Rift
	 */
	void Shutdown();

	bool InitDevice();

	void ReleaseDevice();

	void SetupOcclusionMeshes();

	/**
	 * Reads the device configuration, and sets up the stereoscopic rendering parameters
	 */
	virtual void UpdateStereoRenderingParams() override;
	virtual void UpdateHmdRenderInfo() override;
	virtual void UpdateHmdCaps() override;
	virtual void ApplySystemOverridesOnStereo(bool force = false) override;

	/**
	 * Called when state changes from 'stereo' to 'non-stereo'. Suppose to distribute
	 * the event further to user's code (?).
	 * Returns true if state change confirmed, false otherwise.
	 */
	bool OnOculusStateChange(bool bIsEnabledNow);

	/** Load/save settings */
	void LoadFromIni();
	void SaveToIni();

	class FSceneViewport* FindSceneViewport();

	FGameFrame* GetFrame();
	const FGameFrame* GetFrame() const;

	// Copies one texture to another
	void CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef DstTexture, FTexture2DRHIParamRef SrcTexture, FIntRect DstRect = FIntRect(), FIntRect SrcRect = FIntRect()) const;

private: // data

	TRefCountPtr<FCustomPresent>pCustomPresent;
	IRendererModule*			RendererModule;
	ovrHmd						Hmd;
	ovrHmdDesc					HmdDesc;
	ovrGraphicsLuid				Luid;

	TWeakPtr<SWindow>			CachedWindow;
	TWeakPtr<SWidget>			CachedViewportWidget;

	union
	{
		struct
		{
			uint64	_PlaceHolder_ : 1;
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

#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS
