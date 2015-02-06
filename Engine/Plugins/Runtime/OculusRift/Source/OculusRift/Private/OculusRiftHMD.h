// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IOculusRiftPlugin.h"
#include "HeadMountedDisplay.h"
#include "IHeadMountedDisplay.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#include "SceneViewExtension.h"

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif

#if PLATFORM_WINDOWS
	#define OVR_VISION_ENABLED
	#define OVR_SDK_RENDERING
	#define OVR_D3D_VERSION 11
	#define OVR_GL
#elif PLATFORM_MAC
	#define OVR_VISION_ENABLED
    #define OVR_SDK_RENDERING
    #define OVR_GL
#endif

#ifdef OVR_VISION_ENABLED
	#ifndef OVR_CAPI_VISIONSUPPORT
		#define OVR_CAPI_VISIONSUPPORT
	#endif
#endif

#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	
	#include "OVR.h"
	#include "OVR_Version.h"
	#include "OVR_Kernel.h"

	#include "../Src/Kernel/OVR_Threads.h"
	#include "../Src/OVR_CAPI_Keys.h"

#ifdef OVR_SDK_RENDERING
    #if PLATFORM_WINDOWS
        #include "AllowWindowsPlatformTypes.h"
    #endif
	#ifdef OVR_D3D_VERSION
		#include "../Src/OVR_CAPI_D3D.h"
	#endif // OVR_D3D_VERSION
	#ifdef OVR_GL
		#include "../Src/OVR_CAPI_GL.h"
	#endif
#endif // OVR_SDK_RENDERING

	using namespace OVR;

#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif


/**
 * Oculus Rift Head Mounted Display
 */
class FOculusRiftHMD : public IHeadMountedDisplay, public ISceneViewExtension
{
public:
	static void PreInit();

	/** IHeadMountedDisplay interface */
	virtual bool IsHMDConnected() override;
	virtual bool IsHMDEnabled() const override;
	virtual void EnableHMD(bool allow = true) override;
	virtual EHMDDeviceType::Type GetHMDDeviceType() const override;
	virtual bool GetHMDMonitorInfo(MonitorInfo&) override;

	virtual bool DoesSupportPositionalTracking() const override;
	virtual bool HasValidTrackingPosition() const override;
	virtual void GetPositionalTrackingCameraProperties(FVector& OutOrigin, FRotator& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const override;

	virtual void SetInterpupillaryDistance(float NewInterpupillaryDistance) override;
	virtual float GetInterpupillaryDistance() const override;
	virtual void GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const override;

	virtual void GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition) override;
	virtual void ApplyHmdRotation(APlayerController* PC, FRotator& ViewRotation) override;
	virtual void UpdatePlayerCameraRotation(APlayerCameraManager*, struct FMinimalViewInfo& POV) override;

	virtual bool IsChromaAbCorrectionEnabled() const override;

	virtual class ISceneViewExtension* GetViewExtension() override;
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;
	virtual void OnScreenModeChange(EWindowMode::Type WindowMode) override;

	virtual bool IsFullscreenAllowed() override;
	virtual void RecordAnalytics() override;

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
	virtual void GetTimewarpMatrices_RenderThread(EStereoscopicPass StereoPass, FMatrix& EyeRotationStart, FMatrix& EyeRotationEnd) const override;

	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& Viewport, SViewport*) override;

#ifdef OVR_SDK_RENDERING
	virtual void CalculateRenderTargetSize(uint32& InOutSizeX, uint32& InOutSizeY) const override;
	virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) const override;
#endif//OVR_SDK_RENDERING

	virtual bool ShouldUseSeparateRenderTarget() const override
	{
#ifdef OVR_SDK_RENDERING
		check(IsInGameThread());
		return IsStereoEnabled();
#else
		return false;
#endif//OVR_SDK_RENDERING
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
	virtual void ResetOrientation(float Yaw = 0.f) override;
	virtual void ResetPosition() override;

	virtual void SetClippingPlanes(float NCP, float FCP) override;

	virtual void SetBaseRotation(const FRotator& BaseRot) override;
	virtual FRotator GetBaseRotation() const override;

	virtual void SetBaseOrientation(const FQuat& BaseOrient) override;
	virtual FQuat GetBaseOrientation() const override;

	virtual void SetPositionOffset(const FVector& PosOff) override;
	virtual FVector GetPositionOffset() const override;

	virtual void DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FSceneView& View, const FIntPoint& TextureSize) override;
	virtual void UpdateScreenSettings(const FViewport*) override;

	virtual bool HandleInputKey(class UPlayerInput*, const FKey& Key, EInputEvent EventType, float AmountDepressed, bool bGamepad) override;

	virtual void OnBeginPlay() override;
	virtual void OnEndPlay() override;

	virtual void DrawDebug(UCanvas* Canvas, EStereoscopicPass StereoPass) override;

	bool DoEnableStereo(bool bStereo, bool bApplyToHmd);
	void GetCurrentPose(FQuat& CurrentHmdOrientation, FVector& CurrentHmdPosition);
	void BeginRendering_RenderThread();

#ifdef OVR_SDK_RENDERING
	class BridgeBaseImpl : public FRHICustomPresent
	{
	public:
		BridgeBaseImpl(FOculusRiftHMD* plugin) :
			FRHICustomPresent(nullptr),
			Plugin(plugin), 
			bNeedReinitRendererAPI(true), 
			bInitialized(false) 
		{}

		// Returns true if it is initialized and used.
		bool IsInitialized() const { return bInitialized; }

		virtual void BeginRendering() = 0;
		virtual void UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI) = 0;
		virtual void SetNeedReinitRendererAPI() { bNeedReinitRendererAPI = true; }

		virtual void Reset() = 0;
		virtual void Shutdown() = 0;

	protected: // data
		// Data
		mutable OVR::Lock	ModifyLock;
		mutable OVR::Lock	ModifyEyeTexturesLock;
		FOculusRiftHMD*		Plugin;
		bool				bNeedReinitRendererAPI;
		bool				bInitialized;
	};

#if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)
	class D3D11Bridge : public BridgeBaseImpl
	{
	public:
		D3D11Bridge(FOculusRiftHMD* plugin);

		// Implementation of FRHICustomPresent
		// Resets Viewport-specific pointers (BackBufferRT, SwapChain).
		virtual void OnBackBufferResize() override;
		// Returns true if Engine should perform its own Present.
		virtual bool Present(int SyncInterval) override;

		// Implementation of BridgeBaseImpl, called by Plugin itself
		virtual void BeginRendering() override;
		void FinishRendering();
		virtual void UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI) override;
		virtual void Reset() override;
		virtual void Shutdown() override
		{
			Reset();
		}

	protected:
		void Reset_RenderThread();
		void UpdateEyeTextures();
	protected: // data
		ovrD3D11Config		Cfg;
		ovrD3D11Texture		EyeTexture[2];				
		ovrD3D11Texture		EyeTexture_RenderThread[2];
		bool				bNeedReinitEyeTextures;
	};

#endif

#ifdef OVR_GL
	class OGLBridge : public BridgeBaseImpl
	{
	public:
		OGLBridge(FOculusRiftHMD* plugin);

		// Implementation of FRHICustomPresent
		// Resets Viewport-specific resources.
		virtual void OnBackBufferResize() override;
		// Returns true if Engine should perform its own Present.
		virtual bool Present(int SyncInterval) override;

		// Implementation of BridgeBaseImpl, called by Plugin itself
		virtual void BeginRendering() override;
		void FinishRendering();
		virtual void UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI) override;
		virtual void Reset() override;
		virtual void Shutdown() override
		{
			Reset();
		}

		virtual void Init();

	protected: // data
		ovrGLConfig			Cfg;
		ovrGLTexture		EyeTexture[2];
		ovrGLTexture		EyeTexture_RenderThread[2];
		bool				bNeedReinitEyeTextures;
	};
#endif // OVR_GL
	BridgeBaseImpl* GetActiveRHIBridgeImpl();

	void ShutdownRendering();

#else

	virtual void FinishRenderingFrame_RenderThread(FRHICommandListImmediate& RHICmdList) override;

#endif // #ifdef OVR_SDK_RENDERING

	/** Constructor */
	FOculusRiftHMD();

	/** Destructor */
	virtual ~FOculusRiftHMD();

	/** @return	True if the HMD was initialized OK */
	bool IsInitialized() const;

	FString GetVersionString() const;

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

	/**
	 * Reads the device configuration, and sets up the stereoscopic rendering parameters
	 */
	void UpdateStereoRenderingParams();
	void UpdateHmdRenderInfo();

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

	/**
	 * Called when state changes from 'stereo' to 'non-stereo'. Suppose to distribute
	 * the event further to user's code (?).
	 * Returns true if state change confirmed, false otherwise.
	 */
	bool OnOculusStateChange(bool bIsEnabledNow);

	/** Load/save settings */
	void LoadFromIni();
	void SaveToIni();

	/** Applies overrides on system when switched to stereo (such as VSync and ScreenPercentage) */
	void ApplySystemOverridesOnStereo(bool force = false);
	/** Saves system values before applying overrides. */
	void SaveSystemValues();
	/** Restores system values after overrides applied. */
	void RestoreSystemValues();

	void ResetControlRotation() const;

	void PrecalculatePostProcess_NoLock();

	void UpdateDistortionCaps();
	void UpdateHmdCaps();

	void PoseToOrientationAndPosition(const ovrPosef& InPose, FQuat& OutOrientation, FVector& OutPosition) const;

	class FSceneViewport* FindSceneViewport();

#if !UE_BUILD_SHIPPING
	void DrawDebugTrackingCameraFrustum(class UWorld* InWorld, const FVector& ViewLocation);
#endif // #if !UE_BUILD_SHIPPING

private: // data
	friend class FOculusMessageHandler;

	/** Whether or not the Oculus was successfully initialized */
	enum EInitStatus
	{
		eNotInitialized   = 0x00,
		eStartupExecuted  = 0x01,
		eInitialized      = 0x02,
	};

	typedef uint64 bool64;
	union
	{
		struct
		{
			uint64 InitStatus : 2; // see bitmask EInitStatus

			/** Whether stereo is currently on or off. */
			bool64 bStereoEnabled : 1;

			/** Whether or not switching to stereo is allowed */
			bool64 bHMDEnabled : 1;

			/** Indicates if it is necessary to update stereo rendering params */
			bool64 bNeedUpdateStereoRenderingParams : 1;

			/** Debugging:  Whether or not the stereo rendering settings have been manually overridden by an exec command.  They will no longer be auto-calculated */
			bool64 bOverrideStereo : 1;

			/** Debugging:  Whether or not the IPD setting have been manually overridden by an exec command. */
			bool64 bOverrideIPD : 1;

			/** Debugging:  Whether or not the distortion settings have been manually overridden by an exec command.  They will no longer be auto-calculated */
			bool64 bOverrideDistortion : 1;

			/** Debugging: Allows changing internal params, such as screen size, eye-to-screen distance, etc */
			bool64 bDevSettingsEnabled : 1;

			bool64 bOverrideFOV : 1;

			/** Whether or not to override game VSync setting when switching to stereo */
			bool64 bOverrideVSync : 1;

			/** Overridden VSync value */
			bool64 bVSync : 1;

			/** Saved original values for VSync and ScreenPercentage. */
			bool64 bSavedVSync : 1;

			/** Whether or not to override game ScreenPercentage setting when switching to stereo */
			bool64 bOverrideScreenPercentage : 1;

			/** Allows renderer to finish current frame. Setting this to 'true' may reduce the total 
			 *  framerate (if it was above vsync) but will reduce latency. */
			bool64 bAllowFinishCurrentFrame : 1;

			/** Whether world-to-meters scale is overriden or not. */
			bool64 bWorldToMetersOverride : 1;

			/** Distortion on/off */
			bool64 bHmdDistortion : 1;

			/** Chromatic aberration correction on/off */
			bool64 bChromaAbCorrectionEnabled : 1;

			/** Yaw drift correction on/off */
			bool64 bYawDriftCorrectionEnabled : 1;

			/** Whether or not 2D stereo settings overridden. */
			bool64 bOverride2D : 1;

			/** Low persistence mode */
			bool64 bLowPersistenceMode : 1;

			/** Turns on/off updating view's orientation/position on a RenderThread. When it is on,
				latency should be significantly lower. 
				See 'HMD UPDATEONRT ON|OFF' console command.
			*/
			bool64 bUpdateOnRT : 1;

			/** Overdrive brightness transitions to reduce artifacts on DK2+ displays */
			bool64 bOverdrive : 1;

			/** High-quality sampling of distortion buffer for anti-aliasing */
			bool64 bHQDistortion : 1;

			/** Enforces headtracking to work even in non-stereo mode (for debugging or screenshots). 
				See 'MOTION ENFORCE' console command. */
			bool64 bHeadTrackingEnforced : 1;

			/** Is mirroring enabled or not (see 'HMD MIRROR' console cmd) */
			bool64 bMirrorToWindow : 1;

			/** Whether timewarp is enabled or not */
			bool64 bTimeWarp : 1;

			/** Whether ScreenPercentage usage is enabled */
			bool64 bScreenPercentageEnabled : 1;

			/** True, if pos tracking is enabled */
			bool64				bHmdPosTracking : 1;

			/** True, if vision is acquired at the moment */
			bool64				bHaveVisionTracking : 1;

			bool64				bClippingPlanesOverride : 1;
#if !UE_BUILD_SHIPPING
			/** Draw tracking camera frustum, for debugging purposes. 
			 *  See 'HMDPOS SHOWCAMERA ON|OFF' console command.
			 */
			bool64				bDrawTrackingCameraFrustum : 1;

			/** Turns off updating of orientation/position on game thread. See 'hmd updateongt' cmd */
			bool64				bDoNotUpdateOnGT : 1;

			/** Show status / statistics on screen. See 'hmd stats' cmd */
			bool64				bShowStats : 1;

			/** Draw lens centered grid */
			bool64				bDrawGrid : 1;

			/** Profiling mode, removed extra waits in Present (Direct Rendering). See 'hmd profile' cmd */
			bool64				bProfiling : 1;
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

	/** HUD stereo offset */
	float HudOffset;

	/** Screen center adjustment for 2d elements */
	float CanvasCenterOffset;

	OVR::Lock	UpdateOnRTLock;

	/** Size of mirror window; {0,0} if size is the default one */
	FIntPoint	MirrorWindowSize;

	/** Optional far clipping plane for projection matrix */
	float					NearClippingPlane;

	/** Optional far clipping plane for projection matrix */
	float					FarClippingPlane;

	FRotator				DeltaControlRotation;    // same as DeltaControlOrientation but as rotator
	FQuat					DeltaControlOrientation; // same as DeltaControlRotation but as quat

	FQuat					LastHmdOrientation; // contains last APPLIED ON GT HMD orientation
	uint32					LastFrameNumber; // the copy of GFrameNumber when last Hmd orientation was read.

	/** HMD base values, specify forward orientation and zero pos offset */
	OVR::Vector3f			BaseOffset;      // base position, in Oculus coords
	FQuat					BaseOrientation; // base orientation
	FVector					PositionOffset;

	ovrHmd					Hmd;
	ovrEyeRenderDesc		EyeRenderDesc[2];			// 0 - left, 1 - right, same as Views
	ovrMatrix4f				EyeProjectionMatrices[2];	// 0 - left, 1 - right, same as Views
	ovrFovPort				EyeFov[2];					// 0 - left, 1 - right, same as Views
	ovrPosef				EyeRenderPose[2];
	// U,V scale and offset needed for timewarp.
	ovrRecti				EyeRenderViewport[2];		// 0 - left, 1 - right, same as Views
	ovrSizei				TextureSize; // texture size (for both eyes)

	unsigned				TrackingCaps;
	unsigned				DistortionCaps;
	unsigned				HmdCaps;

	unsigned				SupportedTrackingCaps;
	unsigned				SupportedDistortionCaps;
	unsigned				SupportedHmdCaps;

	FIntPoint				EyeViewportSize; // size of the viewport (for one eye). At the moment it is a half of RT.

#ifndef OVR_SDK_RENDERING
	struct FDistortionMesh : public OVR::RefCountBase<FDistortionMesh>
	{
		struct FDistortionVertex*	pVertices;
		uint16*						pIndices;
		unsigned					NumVertices;
		unsigned					NumIndices;
		unsigned					NumTriangles;

		FDistortionMesh() :pVertices(NULL), pIndices(NULL), NumVertices(0), NumIndices(0), NumTriangles(0) {}
		~FDistortionMesh() { Clear(); }
		void Clear();
	};
	ovrVector2f				UVScaleOffset[2][2];	// 0 - left, 1 - right, same as Views
	Ptr<FDistortionMesh>	pDistortionMesh[2];		// 0 - left, 1 - right, same as Views
#else // DIRECT_RENDERING

	uint32						AlternateFrameRateDivider;

#if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)
	TRefCountPtr<D3D11Bridge>	pD3D11Bridge;
#endif
#if defined(OVR_GL)
	TRefCountPtr<OGLBridge>		pOGLBridge;
#endif

#endif // OVR_SDK_RENDERING
	
	OVR::Lock					StereoParamsLock;

	// Params accessible from rendering thread. Should be filled at the beginning
	// of the rendering thread under the StereoParamsLock.
	struct FRenderParams
	{
		//FVector					LastHmdPosition;	// contains last APPLIED ON GT HMD position 
		FVector					LastEyePosition[2];	// contains last APPLIED ON GT HMD position 
		FQuat					DeltaControlOrientation;
		ovrPosef				EyeRenderPose[2];

#ifndef OVR_SDK_RENDERING
		Ptr<FDistortionMesh>	pDistortionMesh[2]; // 0 - left, 1 - right, same as Views
		ovrVector2f				UVScale[2];			// 0 - left, 1 - right, same as Views
		ovrVector2f				UVOffset[2];		// 0 - left, 1 - right, same as Views
#else
		ovrFovPort				EyeFov[2];			// 0 - left, 1 - right, same as Views
#endif // OVR_SDK_RENDERING
		ovrEyeRenderDesc		EyeRenderDesc[2];	// 0 - left, 1 - right, same as Views
		uint32					FrameNumber;
		bool					bFrameBegun : 1;
		bool					bTimeWarp   : 1;
		FEngineShowFlags		ShowFlags; // a copy of showflags

		FRenderParams(FOculusRiftHMD* plugin);

		void Clear() 
		{ 
			#ifndef OVR_SDK_RENDERING
			pDistortionMesh[0] = pDistortionMesh[1] = NULL; 
			#endif
		}
	} RenderParams_RenderThread;

	void*						OSWindowHandle;
};

DEFINE_LOG_CATEGORY_STATIC(LogHMD, Log, All);

#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS
