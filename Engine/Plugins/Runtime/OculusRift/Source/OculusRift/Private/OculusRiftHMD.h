// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IHeadMountedDisplay.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#include "HeadMountedDisplayCommon.h"
#include "Stats.h"
#include "SceneViewExtension.h"

#include "OculusRiftCommon.h"
#include "OculusRiftLayers.h"
#include "OculusRiftSplash.h"
#if !UE_BUILD_SHIPPING
#include "SceneCubemapCapturer.h"
#endif

class FOculusRiftHMD;

DECLARE_STATS_GROUP(TEXT("OculusRiftHMD"), STATGROUP_OculusRiftHMD, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("BeginRendering"), STAT_BeginRendering, STATGROUP_OculusRiftHMD, );

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
template <typename OVRQuat>
FORCEINLINE OVRQuat ToOVRQuat(const FQuat& InQuat)
{
	return OVRQuat(float(InQuat.Y), float(InQuat.Z), float(-InQuat.X), float(-InQuat.W));
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
#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS

//-------------------------------------------------------------------------------------------------
// FOculusRiftPlugin
//-------------------------------------------------------------------------------------------------

class FOculusRiftPlugin : public IOculusRiftPlugin
{
public:
	FOculusRiftPlugin();

protected:
	bool Initialize();

#ifdef OVR_D3D
	void DisableSLI();
	void SetGraphicsAdapter(const ovrGraphicsLuid& luid);
#endif

public:
	static inline FOculusRiftPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< FOculusRiftPlugin >( "OculusRift" );
	}

#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	ovrResult CreateSession(ovrSession* session, ovrGraphicsLuid* luid);
	static void DestroySession(ovrSession session);
#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS
public:
	/** IModuleInterface */
	virtual void ShutdownModule() override;

	/** IHeadMountedDisplayModule */
	virtual FString GetModulePriorityKeyName() const override;
	virtual bool PreInit() override;
	virtual bool IsHMDConnected() override;
	virtual int GetGraphicsAdapter() override;
	virtual FString GetAudioInputDevice() override;
	virtual FString GetAudioOutputDevice() override;
	virtual TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > CreateHeadMountedDisplay() override;

	/** IOculusRiftPlugin */
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	virtual bool PoseToOrientationAndPosition(const ovrPosef& Pose, FQuat& OutOrientation, FVector& OutPosition) const override;
	virtual class FOvrSessionShared* GetSession() override;
	virtual bool GetCurrentTrackingState(ovrTrackingState* TrackingState) override;
#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS

protected:
	bool bInitialized;
	bool bInitializeCalled;
	TWeakPtr< IHeadMountedDisplay, ESPMode::ThreadSafe > HeadMountedDisplay;
};


//-------------------------------------------------------------------------------------------------
// FSettings
//-------------------------------------------------------------------------------------------------

#if OCULUS_RIFT_SUPPORTED_PLATFORMS
class FSettings : public FHMDSettings
{
public:
	const int TexturePaddingPerEye = 12; // padding, in pixels, per eye (total padding will be doubled)

	ovrEyeRenderDesc		EyeRenderDesc[2];			// 0 - left, 1 - right, same as Views
	ovrMatrix4f				EyeProjectionMatrices[2];	// 0 - left, 1 - right, same as Views
	ovrMatrix4f				PerspectiveProjection[2];	// used for calc ortho projection matrices
	ovrFovPort				EyeFov[2];					// 0 - left, 1 - right, same as Views

	FIntPoint				RenderTargetSize;
	float					PixelDensity;
	float					PixelDensityMin;
	float					PixelDensityMax;
	bool					PixelDensityAdaptive;

	unsigned				SupportedTrackingCaps;
	unsigned				SupportedHmdCaps;

	unsigned				TrackingCaps;
	unsigned				HmdCaps;

	float					VsyncToNextVsync;

	enum MirrorWindowModeType
	{
		eMirrorWindow_Distorted,
		eMirrorWindow_Undistorted,
		eMirrorWindow_SingleEye,
		eMirrorWindow_SingleEyeLetterboxed,
		eMirrorWindow_SingleEyeCroppedToFill,

		eMirrorWindow_Total_
	};
	MirrorWindowModeType	MirrorWindowMode;

	FSettings();
	virtual ~FSettings() override {}

	virtual TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> Clone() const override;

	float GetTexturePaddingPerEye() const { return TexturePaddingPerEye * GetActualScreenPercentage()/100.f; }
};


//-------------------------------------------------------------------------------------------------
// FGameFrame
//-------------------------------------------------------------------------------------------------

class FGameFrame : public FHMDGameFrame
{
public:
	ovrTrackingState	InitialTrackingState;		// tracking state at start of frame
	ovrPosef			CurHeadPose;				// current head pose
	ovrPosef			CurEyeRenderPose[2];		// current eye render poses
	ovrPosef			HeadPose;					// position of head actually used
	ovrPosef			EyeRenderPose[2];			// eye render pose actually used
	ovrSessionStatus	SessionStatus;

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

	// Returns tracking state for current frame
	ovrTrackingState GetTrackingState(ovrSession InOvrSession) const;
	// Returns HeadPose and EyePoses calculated from TrackingState
	void GetHeadAndEyePoses(const ovrTrackingState& TrackingState, ovrPosef& outHeadPose, ovrPosef outEyePoses[2]) const;
};

//-------------------------------------------------------------------------------------------------
// FViewExtension class that contains all the rendering-specific data (also referred as 'RenderContext').
// Each call to RT will use an unique instance of this class, attached to ViewFamily.
//-------------------------------------------------------------------------------------------------

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
	FOvrSessionSharedPtr Session;

	FEngineShowFlags	ShowFlags; // a copy of showflags
	bool				bFrameBegun : 1;
};

//-------------------------------------------------------------------------------------------------
// FCustomPresent 
//-------------------------------------------------------------------------------------------------

class FCustomPresent : public FRHICustomPresent
{
	friend class FLayerManager;
public:
	FCustomPresent(const FOvrSessionSharedPtr& InOvrSession);

	// Returns true if it is initialized and used.
	bool IsInitialized() const { return Session->IsActive(); }

	virtual bool IsUsingGraphicsAdapter(const ovrGraphicsLuid& luid) = 0;

	virtual void BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT) = 0;
	virtual void FinishRendering();

	// Resets Viewport-specific pointers (BackBufferRT, SwapChain).
	virtual void OnBackBufferResize() override;

	virtual void Reset();
	virtual void Shutdown();

	// Returns true if Engine should perform its own Present.
	virtual bool Present(int32& SyncInterval) override;

	virtual void UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI, FGameFrame* InRenderFrame);
	FGameFrame* GetRenderFrame() const { check(IsInRenderingThread()); return static_cast<FGameFrame*>(RenderContext->RenderFrame.Get()); }
	FViewExtension* GetRenderContext() const { return static_cast<FViewExtension*>(RenderContext.Get()); }
	FSettings* GetFrameSetting() const { check(IsInRenderingThread()); return static_cast<FSettings*>(RenderContext->RenderFrame->GetSettings()); }

	const FOvrSessionSharedPtr& GetSession() const { return Session; }

	// marking textures invalid, that will force re-allocation of ones
	void MarkTexturesInvalid();

	bool AreTexturesMarkedAsInvalid() const { return bNeedReAllocateTextureSet; }

	virtual FTexture2DRHIRef GetMirrorTexture() { return MirrorTextureRHI; }

	enum ELayerFlags
	{
		None = 0,
		HighQuality = 0x1
	};
	// Allocates render target texture
	// If returns false then a default RT texture will be used.
	// TextureFlags - TexCreate_<>
	// LayerFlags   - see ELayerFlag
	bool AllocateRenderTargetTexture(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 TextureFlags, uint32 LayerFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture);

	// Can be called on any thread. Returns true, if HMD is marked as invalid and it must be killed.
	// This function resets the flag.
	bool NeedsToKillHmd()
	{
		// This keeps the flag value
		return FPlatformAtomics::InterlockedAdd(&NeedToKillHmd, 0) != 0;
	}
	bool ResetNeedsToKillHmd()
	{
		// This resets the flag to 0
		return FPlatformAtomics::InterlockedExchange(&NeedToKillHmd, 0) != 0;
	}

	// Returns true, if everything is ready for frame submission.
	bool IsReadyToSubmitFrame() const { return bReady; }
	int32 LockSubmitFrame() { return SubmitFrameLocker.Increment(); }
	int32 UnlockSubmitFrame() { return SubmitFrameLocker.Decrement(); }
	bool IsSubmitFrameLocked() const { return SubmitFrameLocker.GetValue() != 0; }

	enum ECreateTexFlags
	{
		StaticImage			= 0x1,
		RenderTargetable	= 0x2,
		ShaderResource		= 0x4,
		EyeBuffer			= 0x8,
		LinearSpace         = 0x10,

		Default				= RenderTargetable | ShaderResource,
		DefaultLinear       = LinearSpace | Default,
		DefaultStaticImage	= StaticImage | Default,
		DefaultEyeBuffer	= EyeBuffer | Default,
	};
	// Create and destroy textureset from a texture.
	virtual FTexture2DSetProxyPtr CreateTextureSet(const uint32 InSizeX, const uint32 InSizeY, const EPixelFormat InSrcFormat, const uint32 InNumMips = 1, uint32 InCreateTexFlags = ECreateTexFlags::Default) = 0;

	// Copies one texture to another
	void CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef DstTexture, FTexture2DRHIParamRef SrcTexture, int SrcSizeX, int SrcSizeY, FIntRect DstRect = FIntRect(), FIntRect SrcRect = FIntRect(), bool bAlphaPremultiply = false, bool bNoAlphaWrite = false) const;

	FLayerManager* GetLayerMgr() { return static_cast<FLayerManager*>(LayerMgr.Get()); }
	void UpdateLayers(FRHICommandListImmediate& RHICmdList);

	static uint32 GetNumMipLevels(uint32 w, uint32 h, uint32 InCreateTexFlags);

	ovrResult GetLastSubmitFrameResult() const { return LayerMgr->GetLastSubmitFrameResult(); }

protected:
	void SetRenderContext(FHMDViewExtension* InRenderContext);
	void Reset_RenderThread();

protected: // data
	FOvrSessionSharedPtr Session;
	TSharedPtr<FViewExtension, ESPMode::ThreadSafe> RenderContext;
	IRendererModule*	RendererModule;

	TSharedPtr<FLayerManager> LayerMgr;

	// Mirror texture
	ovrMirrorTexture	MirrorTexture;
	FTexture2DRHIRef	MirrorTextureRHI;

	FThreadSafeCounter  SubmitFrameLocker;

	volatile int32		NeedToKillHmd;
	bool				bNeedReAllocateTextureSet : 1;
	bool				bNeedReAllocateMirrorTexture : 1;
	bool				bReady : 1;
};


//-------------------------------------------------------------------------------------------------
// FPerformanceStats
//-------------------------------------------------------------------------------------------------

struct FPerformanceStats
{
	uint64 Frames;
	double Seconds;

	FPerformanceStats(uint32 InFrames = 0, double InSeconds = 0.0)
		: Frames(InFrames)
		, Seconds(InSeconds) 
	{}

	FPerformanceStats operator - (const FPerformanceStats& PerformanceStats) const
	{
		return FPerformanceStats(
			Frames - PerformanceStats.Frames,
			Seconds - PerformanceStats.Seconds);
	}
};

} // namespace OculusRift

using namespace OculusRift;


//-------------------------------------------------------------------------------------------------
// FOculusRiftHMD - Oculus Rift Head Mounted Display
//-------------------------------------------------------------------------------------------------

class FOculusRiftHMD : public FHeadMountedDisplay
{
	friend class FViewExtension;
	friend class UOculusFunctionLibrary;
	friend class FOculusRiftPlugin;
	friend class FLayerManager;
	friend class FOculusRiftSplash;
public:

	/** IHeadMountedDisplay interface */
	virtual FName GetDeviceName() const override
	{
		static FName DefaultName(TEXT("OculusRift"));
		return DefaultName;
	}
	virtual bool OnStartGameFrame( FWorldContext& WorldContext ) override;

	virtual bool IsHMDConnected() override;
	virtual EHMDWornState::Type GetHMDWornState() override;
	virtual EHMDDeviceType::Type GetHMDDeviceType() const override;
	virtual bool GetHMDMonitorInfo(MonitorInfo&) override;

	virtual bool DoesSupportPositionalTracking() const override;
	virtual bool HasValidTrackingPosition() override;
	virtual uint32 GetNumOfTrackingSensors() const override;
	virtual bool GetTrackingSensorProperties(uint8 InSensorIndex, FVector& OutOrigin, FQuat& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const override;
	virtual void RebaseObjectOrientationAndPosition(FVector& OutPosition, FQuat& OutOrientation) const override;

	virtual TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> GetViewExtension() override;
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;

	virtual void RecordAnalytics() override;

	virtual bool HasHiddenAreaMesh() const override;
	virtual void DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;

	virtual bool HasVisibleAreaMesh() const override;
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
	
	// InTexFlags - TexCreate_<>
	// InTargetableTextureFlags - TexCreate_RenderTargetable or 0
	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 InTexFlags, uint32 InTargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override;

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

	virtual void OnBeginPlay(FWorldContext& InWorldContext) override;
	virtual void OnEndPlay(FWorldContext& InWorldContext) override;

	virtual void DrawDebug(UCanvas* Canvas) override;

	/**
	* Reports raw sensor data. If HMD doesn't support any of the parameters then it should be set to zero.
	*
	* @param OutData	(out) SensorData structure to be filled in.
	*/
	virtual void GetRawSensorData(SensorData& OutData) override;

	virtual bool GetUserProfile(UserProfile& OutProfile) override;

	virtual FString GetVersionString() const override;

	virtual bool IsHMDActive() override { return Session->IsActive(); }

	virtual FHMDLayerManager* GetLayerManager() override { return pCustomPresent->GetLayerMgr(); }

	virtual void					 SetTrackingOrigin(EHMDTrackingOrigin::Type) override;
	virtual EHMDTrackingOrigin::Type GetTrackingOrigin() override;

	virtual void UseImplicitHmdPosition( bool bInImplicitHmdPosition ) override;

	virtual void SetPixelDensity(float NewPD) override
	{
		GetSettings()->PixelDensity = FMath::Clamp(NewPD, 0.5f, 2.0f);
		GetSettings()->PixelDensityMin = FMath::Min(GetSettings()->PixelDensity, GetSettings()->PixelDensityMin);
		GetSettings()->PixelDensityMax = FMath::Max(GetSettings()->PixelDensity, GetSettings()->PixelDensityMax);
		Flags.bNeedUpdateStereoRenderingParams = true;
	}
protected:
	virtual TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> CreateNewGameFrame() const override;
	virtual TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> CreateNewSettings() const override;
	
	virtual bool DoEnableStereo(bool bStereo) override;
	virtual void ResetStereoRenderingParams() override;

	virtual void GetCurrentPose(FQuat& CurrentHmdOrientation, FVector& CurrentHmdPosition, bool bUseOrienationForPlayerCamera = false, bool bUsePositionForPlayerCamera = false) override;

	void MakeSureValidFrameExists(AWorldSettings* InWorldSettings);

public:

	float GetVsyncToNextVsync() const;
	FPerformanceStats GetPerformanceStats() const;

#ifdef OVR_D3D
	class D3D11Bridge : public FCustomPresent
	{
	public:
		D3D11Bridge(const FOvrSessionSharedPtr& InOvrSession);

		// Implementation of FCustomPresent, called by Plugin itself
		virtual bool IsUsingGraphicsAdapter(const ovrGraphicsLuid& luid) override;
		virtual void BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT) override;

		// Create and destroy textureset from a texture.
		virtual FTexture2DSetProxyPtr CreateTextureSet(const uint32 InSizeX, const uint32 InSizeY, const EPixelFormat InSrcFormat, const uint32 InNumMips, uint32 InCreateTexFlags) override;
	};

	class D3D12Bridge : public FCustomPresent
	{
	public:
		D3D12Bridge(const FOvrSessionSharedPtr& InOvrSession);

		// Implementation of FCustomPresent, called by Plugin itself
		virtual bool IsUsingGraphicsAdapter(const ovrGraphicsLuid& luid) override;
		virtual void BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT) override;

		// Create and destroy textureset from a texture.
		virtual FTexture2DSetProxyPtr CreateTextureSet(const uint32 InSizeX, const uint32 InSizeY, const EPixelFormat InSrcFormat, const uint32 InNumMips, uint32 InCreateTexFlags) override;
	};
#endif

#ifdef OVR_GL
	class OGLBridge : public FCustomPresent
	{
	public:
		OGLBridge(const FOvrSessionSharedPtr& InOvrSession);

		// Implementation of FCustomPresent, called by Plugin itself
		virtual bool IsUsingGraphicsAdapter(const ovrGraphicsLuid& luid) override;
		virtual void BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT) override;

		// Create and destroy textureset from a texture.
		virtual FTexture2DSetProxyPtr CreateTextureSet(const uint32 InSizeX, const uint32 InSizeY, const EPixelFormat InSrcFormat, const uint32 InNumMips, uint32 InCreateTexFlags) override;
	};
#endif // OVR_GL

	void ShutdownRendering();

	/** Constructor */
	FOculusRiftHMD();

	/** Destructor */
	virtual ~FOculusRiftHMD();

	FCustomPresent* GetCustomPresent_Internal() const { return pCustomPresent; }
	
	#if !UE_BUILD_SHIPPING
	enum ECubemapType
	{
		CM_GearVR = 0,
		CM_Rift = 1
	};
	void CaptureCubemap(UWorld* InWorld, ECubemapType CubemapType = CM_Rift, FVector InOffset = FVector::ZeroVector, float InYaw = 0);
	#endif //#if !UE_BUILD_SHIPPING

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

	virtual FAsyncLoadingSplash* GetAsyncLoadingSplash() const override { return Splash.Get(); }

	void PreShutdown();

private: // data

	TRefCountPtr<FCustomPresent>pCustomPresent;
	TSharedPtr<FLayerManager>	LayerMgr;
	IRendererModule*			RendererModule;
	FOvrSessionSharedPtr		Session;
	ovrHmdDesc					HmdDesc;
	ovrTrackingOrigin			OvrOrigin;

	TWeakPtr<SWindow>			CachedWindow;
	TWeakPtr<SWidget>			CachedViewportWidget;

	ovrResult					LastSubmitFrameResult;

	TSharedPtr<FOculusRiftSplash> Splash;

	FWorldContext*				WorldContext;

	// used to capture cubemaps for Oculus Home
	class USceneCubemapCapturer* CubemapCapturer;

	union
	{
		struct
		{
			uint64 NeedSetTrackingOrigin : 1; // set to true when origin was set while OvrSession == null; the origin will be set ASA OvrSession != null

			uint64 EnforceExit : 1; // enforces exit; used mostly for testing

			uint64 AppIsPaused : 1; // set if a game is paused by the plugin

			uint64 DisplayLostDetected : 1; // set to indicate that DisplayLost was detected by game thread.

			uint64 NeedSetFocusToGameViewport : 1; // set to true once new session is created; being handled and reset as soon as session->IsVisible.
		};
		uint64 Raw;
	} OCFlags;

	FPerformanceStats			PerformanceStats;
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
