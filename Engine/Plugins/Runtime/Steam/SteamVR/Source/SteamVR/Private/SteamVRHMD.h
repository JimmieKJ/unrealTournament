// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ISteamVRPlugin.h"
#include "HeadMountedDisplay.h"
#include "IHeadMountedDisplay.h"
#include "SteamVRFunctionLibrary.h"

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#include <d3d11.h>
#include "HideWindowsPlatformTypes.h"
#endif

#if STEAMVR_SUPPORTED_PLATFORMS

#include "SceneViewExtension.h"

/** Stores vectors, in clockwise order, to define soft and hard bounds for Chaperone */
struct FBoundingQuad
{
	FVector Corners[4];
};

//@todo steamvr: remove GetProcAddress() workaround once we have updated to Steamworks 1.33 or higher
typedef vr::IVRSystem*(VR_CALLTYPE *pVRInit)(vr::HmdError* peError);
typedef void(VR_CALLTYPE *pVRShutdown)();
typedef bool(VR_CALLTYPE *pVRIsHmdPresent)();
typedef const char*(VR_CALLTYPE *pVRGetStringForHmdError)(vr::HmdError error);
typedef void*(VR_CALLTYPE *pVRGetGenericInterface)(const char* pchInterfaceVersion, vr::HmdError* peError);


/**
 * SteamVR Head Mounted Display
 */
class FSteamVRHMD : public IHeadMountedDisplay, public ISceneViewExtension, public TSharedFromThis<FSteamVRHMD, ESPMode::ThreadSafe>
{
public:
	/** IHeadMountedDisplay interface */
	virtual bool IsHMDConnected() override { return true; }
	virtual bool IsHMDEnabled() const override;
	virtual void EnableHMD(bool allow = true) override;
	virtual EHMDDeviceType::Type GetHMDDeviceType() const override;
	virtual bool GetHMDMonitorInfo(MonitorInfo&) override;

	virtual void GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const override;

	virtual bool DoesSupportPositionalTracking() const override;
	virtual bool HasValidTrackingPosition() override;
	virtual void GetPositionalTrackingCameraProperties(FVector& OutOrigin, FQuat& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const override;
	virtual void RebaseObjectOrientationAndPosition(FVector& OutPosition, FQuat& OutOrientation) const override;

	virtual void SetInterpupillaryDistance(float NewInterpupillaryDistance) override;
	virtual float GetInterpupillaryDistance() const override;

	virtual void GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition) override;
	virtual class TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> GetViewExtension() override;
	virtual void ApplyHmdRotation(APlayerController* PC, FRotator& ViewRotation) override;
	virtual void UpdatePlayerCameraRotation(APlayerCameraManager*, struct FMinimalViewInfo& POV) override;

	virtual bool IsChromaAbCorrectionEnabled() const override;

	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;
	virtual void OnScreenModeChange(EWindowMode::Type WindowMode) override;

	virtual bool IsFullscreenAllowed() override { return false; }

	virtual bool IsPositionalTrackingEnabled() const override;
	virtual bool EnablePositionalTracking(bool enable) override;

	virtual bool IsHeadTrackingAllowed() const override;

	virtual bool IsInLowPersistenceMode() const override;
	virtual void EnableLowPersistenceMode(bool Enable = true) override;

	virtual void ResetOrientationAndPosition(float yaw = 0.f) override;
	virtual void ResetOrientation(float Yaw = 0.f) override;
	virtual void ResetPosition() override;

	virtual void SetClippingPlanes(float NCP, float FCP) override;

	virtual void SetBaseRotation(const FRotator& BaseRot) override;
	virtual FRotator GetBaseRotation() const override;

	virtual void SetBaseOrientation(const FQuat& BaseOrient) override;
	virtual FQuat GetBaseOrientation() const override;

	virtual bool HasHiddenAreaMesh() const override { return HiddenAreaMeshes[0].IsValid() && HiddenAreaMeshes[1].IsValid(); }
	virtual void DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;

	virtual bool HasVisibleAreaMesh() const override { return VisibleAreaMeshes[0].IsValid() && VisibleAreaMeshes[1].IsValid(); }
	virtual void DrawVisibleAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;

	virtual void DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FIntPoint& TextureSize) override;

	virtual void UpdateScreenSettings(const FViewport* InViewport) override {}

	/** IStereoRendering interface */
	virtual bool IsStereoEnabled() const override;
	virtual bool EnableStereo(bool stereo = true) override;
	virtual void AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
	virtual void CalculateStereoViewOffset(const EStereoscopicPass StereoPassType, const FRotator& ViewRotation, 
		const float MetersToWorld, FVector& ViewLocation) override;
	virtual FMatrix GetStereoProjectionMatrix(const EStereoscopicPass StereoPassType, const float FOV) const override;
	virtual void InitCanvasFromView(FSceneView* InView, UCanvas* Canvas) override;
	virtual void RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef BackBuffer, FTexture2DRHIParamRef SrcTexture) const override;
	virtual void GetEyeRenderParams_RenderThread(const FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const override;
	virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;
	virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) override;
	virtual bool ShouldUseSeparateRenderTarget() const override
	{
		check(IsInGameThread());
		return IsStereoEnabled();
	}
	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& Viewport, SViewport*) override;

	/** ISceneViewExtension interface */
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;

	class BridgeBaseImpl : public FRHICustomPresent
	{
	public:
		BridgeBaseImpl(FSteamVRHMD* plugin) :
			FRHICustomPresent(nullptr),
			Plugin(plugin), 
			bNeedReinitRendererAPI(true), 
			bInitialized(false) 
		{}

		bool IsInitialized() const { return bInitialized; }

		virtual void BeginRendering() = 0;
		virtual void UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI) = 0;
		virtual void SetNeedReinitRendererAPI() { bNeedReinitRendererAPI = true; }

		virtual void Reset() = 0;
		virtual void Shutdown() = 0;

	protected:
		FSteamVRHMD*		Plugin;
		bool				bNeedReinitRendererAPI;
		bool				bInitialized;
	};

#if PLATFORM_WINDOWS
	class D3D11Bridge : public BridgeBaseImpl
	{
	public:
		D3D11Bridge(FSteamVRHMD* plugin);

		virtual void OnBackBufferResize() override;
		virtual bool Present(int& SyncInterval) override;

		virtual void BeginRendering() override;
		void FinishRendering();
		virtual void UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI) override;
		virtual void Reset() override;
		virtual void Shutdown() override
		{
			Reset();
		}

	protected:
		ID3D11Texture2D* RenderTargetTexture = NULL;
	};
#endif // PLATFORM_WINDOWS

	BridgeBaseImpl* GetActiveRHIBridgeImpl();
	void ShutdownRendering();

	/** Motion Controllers */
	ESteamVRTrackedDeviceType GetTrackedDeviceType(uint32 DeviceId) const;
	void GetTrackedDeviceIds(ESteamVRTrackedDeviceType DeviceType, TArray<int32>& TrackedIds);
	bool GetTrackedObjectOrientationAndPosition(uint32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition);
	STEAMVR_API bool GetControllerHandPositionAndOrientation( const int32 ControllerIndex, EControllerHand Hand, FVector& OutPosition, FQuat& OutOrientation );


	/** Chaperone */
	/** Returns whether or not the player is currently inside the soft bounds */
	bool IsInsideSoftBounds();

	/** Returns an array of the soft bounds as Unreal-scaled vectors, relative to the HMD calibration point (0,0,0).  The Z will always be at 0.f */
	TArray<FVector> GetSoftBounds() const;

	/** Returns an array of the hard bounds as Unreal-scaled vectors, relative to the HMD calibration point (0,0,0).  Every four entries will represent one quad of the bounding box */
	TArray<FVector> GetHardBounds() const;

	/** Get the windowed mirror mode.  @todo steamvr: thread safe flags */
	int32 GetWindowMirrorMode() const { return WindowMirrorMode; }

	/** Sets the tracking space for the returned coordinate system (e.g. standing, sitting) */
	void SetTrackingSpace(TEnumAsByte<ESteamVRTrackingSpace> NewSpace);

	/** Returns the tracking space for the returned coordinate system (e.g. standing, sitting) */
	ESteamVRTrackingSpace GetTrackingSpace() const;

	/** Sets the map from Unreal controller id and hand index, to tracked device id. */
	void SetUnrealControllerIdAndHandToDeviceIdMap(int32 InUnrealControllerIdAndHandToDeviceIdMap[ MAX_STEAMVR_CONTROLLER_PAIRS ][ 2 ]);

public:
	/** Constructor */
	FSteamVRHMD(ISteamVRPlugin* SteamVRPlugin);

	/** Destructor */
	virtual ~FSteamVRHMD();

	/** @return	True if the API was initialized OK */
	bool IsInitialized() const;

private:

	/**
	 * Starts up the OpenVR API
	 */
	void Startup();

	/**
	 * Shuts down the OpenVR API
	 */
	void Shutdown();

	void LoadFromIni();
	void SaveToIni();

	bool LoadOpenVRModule();
	void UnloadOpenVRModule();

	void PoseToOrientationAndPosition(const vr::HmdMatrix34_t& Pose, FQuat& OutOrientation, FVector& OutPosition) const;
	void GetCurrentPose(FQuat& CurrentOrientation, FVector& CurrentPosition, uint32 DeviceID = vr::k_unTrackedDeviceIndex_Hmd, bool bForceRefresh=false);

	FORCEINLINE FMatrix ToFMatrix(const vr::HmdMatrix34_t& tm) const
	{
		// Rows and columns are swapped between vr::HmdMatrix34_t and FMatrix
		return FMatrix(
			FPlane(tm.m[0][0], tm.m[1][0], tm.m[2][0], 0.0f),
			FPlane(tm.m[0][1], tm.m[1][1], tm.m[2][1], 0.0f),
			FPlane(tm.m[0][2], tm.m[1][2], tm.m[2][2], 0.0f),
			FPlane(tm.m[0][3], tm.m[1][3], tm.m[2][3], 1.0f));
	}

	FORCEINLINE FMatrix ToFMatrix(const vr::HmdMatrix44_t& tm) const
	{
		// Rows and columns are swapped between vr::HmdMatrix44_t and FMatrix
		return FMatrix(
			FPlane(tm.m[0][0], tm.m[1][0], tm.m[2][0], tm.m[3][0]),
			FPlane(tm.m[0][1], tm.m[1][1], tm.m[2][1], tm.m[3][1]),
			FPlane(tm.m[0][2], tm.m[1][2], tm.m[2][2], tm.m[3][2]),
			FPlane(tm.m[0][3], tm.m[1][3], tm.m[2][3], tm.m[3][3]));
	}

private:

	void SetupOcclusionMeshes();

	bool bHmdEnabled;
	bool bStereoEnabled;
	bool bHmdPosTracking;
	mutable bool bHaveVisionTracking;

 	struct FTrackingFrame
 	{
 		uint32 FrameNumber;
 
 		bool bDeviceIsConnected[vr::k_unMaxTrackedDeviceCount];
 		bool bPoseIsValid[vr::k_unMaxTrackedDeviceCount];
 		FVector DevicePosition[vr::k_unMaxTrackedDeviceCount];
 		FQuat DeviceOrientation[vr::k_unMaxTrackedDeviceCount];

		vr::HmdMatrix34_t RawPoses[vr::k_unMaxTrackedDeviceCount];

		FTrackingFrame()
		{
			FrameNumber = 0;

			const uint32 MaxDevices = vr::k_unMaxTrackedDeviceCount;

			FMemory::Memzero(bDeviceIsConnected, MaxDevices * sizeof(bool));
			FMemory::Memzero(bPoseIsValid, MaxDevices * sizeof(bool));
			FMemory::Memzero(DevicePosition, MaxDevices * sizeof(FVector));

			for (uint32 i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
			{
				DeviceOrientation[i] = FQuat::Identity;
			}

			FMemory::Memzero(RawPoses, MaxDevices * sizeof(vr::HmdMatrix34_t));
		}
 	};
	FTrackingFrame TrackingFrame;

	/** Coverts a SteamVR-space vector to an Unreal-space vector.  Does not handle scaling, only axes conversion */
	FORCEINLINE static FVector CONVERT_STEAMVECTOR_TO_FVECTOR(const vr::HmdVector3_t InVector)
	{
		return FVector(-InVector.v[2], InVector.v[0], InVector.v[1]);
	}

	FORCEINLINE static FVector RAW_STEAMVECTOR_TO_FVECTOR(const vr::HmdVector3_t InVector)
	{
		return FVector(InVector.v[0], InVector.v[1], InVector.v[2]);
	}

	FHMDViewMesh HiddenAreaMeshes[2];
	FHMDViewMesh VisibleAreaMeshes[2];

	/** Chaperone Support */
	struct FChaperoneBounds
	{
		/** Stores the soft bounds in SteamVR HMD space, for fast checking.  These will need to be converted to Unreal HMD-calibrated space before being used in the world */
		FBoundingQuad			SoftBounds;

		/** Stores the hard bounds in SteamVR HMD space, for fast checking.  These will need to be converted to Unreal HMD-calibrated space before being used in the world */
		TArray<FBoundingQuad>	HardBounds;

		uint32 NumHardBounds;

	public:
		FChaperoneBounds()
			: NumHardBounds(0)
		{}

		FChaperoneBounds(vr::IVRChaperone* Chaperone)
			: FChaperoneBounds()
		{
			vr::ChaperoneSoftBoundsInfo_t	VRSoftBounds;
			Chaperone->GetSoftBoundsInfo(&VRSoftBounds);
			for (uint8 i = 0; i < 4; ++i)
			{
				const vr::HmdVector3_t Corner = VRSoftBounds.quadCorners.vCorners[i];
				SoftBounds.Corners[i] = RAW_STEAMVECTOR_TO_FVECTOR(Corner);
			}

			// Check to see if we have any bounds specified.  This MUST be called before actually getting buffer info
			Chaperone->GetHardBoundsInfo(NULL, &NumHardBounds);
			if (NumHardBounds > 0)
			{
				vr::HmdQuad_t* HardBoundsBuf = (vr::HmdQuad_t*)FMemory_Alloca(NumHardBounds * sizeof(vr::HmdQuad_t));
				FMemory::Memzero(HardBoundsBuf, NumHardBounds * sizeof(vr::HmdQuad_t));

				// Actually grab the bounds from the buffer
				Chaperone->GetHardBoundsInfo(HardBoundsBuf, &NumHardBounds);
				for (uint32 i = 0; i < NumHardBounds; ++i)
				{
					FBoundingQuad Quad;
					Quad.Corners[0] = RAW_STEAMVECTOR_TO_FVECTOR(HardBoundsBuf[i].vCorners[0]);
					Quad.Corners[1] = RAW_STEAMVECTOR_TO_FVECTOR(HardBoundsBuf[i].vCorners[1]);
					Quad.Corners[2] = RAW_STEAMVECTOR_TO_FVECTOR(HardBoundsBuf[i].vCorners[2]);
					Quad.Corners[3] = RAW_STEAMVECTOR_TO_FVECTOR(HardBoundsBuf[i].vCorners[3]);

					HardBounds.Add(Quad);
				}
			}
		}
	};
	FChaperoneBounds ChaperoneBounds;
	
	float IPD;
	int32 WindowMirrorMode;		// how to mirror the display contents to the desktop window: 0 - no mirroring, 1 - single eye, 2 - stereo pair

	/** Player's orientation tracking */
	mutable FQuat			CurHmdOrientation;

	FRotator				DeltaControlRotation;    // same as DeltaControlOrientation but as rotator
	FQuat					DeltaControlOrientation; // same as DeltaControlRotation but as quat

	mutable FVector			CurHmdPosition;

	mutable FQuat			LastHmdOrientation; // contains last APPLIED ON GT HMD orientation
	FVector					LastHmdPosition;	// contains last APPLIED ON GT HMD position

	// HMD base values, specify forward orientation and zero pos offset
	FQuat					BaseOrientation;	// base orientation

	/** World units (UU) to Meters scale.  Read from the level, and used to transform positional tracking data */
	float WorldToMetersScale;

	/** Mapping from Unreal Controller Id and Hand to a tracked device id.  Passed in from the controller plugin */
	int32 UnrealControllerIdAndHandToDeviceIdMap[MAX_STEAMVR_CONTROLLER_PAIRS][2];

	IRendererModule* RendererModule;
	ISteamVRPlugin* SteamVRPlugin;

	vr::IVRSystem* VRSystem;
	vr::IVRCompositor* VRCompositor;
	vr::IVRChaperone* VRChaperone;

	void* OpenVRDLLHandle;

	//@todo steamvr: Remove GetProcAddress() workaround once we have updated to Steamworks 1.33 or higher
	pVRInit VRInitFn;
	pVRShutdown VRShutdownFn;
	pVRIsHmdPresent VRIsHmdPresentFn;
	pVRGetStringForHmdError VRGetStringForHmdErrorFn;
	pVRGetGenericInterface VRGetGenericInterfaceFn;
	
	FString DisplayId;

	float IdealScreenPercentage;

#if PLATFORM_WINDOWS
	TRefCountPtr<D3D11Bridge>	pD3D11Bridge;
#endif
};


DEFINE_LOG_CATEGORY_STATIC(LogHMD, Log, All);

#endif //STEAMVR_SUPPORTED_PLATFORMS
