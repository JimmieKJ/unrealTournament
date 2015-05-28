// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
// Mac uses 0.5.0 SDK, while PC - 0.6.0. This version supports 0.5.0 (Mac/Linux).
#if PLATFORM_MAC

#include "IOculusRiftPlugin.h"
#include "IHeadMountedDisplay.h"
#include "HeadMountedDisplayCommon.h"

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
	#include <OVR_Version.h>
	#include <OVR_CAPI.h>
	#include <OVR_CAPI_Keys.h>
	#include <Extras/OVR_Math.h>
    #include <OVR_CAPI_Util.h>

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif
#ifdef OVR_SDK_RENDERING
	#ifdef OVR_D3D_VERSION
		#include "OVR_CAPI_D3D.h"
	#endif // OVR_D3D_VERSION
	#ifdef OVR_GL
		#include "OVR_CAPI_GL.h"
	#endif
#endif // OVR_SDK_RENDERING

#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

// #ifdef UE_BUILD_DEBUG
// 	#undef FORCEINLINE
// 	#define FORCEINLINE
// #endif

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

typedef uint64 bool64;

class FSettings : public FHMDSettings
{
public:
	const int TexturePaddingPerEye = 12; // padding, in pixels, per eye (total padding will be doubled)

	ovrEyeRenderDesc		EyeRenderDesc[2];			// 0 - left, 1 - right, same as Views
	ovrMatrix4f				EyeProjectionMatrices[2];	// 0 - left, 1 - right, same as Views
	ovrMatrix4f				PerspectiveProjection[2];	// used for calc ortho projection matrices
	ovrFovPort				EyeFov[2];					// 0 - left, 1 - right, same as Views

	unsigned				SupportedTrackingCaps;
	unsigned				SupportedDistortionCaps;
	unsigned				SupportedHmdCaps;

	unsigned				TrackingCaps;
	unsigned				DistortionCaps;
	unsigned				HmdCaps;

#ifndef OVR_SDK_RENDERING
	struct FDistortionMesh : public TSharedFromThis<FDistortionMesh>
	{
		FDistortionVertex*	pVertices;
		uint16*				pIndices;
		unsigned			NumVertices;
		unsigned			NumIndices;
		unsigned			NumTriangles;

		FDistortionMesh() :pVertices(NULL), pIndices(NULL), NumVertices(0), NumIndices(0), NumTriangles(0) {}
		~FDistortionMesh() { Reset(); }
		void Reset();
	};
	// U,V scale and offset needed for timewarp.
	ovrVector2f				UVScaleOffset[2][2];	// scale UVScaleOffset[x][0], offset UVScaleOffset[x][1], where x is eye idx
	TSharedPtr<FDistortionMesh>	pDistortionMesh[2];		// 
#endif

	FSettings();
	virtual ~FSettings() override {}

	virtual TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> Clone() const override;

	virtual void SetEyeRenderViewport(int OneEyeVPw, int OneEyeVPh) override;
	
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
	FSettings* GetFrameSetting() const { return static_cast<FSettings*>(RenderFrame->GetSettings()); }
public:
	class FCustomPresent* pPresentBridge;
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
		, bNeedReinitRendererAPI(true)
		, bInitialized(false)
	{}

	// Returns true if it is initialized and used.
	bool IsInitialized() const { return bInitialized; }

	virtual void BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT) = 0;
	virtual void SetNeedReinitRendererAPI() { bNeedReinitRendererAPI = true; }

	virtual void Reset() = 0;
	virtual void Shutdown() = 0;

	void UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI, FGameFrame* InRenderFrame);
	FGameFrame* GetRenderFrame() const { check(IsInRenderingThread()); return static_cast<FGameFrame*>(RenderContext->RenderFrame.Get()); }
	FViewExtension* GetRenderContext() const { return static_cast<FViewExtension*>(RenderContext.Get()); }
	FSettings* GetFrameSetting() const { check(IsInRenderingThread()); return static_cast<FSettings*>(RenderContext->RenderFrame->GetSettings()); }

protected:
	void SetRenderContext(FHMDViewExtension* InRenderContext);

protected: // data
	TSharedPtr<FViewExtension, ESPMode::ThreadSafe> RenderContext;
	bool				bNeedReinitRendererAPI : 1;
	bool				bInitialized : 1;
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
public:
	static void PreInit();

	/** IHeadMountedDisplay interface */
	virtual bool OnStartGameFrame() override;

	virtual bool IsHMDConnected() override;
	virtual EHMDDeviceType::Type GetHMDDeviceType() const override;
	virtual bool GetHMDMonitorInfo(MonitorInfo&) override;

	virtual bool DoesSupportPositionalTracking() const override;
	virtual bool HasValidTrackingPosition() override;
	virtual void GetPositionalTrackingCameraProperties(FVector& OutOrigin, FQuat& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const override;

	virtual void GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition) override;

	virtual FVector GetNeckPosition(const FQuat& CurrentOrientation, const FVector& CurrentPosition, const FVector& PositionScale) override;

	virtual TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> GetViewExtension() override;
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;

	virtual bool IsFullscreenAllowed() override;
	virtual void RecordAnalytics() override;

	/** IStereoRendering interface */
	virtual void CalculateStereoViewOffset(const EStereoscopicPass StereoPassType, const FRotator& ViewRotation, 
										   const float MetersToWorld, FVector& ViewLocation) override;
	virtual FMatrix GetStereoProjectionMatrix(const EStereoscopicPass StereoPassType, const float FOV) const override;
	virtual void GetOrthoProjection(int32 RTWidth, int32 RTHeight, float OrthoDistance, FMatrix OrthoProjection[2]) const override;
	virtual void InitCanvasFromView(FSceneView* InView, UCanvas* Canvas) override;
	virtual void GetEyeRenderParams_RenderThread(const struct FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const override;
	virtual void GetTimewarpMatrices_RenderThread(const struct FRenderingCompositePassContext& Context, FMatrix& EyeRotationStart, FMatrix& EyeRotationEnd) const override;

	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& Viewport, SViewport*) override;

#ifdef OVR_SDK_RENDERING
	virtual void CalculateRenderTargetSize(const FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;
	virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) override;
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

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
    virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual uint32 GetNumberOfBufferedFrames() const override { return 1; }

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

	virtual void DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FIntPoint& TextureSize) override;
	virtual void UpdatePostProcessSettings(FPostProcessSettings*) override;

	virtual bool HandleInputKey(class UPlayerInput*, const FKey& Key, EInputEvent EventType, float AmountDepressed, bool bGamepad) override;

	virtual void OnBeginPlay() override;
	virtual void OnEndPlay() override;

	virtual void DrawDebug(UCanvas* Canvas) override;

	/* Raw sensor data structure. */
	struct SensorData
	{
		FVector Accelerometer;	// Acceleration reading in m/s^2.
		FVector Gyro;			// Rotation rate in rad/s.
		FVector Magnetometer;   // Magnetic field in Gauss.
		float Temperature;		// Temperature of the sensor in degrees Celsius.
		float TimeInSeconds;	// Time when the reported IMU reading took place, in seconds.
	};

	/**
	* Reports raw sensor data. If HMD doesn't support any of the parameters then it should be set to zero.
	*
	* @param OutData	(out) SensorData structure to be filled in.
	*/
	virtual void GetRawSensorData(SensorData& OutData);

	/**
	* User profile structure.
	*/
	struct UserProfile
	{
		FString Name;
		FString Gender;
		float PlayerHeight;				// Height of the player, in meters
		float EyeHeight;				// Height of the player's eyes, in meters
		float IPD;						// Interpupillary distance, in meters
		FVector2D NeckToEyeDistance;	// Neck-to-eye distance, X - horizontal, Y - vertical, in meters
		TMap<FString, FString> ExtraFields; // extra fields in name / value pairs.
	};
	virtual bool GetUserProfile(UserProfile& OutProfile);

	virtual FString GetVersionString() const override;

	// An improved version of GetCurrentOrientationAndPostion, used from blueprints by OculusLibrary.
	void GetCurrentHMDPose(FQuat& CurrentOrientation, FVector& CurrentPosition,
		bool bUseOrienationForPlayerCamera, bool bUsePositionForPlayerCamera, const FVector& PositionScale);

protected:
	virtual TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> CreateNewGameFrame() const override;
	virtual TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> CreateNewSettings() const override;
	
	virtual bool DoEnableStereo(bool bStereo, bool bApplyToHmd) override;
	virtual void ResetStereoRenderingParams() override;

	virtual void GetCurrentPose(FQuat& CurrentHmdOrientation, FVector& CurrentHmdPosition, bool bUseOrienationForPlayerCamera = false, bool bUsePositionForPlayerCamera = false) override;

public:

#ifdef OVR_SDK_RENDERING

#if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)
	class D3D11Bridge : public FCustomPresent
	{
	public:
		D3D11Bridge();

		// Implementation of FRHICustomPresent
		// Resets Viewport-specific pointers (BackBufferRT, SwapChain).
		virtual void OnBackBufferResize() override;
		// Returns true if Engine should perform its own Present.
		virtual bool Present(int& SyncInterval) override;

		// Implementation of FCustomPresent, called by Plugin itself
		virtual void BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT) override;
		void FinishRendering();
		virtual void Reset() override;
		virtual void Shutdown() override;

	protected:
		void Reset_RenderThread();
	protected: // data
		ovrD3D11Config		Cfg;
		ovrD3D11Texture		EyeTexture[2];				
	};

#endif

#ifdef OVR_GL
	class OGLBridge : public FCustomPresent
	{
	public:
		OGLBridge();

		// Implementation of FRHICustomPresent
		// Resets Viewport-specific resources.
		virtual void OnBackBufferResize() override;
		// Returns true if Engine should perform its own Present.
		virtual bool Present(int& SyncInterval) override;

		// Implementation of FCustomPresent, called by Plugin itself
		virtual void BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT) override;
		void FinishRendering();
		virtual void Reset() override;
		virtual void Shutdown() override
		{
			Reset();
		}

	protected: // data
		ovrGLConfig			Cfg;
		ovrGLTexture		EyeTexture[2];
	};
#endif // OVR_GL
	FCustomPresent* GetActiveRHIBridgeImpl() const;

	void ShutdownRendering();

#else

	virtual void FinishRenderingFrame_RenderThread(FRHICommandListImmediate& RHICmdList) override;

#endif // #ifdef OVR_SDK_RENDERING

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

	/**
	 * Reads the device configuration, and sets up the stereoscopic rendering parameters
	 */
	virtual void UpdateStereoRenderingParams() override;
	virtual void UpdateHmdRenderInfo() override;
	virtual void UpdateDistortionCaps() override;
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

	/** Saves system values before applying overrides. */
	void SaveSystemValues();
	/** Restores system values after overrides applied. */
	void RestoreSystemValues();

	void PrecalculateDistortionMesh();

	class FSceneViewport* FindSceneViewport();

	FGameFrame* GetFrame();
	const FGameFrame* GetFrame() const;

private: // data

	ovrHmd			Hmd;

	union
	{
		struct
		{
			bool64	_PlaceHolder_ : 1;
		};
		uint64 Raw;
	} OCFlags;

#ifdef OVR_SDK_RENDERING

#if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)
	TRefCountPtr<D3D11Bridge>	pD3D11Bridge;
#endif
#if defined(OVR_GL)
	TRefCountPtr<OGLBridge>		pOGLBridge;
#endif
#else // !OVR_SDK_RENDERING
	// this can be set and accessed only from renderthread!
	TSharedPtr<FViewExtension, ESPMode::ThreadSafe> RenderContext;

	//TSharedPtr<FViewExtension, ESPMode::ThreadSafe> FindRenderContext(const FSceneViewFamily& ViewFamily) const;
	FGameFrame* GetRenderFrameFromContext() const;
#endif // OVR_SDK_RENDERING
	
	void*						OSWindowHandle;

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

public:
	static bool bDirectModeHack; // a hack to allow quickly check if Oculus is in Direct mode
};

#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS
#endif //#if PLATFORM_MAC
