// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HeadMountedDisplay.h"
#include "IHeadMountedDisplay.h"
#include "HeadMountedDisplayCommon.h"

#if GEARVR_SUPPORTED_PLATFORMS

#include "SceneViewExtension.h"

#include "GearVRSplash.h"

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif
	
PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
#include "OVR_Math.h"
#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_LocalPrefs.h"
#include "SystemActivities.h"
PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS

#include <GLES2/gl2.h>
#include "OpenGLDrvPrivate.h"
#include "OpenGLResources.h"

using namespace OVR;

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

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

	ovrHeadModelParms HeadModelParms;

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

	ovrFrameParms			FrameParms;

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
	class FCustomPresent* pPresentBridge;

	FEngineShowFlags	ShowFlags;			// a copy of showflags
	bool				bFrameBegun : 1;
};

class FOvrMobileSynced
{
	friend class FCustomPresent;
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

class FOpenGLTexture2DSet : public FOpenGLTexture2D
{
public:
	FOpenGLTexture2DSet(
	class FOpenGLDynamicRHI* InGLRHI,
		GLuint InResource,
		GLenum InTarget,
		GLenum InAttachment,
		uint32 InSizeX,
		uint32 InSizeY,
		uint32 InSizeZ,
		uint32 InNumMips,
		uint32 InNumSamples,
		uint32 InArraySize,
		EPixelFormat InFormat,
		bool bInCubemap,
		bool bInAllocatedStorage,
		uint32 InFlags,
		uint8* InTextureRange
		)
		: FOpenGLTexture2D(
			InGLRHI,
			InResource,
			InTarget,
			InAttachment,
			InSizeX,
			InSizeY,
			InSizeZ,
			InNumMips,
			InNumSamples,
			InArraySize,
			InFormat,
			bInCubemap,
			bInAllocatedStorage,
			InFlags,
			InTextureRange,
			FClearValueBinding::Black
			)
	{
		ColorTextureSet = nullptr;
		CurrentIndex = TextureCount = 0;
	}

	~FOpenGLTexture2DSet() { }

	void ReleaseResources() 
	{ 	
		UE_LOG(LogHMD, Log, TEXT("Freeing textureSet %p"), ColorTextureSet);
		vrapi_DestroyTextureSwapChain(ColorTextureSet);
		Resource = 0;
	}

	void SwitchToNextElement();

	static FOpenGLTexture2DSet* CreateTexture2DSet(
		FOpenGLDynamicRHI* InGLRHI,
		uint32 SizeX, uint32 SizeY,
		uint32 InNumSamples,
		uint32 InNumMips,
		EPixelFormat InFormat,
		uint32 InFlags,
		bool bBuffered);

	ovrTextureSwapChain	*	GetColorTextureSet() const { return ColorTextureSet; }
	uint32					GetCurrentIndex() const { return CurrentIndex; }
	uint32					GetTextureCount() const { return TextureCount; }
protected:
	void InitWithCurrentElement();

	uint32					CurrentIndex;
	uint32					TextureCount;
	ovrTextureSwapChain*	ColorTextureSet;
};

typedef TRefCountPtr<FOpenGLTexture2DSet>	FOpenGLTexture2DSetRef;

class FTexture2DSetProxy : public FTextureSetProxy
{
public:
	FTexture2DSetProxy(FOpenGLTexture2DSetRef InTextureSet, uint32 InSrcSizeX, uint32 InSrcSizeY, EPixelFormat InSrcFormat, uint32 InSrcNumMips)
		: FTextureSetProxy(InSrcSizeX, InSrcSizeY, InSrcFormat, InSrcNumMips) , TextureSet(InTextureSet) { }

	~FTexture2DSetProxy() { }

	FOpenGLTexture2DSetRef GetTextureSet() { return TextureSet; }
	
	virtual FTextureRHIRef GetRHITexture() const override 
	{ 
		FTextureRHIParamRef inTexture = TextureSet->GetTexture2D();
		return inTexture; 
	}

	virtual FTexture2DRHIRef GetRHITexture2D() const override 
	{ 
		return TextureSet.GetReference();
	}

	virtual void ReleaseResources() override 
	{
		if(TextureSet.IsValid()) 
		{
			TextureSet->ReleaseResources(); 
		}
		TextureSet = nullptr;
	}

	virtual void SwitchToNextElement() override { TextureSet->SwitchToNextElement(); }
	virtual bool Commit(FRHICommandListImmediate& RHICmdList) override
	{
		FTextureRHIRef RHITexture = GetRHITexture();
		if (RHITexture.IsValid() && RHITexture->GetNumMips() > 1)
		{
			RHICmdList.GenerateMips(RHITexture);
		}
		return true;
	}
protected:
	FOpenGLTexture2DSetRef  TextureSet;
};

typedef TSharedPtr<FTexture2DSetProxy, ESPMode::ThreadSafe>	FTexture2DSetProxyPtr;

class FRenderLayer : public FHMDRenderLayer
{
	friend class FLayerManager;
public:
	FRenderLayer(FHMDLayerDesc&);
	~FRenderLayer();

	ovrFrameLayer&	GetLayer() { return Layer; }
	const ovrFrameLayer&	GetLayer() const { return Layer; }
	ovrTextureSwapChain* GetSwapTextureSet() const
	{
		if (TextureSet.IsValid())
		{
			return static_cast<FTexture2DSetProxy*>(TextureSet.Get())->GetTextureSet()->GetColorTextureSet();
		}
		return nullptr; 
	}

	uint32 GetSwapTextureIndex() const
	{
		if (TextureSet.IsValid())
		{
			return static_cast<FTexture2DSetProxy*>(TextureSet.Get())->GetTextureSet()->GetCurrentIndex();
		}
		return 1;
	}

	virtual TSharedPtr<FHMDRenderLayer> Clone() const override;

protected:
	ovrFrameLayer Layer;
};

class FLayerManager : public FHMDLayerManager 
{
public:
	FLayerManager(class FCustomPresent*);
	~FLayerManager();

	virtual void Startup() override;
	virtual void Shutdown() override;

	// Releases all textureSets used by all layers
	virtual void ReleaseTextureSets_RenderThread_NoLock() override;

	virtual void PreSubmitUpdate_RenderThread(FRHICommandListImmediate& RHICmdList, const FHMDGameFrame* CurrentFrame, bool ShowFlagsRendering) override;

	void ReleaseTextureSets(); // can be called on any thread

	void SubmitFrame_RenderThread(ovrMobile* mobilePtr, ovrFrameParms* currentParams);

	bool HasEyeLayer() const { FScopeLock ScopeLock(&LayersLock); return EyeLayers.Num() != 0; }

	const FHMDLayerDesc* GetEyeLayerDesc() const { return GetLayerDesc(EyeLayerId); }

protected:
	virtual TSharedPtr<FHMDRenderLayer> CreateRenderLayer_RenderThread(FHMDLayerDesc& InDesc) override;

	virtual uint32 GetTotalNumberOfLayersSupported() const override { return VRAPI_FRAME_LAYER_TYPE_MAX; }

private:
	FCustomPresent *pPresentBridge;
	uint32			EyeLayerId;
	bool			bInitialized : 1;
};

class FCustomPresent : public FRHICustomPresent
{
	friend class FViewExtension;
	friend class ::FGearVR;
public:
	FCustomPresent(jobject InActivityObject, int32 InMinimumVsyncs);

	// Returns true if it is initialized and used.
	bool IsInitialized() const { return bInitialized; }

	void UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI, FGameFrame* InRenderFrame);
	FGameFrame* GetRenderFrame() const { check(IsInRenderingThread()); return static_cast<FGameFrame*>(RenderContext->RenderFrame.Get()); }
	FViewExtension* GetRenderContext() const { return static_cast<FViewExtension*>(RenderContext.Get()); }
	FSettings* GetFrameSetting() const { check(IsInRenderingThread()); return static_cast<FSettings*>(RenderContext->RenderFrame->GetSettings()); }

	void BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT);
	void FinishRendering();
	void UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI);

	void UpdateLayers(FRHICommandListImmediate& RHICmdList);

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

	FTexture2DSetProxyPtr CreateTextureSet(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, bool bBuffered);

	void CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef DstTexture, FTextureRHIParamRef SrcTexture, int SrcSizeX, int SrcSizeY, FIntRect DstRect = FIntRect(), FIntRect SrcRect = FIntRect(), bool bAlphaPremultiply = false) const;
		
	FOvrMobileSynced GetMobileSynced() { return FOvrMobileSynced(OvrMobile, &OvrMobileLock); }
	void EnterVRMode_RenderThread();
	void LeaveVRMode_RenderThread();

	FLayerManager* GetLayerMgr() { return static_cast<FLayerManager*>(LayerMgr.Get()); }

	pid_t GetRenderThreadId() const { return RenderThreadId; }

	// Allocates a texture set and copies the texture into it.
	// To turn it off, call with 'nullptr' param.
	void SetLoadingIconTexture_RenderThread(FTextureRHIRef Texture);

	// Sets/clears "loading icon mode"
	void SetLoadingIconMode(bool bLoadingIconActive);
	bool IsInLoadingIconMode() const;

	// Forcedly renders the loading icon.
	void RenderLoadingIcon_RenderThread(uint32 FrameIndex);
	
	int32 LockSubmitFrame();
	int32 UnlockSubmitFrame();
	bool IsSubmitFrameLocked() const { return SubmitFrameLocker.GetValue() != 0; }

	void PushFrame(FLayerManager* pInLayerMgr, const FGameFrame* CurrentFrame);
protected:
	void SetRenderContext(FHMDViewExtension* InRenderContext);
	void DoRenderLoadingIcon_RenderThread(int CpuLevel, int GpuLevel, pid_t GameTid);
	void SystemActivities_Update_RenderThread();
	void PushBlackFinal(const FGameFrame* frame);

protected: // data
	TSharedPtr<FViewExtension, ESPMode::ThreadSafe> RenderContext;
	bool											bInitialized;

	IRendererModule*	RendererModule;

	// should be accessed only on a RenderThread!
	ovrFrameParms							DefaultFrameParms;
 	ovrFrameParms							LoadingIconParms;
	ovrPerformanceParms						DefaultPerfParms;
	FTexture2DSetProxyPtr					LoadingIconTextureSet;
	bool									bLoadingIconIsActive;
	bool									bExtraLatencyMode;
	bool									bHMTWasMounted;
	int32									MinimumVsyncs;

	TSharedPtr<FLayerManager>				LayerMgr;

	ovrMobile*								OvrMobile;		// to be accessed only on RenderThread (or, when RT is suspended)
	pid_t									RenderThreadId; // the rendering thread id where EnterVrMode was called.
	FCriticalSection						OvrMobileLock;	// used to access OvrMobile_RT/HmdInfo_RT on a game thread
	ovrJava									JavaRT;			// Rendering thread Java obj
	jobject									ActivityObject;
	FThreadSafeCounter						SubmitFrameLocker;
};
}  // namespace GearVR

using namespace GearVR;

/**
 * GearVR Head Mounted Display
 */
class FGearVR : public FHeadMountedDisplay
{
	friend class FViewExtension;
	friend class FCustomPresent;
	friend class FGearVRSplash;
public:
	/** IHeadMountedDisplay interface */
	virtual FName GetDeviceName() const override
	{
		static FName DefaultName(TEXT("GearVR"));
		return DefaultName;
	}

	virtual bool OnStartGameFrame( FWorldContext& WorldContext ) override;
	virtual bool IsHMDConnected() override;
	virtual EHMDDeviceType::Type GetHMDDeviceType() const override;
	virtual bool GetHMDMonitorInfo(MonitorInfo&) override;

	virtual TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> GetViewExtension() override;
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;

	/** IStereoRendering interface */
	virtual void RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture) const override;

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
	virtual void ResetOrientation(float yaw = 0.f) override { ResetOrientationAndPosition(yaw); }

	void RebaseObjectOrientationAndPosition(FVector& OutPosition, FQuat& OutOrientation) const override;

	virtual FString GetVersionString() const override;

	virtual void DrawDebug(UCanvas* Canvas) override;

	/**
	* Reports raw sensor data. If HMD doesn't support any of the parameters then it should be set to zero.
	*
	* @param OutData	(out) SensorData structure to be filled in.
	*/
	virtual void GetRawSensorData(SensorData& OutData) override;

	virtual bool HandleInputKey(class UPlayerInput*, const FKey& Key, EInputEvent EventType, float AmountDepressed, bool bGamepad) override;

	virtual void OnBeginPlay(FWorldContext& InWorldContext) override;

	virtual FHMDLayerManager* GetLayerManager() override { return pGearVRBridge->GetLayerMgr(); }

	/** Constructor */
	FGearVR();

	/** Destructor */
	virtual ~FGearVR();

	TRefCountPtr<FCustomPresent> pGearVRBridge;

	void StartOVRGlobalMenu();
	void StartOVRQuitMenu();

	void SetCPUAndGPULevels(int32 CPULevel, int32 GPULevel);
	bool IsPowerLevelStateMinimum() const;
	bool IsPowerLevelStateThrottled() const;
	bool AreHeadPhonesPluggedIn() const;
	float GetTemperatureInCelsius() const;
	float GetBatteryLevel() const;

	void SetLoadingIconTexture(FTextureRHIRef InTexture);
	void SetLoadingIconMode(bool bActiveLoadingIcon);
	void RenderLoadingIcon_RenderThread();
	bool IsInLoadingIconMode() const;

	FCustomPresent* GetCustomPresent_Internal() const { return pGearVRBridge; }
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

	virtual bool DoEnableStereo(bool bStereo) override;
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
	// Can be called on GameThread to figure out if OvrMobile obj is valid.
	bool HasValidOvrMobile() const;

	void HandleBackButtonAction();
	void StartSystemActivity_RenderThread(const char * commandString);
	void PushBlackFinal();

	virtual FAsyncLoadingSplash* GetAsyncLoadingSplash() const override { return Splash.Get(); }

private: // data

	FRotator			DeltaControlRotation;    // same as DeltaControlOrientation but as rotator

	ovrJava				JavaGT;		// game thread (main) Java VM obj	

	float				ResetToYaw; // used for delayed orientation/position reset

	typedef enum
	{
		BACK_BUTTON_STATE_NONE,
		BACK_BUTTON_STATE_PENDING_DOUBLE_TAP,
		BACK_BUTTON_STATE_PENDING_SHORT_PRESS,
		BACK_BUTTON_STATE_SKIP_UP
	} ovrBackButtonState;
	ovrBackButtonState	BackButtonState;
	bool				BackButtonDown;
	double				BackButtonDownStartTime;

	TSharedPtr<FGearVRSplash> Splash;

	union
	{
		struct
		{
			/** To perform delayed orientation and position reset */
			uint64	NeedResetOrientationAndPosition : 1;

			/** Is the app resumed or not */
			uint64  bResumed : 1;
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