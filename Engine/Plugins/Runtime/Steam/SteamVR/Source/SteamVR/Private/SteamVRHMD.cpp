// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SteamVRPrivatePCH.h"
#include "SteamVRHMD.h"

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessHMD.h"
#include "Classes/SteamVRFunctionLibrary.h"


//---------------------------------------------------
// SteamVR Plugin Implementation
//---------------------------------------------------

class FSteamVRPlugin : public ISteamVRPlugin
{
	/** IHeadMountedDisplayModule implementation */
	virtual TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > CreateHeadMountedDisplay() override;

	FString GetModulePriorityKeyName() const
	{
		return FString(TEXT("SteamVR"));
	}

public:
	FSteamVRPlugin::FSteamVRPlugin()
		: VRSystem(nullptr)
	{
	}

	virtual vr::IVRSystem* GetVRSystem() const override
	{
		return VRSystem;
	}

	virtual void SetVRSystem(vr::IVRSystem* InVRSystem) override
	{
		VRSystem = InVRSystem;
	}

	virtual void SetControllerToDeviceMap(int32* InControllerToDeviceMap) override
	{
		if (!GEngine->HMDDevice.IsValid() || (GEngine->HMDDevice->GetHMDDeviceType() != EHMDDeviceType::DT_SteamVR))
		{
			// no valid SteamVR HMD found
			return;
		}

		FSteamVRHMD* SteamVRHMD = static_cast<FSteamVRHMD*>(GEngine->HMDDevice.Get());

		SteamVRHMD->SetControllerToDeviceMap(InControllerToDeviceMap);
	}

private:
	vr::IVRSystem* VRSystem;
};

IMPLEMENT_MODULE( FSteamVRPlugin, SteamVR )

TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > FSteamVRPlugin::CreateHeadMountedDisplay()
{
#if STEAMVR_SUPPORTED_PLATFORMS
	TSharedPtr< FSteamVRHMD, ESPMode::ThreadSafe > SteamVRHMD( new FSteamVRHMD(this) );
	if( SteamVRHMD->IsInitialized() )
	{
		return SteamVRHMD;
	}
#endif//STEAMVR_SUPPORTED_PLATFORMS
	return nullptr;
}


//---------------------------------------------------
// SteamVR IHeadMountedDisplay Implementation
//---------------------------------------------------

#if STEAMVR_SUPPORTED_PLATFORMS


bool FSteamVRHMD::IsHMDEnabled() const
{
	return bHmdEnabled;
}

void FSteamVRHMD::EnableHMD(bool enable)
{
	bHmdEnabled = enable;

	if (!bHmdEnabled)
	{
		EnableStereo(false);
	}
}

EHMDDeviceType::Type FSteamVRHMD::GetHMDDeviceType() const
{
	return EHMDDeviceType::DT_SteamVR;
}

bool FSteamVRHMD::GetHMDMonitorInfo(MonitorInfo& MonitorDesc) 
{
	if (IsInitialized())
	{
		int32 X, Y;
		uint32 Width, Height;
		VRSystem->GetWindowBounds(&X, &Y, &Width, &Height);

		MonitorDesc.MonitorName = DisplayId;
		MonitorDesc.MonitorId	= 0;
		MonitorDesc.DesktopX	= X;
		MonitorDesc.DesktopY	= Y;
		MonitorDesc.ResolutionX = Width;
		MonitorDesc.ResolutionY = Height;

		return true;
	}
	else
	{
		MonitorDesc.MonitorName = "";
		MonitorDesc.MonitorId = 0;
		MonitorDesc.DesktopX = MonitorDesc.DesktopY = MonitorDesc.ResolutionX = MonitorDesc.ResolutionY = 0;
	}

	return false;
}

void FSteamVRHMD::GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const
{
	OutHFOVInDegrees = 0.0f;
	OutVFOVInDegrees = 0.0f;
}

bool FSteamVRHMD::DoesSupportPositionalTracking() const
{
	return true;
}

bool FSteamVRHMD::HasValidTrackingPosition()
{
	return bHmdPosTracking && bHaveVisionTracking;
}

void FSteamVRHMD::GetPositionalTrackingCameraProperties(FVector& OutOrigin, FQuat& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const
{
}

void FSteamVRHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
}

float FSteamVRHMD::GetInterpupillaryDistance() const
{
	return 0.064f;
}

void FSteamVRHMD::GetCurrentPose(FQuat& CurrentOrientation, FVector& CurrentPosition, uint32 DeviceId, bool bForceRefresh /* = false*/)
{
	if (VRSystem == nullptr)
	{
		return;
	}

	check(DeviceId < vr::k_unMaxTrackedDeviceCount);

	if (bForceRefresh)
	{
		// With SteamVR, we should only update on the PreRender_ViewFamily, and then the next frame should use the previous frame's results
		check(IsInRenderingThread());

		TrackingFrame.FrameNumber = GFrameNumberRenderThread;

		vr::TrackedDevicePose_t Poses[vr::k_unMaxTrackedDeviceCount];
		VRCompositor->WaitGetPoses(Poses, ARRAYSIZE(Poses));

		for (uint32 i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
		{
			TrackingFrame.bDeviceIsConnected[i] = Poses[i].bDeviceIsConnected;
			TrackingFrame.bPoseIsValid[i] = Poses[i].bPoseIsValid;

			FVector LocalCurrentPosition;
			FQuat LocalCurrentOrientation;
			PoseToOrientationAndPosition(Poses[i].mDeviceToAbsoluteTracking, LocalCurrentOrientation, LocalCurrentPosition);

			TrackingFrame.DeviceOrientation[i] = LocalCurrentOrientation;
			TrackingFrame.DevicePosition[i] = LocalCurrentPosition;

			TrackingFrame.RawPoses[i] = Poses[i].mDeviceToAbsoluteTracking;
		}
	}
	
	// Update CurrentOrientation and CurrentPosition for the desired device, if valid
	if (TrackingFrame.bPoseIsValid[DeviceId])
 	{
		CurrentOrientation = TrackingFrame.DeviceOrientation[DeviceId];
		CurrentPosition = TrackingFrame.DevicePosition[DeviceId];
 	}
}



bool FSteamVRHMD::IsInsideSoftBounds()
{
	if (VRChaperone)
	{
		vr::HmdMatrix34_t VRPose = TrackingFrame.RawPoses[vr::k_unTrackedDeviceIndex_Hmd];
		FMatrix Pose = ToFMatrix(VRPose);
		
		const FVector HMDLocation(Pose.M[3][0], 0.f, Pose.M[3][2]);

		bool bLastWasNegative = false;

		// Since the order of the soft bounds are points on a plane going clockwise, wind around the sides, checking the crossproduct of the affine side to the affine HMD position.  If they're all on the same side, we're in the bounds
		for (uint8 i = 0; i < 4; ++i)
		{
			const FVector PointA = ChaperoneBounds.SoftBounds.Corners[i];
			const FVector PointB = ChaperoneBounds.SoftBounds.Corners[(i + 1) % 4];

			const FVector AffineSegment = PointB - PointA;
			const FVector AffinePoint = HMDLocation - PointA;
			const FVector CrossProduct = FVector::CrossProduct(AffineSegment, AffinePoint);

			const bool bIsNegative = (CrossProduct.Y < 0);

			// If the cross between the point and the side has flipped, that means we're not consistent, and therefore outside the bounds
			if ((i > 0) && (bLastWasNegative != bIsNegative))
			{
				return false;
			}

			bLastWasNegative = bIsNegative;
		}

		return true;
	}

	return false;
}

/** Helper function to convert bounds from SteamVR space to scaled Unreal space*/
TArray<FVector> ConvertBoundsToUnrealSpace(const FBoundingQuad& InBounds, const float WorldToMetersScale)
{
	TArray<FVector> Bounds;

	for (int32 i = 0; i < ARRAYSIZE(InBounds.Corners); ++i)
	{
		const FVector SteamVRCorner = InBounds.Corners[i];
		const FVector UnrealVRCorner(-SteamVRCorner.Z, SteamVRCorner.X, SteamVRCorner.Y);
		Bounds.Add(UnrealVRCorner * WorldToMetersScale);
	}

	return Bounds;
}

TArray<FVector> FSteamVRHMD::GetSoftBounds() const
{
	return ConvertBoundsToUnrealSpace(ChaperoneBounds.SoftBounds, WorldToMetersScale);
}

TArray<FVector> FSteamVRHMD::GetHardBounds() const
{
	TArray<FVector> Bounds;

	for (int32 i = 0; i < ChaperoneBounds.HardBounds.Num(); ++i)
	{
		Bounds.Append(ConvertBoundsToUnrealSpace(ChaperoneBounds.HardBounds[i], WorldToMetersScale));
	}
	
	return Bounds;
}

void FSteamVRHMD::SetTrackingSpace(TEnumAsByte<ESteamVRTrackingSpace::Type> NewSpace)
{
	if(VRCompositor)
	{
		vr::TrackingUniverseOrigin NewOrigin;

		switch(NewSpace)
		{
			case ESteamVRTrackingSpace::Seated:
				NewOrigin = vr::TrackingUniverseOrigin::TrackingUniverseSeated;
				break;
			case ESteamVRTrackingSpace::Standing:
			default:
				NewOrigin = vr::TrackingUniverseOrigin::TrackingUniverseStanding;
				break;
		}

		VRCompositor->SetTrackingSpace(NewOrigin);
	}
}

ESteamVRTrackingSpace::Type FSteamVRHMD::GetTrackingSpace() const
{
	if(VRCompositor)
	{
		const vr::TrackingUniverseOrigin CurrentOrigin = VRCompositor->GetTrackingSpace();

		switch(CurrentOrigin)
		{
		case vr::TrackingUniverseOrigin::TrackingUniverseSeated:
			return ESteamVRTrackingSpace::Seated;
		case vr::TrackingUniverseOrigin::TrackingUniverseStanding:
		default:
			return ESteamVRTrackingSpace::Standing;
		}
	}

	// By default, assume standing
	return ESteamVRTrackingSpace::Standing;
}

void FSteamVRHMD::SetControllerToDeviceMap(int32* InControllerToDeviceMap)
{
	check(sizeof(ControllerToDeviceMap) == (MAX_STEAMVR_CONTROLLERS*sizeof(int32)));

	FMemory::Memcpy(ControllerToDeviceMap, InControllerToDeviceMap, sizeof(ControllerToDeviceMap));
}

void FSteamVRHMD::PoseToOrientationAndPosition(const vr::HmdMatrix34_t& InPose, FQuat& OutOrientation, FVector& OutPosition) const
{
	FMatrix Pose = ToFMatrix(InPose);
	FQuat Orientation(Pose);
 
	OutOrientation.X = -Orientation.Z;
	OutOrientation.Y = Orientation.X;
	OutOrientation.Z = Orientation.Y;
 	OutOrientation.W = -Orientation.W;	

	FVector Position = FVector(-Pose.M[3][2], Pose.M[3][0], Pose.M[3][1]) * WorldToMetersScale;
	OutPosition = BaseOrientation.Inverse().RotateVector(Position);

	OutOrientation = BaseOrientation.Inverse() * OutOrientation;
	OutOrientation.Normalize();
}

void FSteamVRHMD::GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	check(IsInGameThread());
	GetCurrentPose(CurHmdOrientation, CurHmdPosition);
	CurrentOrientation = LastHmdOrientation = CurHmdOrientation;

	CurrentPosition = CurHmdPosition;
}

ESteamVRTrackedDeviceType::Type FSteamVRHMD::GetTrackedDeviceType(uint32 DeviceId) const
{
	vr::TrackedDeviceClass DeviceClass = VRSystem->GetTrackedDeviceClass(DeviceId);

	switch (DeviceClass)
	{
	case vr::TrackedDeviceClass_Controller:
		return ESteamVRTrackedDeviceType::Controller;
	case vr::TrackedDeviceClass_TrackingReference:
		return ESteamVRTrackedDeviceType::TrackingReference;
	case vr::TrackedDeviceClass_Other:
		return ESteamVRTrackedDeviceType::Other;
	default:
		return ESteamVRTrackedDeviceType::Invalid;
	}
}


void FSteamVRHMD::GetTrackedDeviceIds(ESteamVRTrackedDeviceType::Type DeviceType, TArray<int32>& TrackedIds)
{
	TrackedIds.Empty();

	for (uint32 i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
	{
		// Add only devices with a currently valid tracked pose, and exclude the HMD
		if ((i != vr::k_unTrackedDeviceIndex_Hmd) 
			&& TrackingFrame.bPoseIsValid[i]
			&& (GetTrackedDeviceType(i) == DeviceType))
		{
			TrackedIds.Add(i);
		}
	}
}

bool FSteamVRHMD::GetTrackedObjectOrientationAndPosition(uint32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	check(IsInGameThread());

	bool bHasValidPose = false;

	if (DeviceId < vr::k_unMaxTrackedDeviceCount)
	{
		CurrentOrientation = TrackingFrame.DeviceOrientation[DeviceId];
		CurrentPosition = TrackingFrame.DevicePosition[DeviceId];

		bHasValidPose = TrackingFrame.bPoseIsValid[DeviceId] && TrackingFrame.bDeviceIsConnected[DeviceId];
	}

	return bHasValidPose;
}

bool FSteamVRHMD::GetTrackedDeviceIdFromControllerIndex(int32 ControllerIndex, int32& OutDeviceId)
{
	check(IsInGameThread());

	OutDeviceId = -1;

	if ((ControllerIndex < 0) || (ControllerIndex >= MAX_STEAMVR_CONTROLLERS))
	{
		return false;
	}

	OutDeviceId = ControllerToDeviceMap[ControllerIndex];
	
	return (OutDeviceId != -1);
}

TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> FSteamVRHMD::GetViewExtension()
{
	TSharedPtr<FSteamVRHMD, ESPMode::ThreadSafe> ptr(AsShared());
	return StaticCastSharedPtr<ISceneViewExtension>(ptr);
}

void FSteamVRHMD::ApplyHmdRotation(APlayerController* PC, FRotator& ViewRotation)
{
	ViewRotation.Normalize();

	GetCurrentPose(CurHmdOrientation, CurHmdPosition);
	LastHmdOrientation = CurHmdOrientation;

	const FRotator DeltaRot = ViewRotation - PC->GetControlRotation();
	DeltaControlRotation = (DeltaControlRotation + DeltaRot).GetNormalized();

	// Pitch from other sources is never good, because there is an absolute up and down that must be respected to avoid motion sickness.
	// Same with roll.
	DeltaControlRotation.Pitch = 0;
	DeltaControlRotation.Roll = 0;
	DeltaControlOrientation = DeltaControlRotation.Quaternion();

	ViewRotation = FRotator(DeltaControlOrientation * CurHmdOrientation);
}

void FSteamVRHMD::UpdatePlayerCameraRotation(APlayerCameraManager* Camera, struct FMinimalViewInfo& POV)
{
	GetCurrentPose(CurHmdOrientation, CurHmdPosition);
	LastHmdOrientation = CurHmdOrientation;

	DeltaControlRotation = POV.Rotation;
	DeltaControlOrientation = DeltaControlRotation.Quaternion();

	// Apply HMD orientation to camera rotation.
	POV.Rotation = FRotator(POV.Rotation.Quaternion() * CurHmdOrientation);
}

bool FSteamVRHMD::IsChromaAbCorrectionEnabled() const
{
	return true;
}

bool FSteamVRHMD::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (FParse::Command( &Cmd, TEXT("STEREO") ))
	{
		if (FParse::Command(&Cmd, TEXT("ON")))
		{
			if (!IsHMDEnabled())
			{
				Ar.Logf(TEXT("HMD is disabled. Use 'hmd enable' to re-enable it."));
			}
			EnableStereo(true);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("OFF")))
		{
			EnableStereo(false);
			return true;
		}

		float val;
		if (FParse::Value(Cmd, TEXT("E="), val))
		{
			IPD = val;
		}
	}
	else if (FParse::Command(&Cmd, TEXT("HMD")))
	{
		if (FParse::Command(&Cmd, TEXT("ENABLE")))
		{
			EnableHMD(true);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("DISABLE")))
		{
			EnableHMD(false);
			return true;
		}

		int32 val;
		if (FParse::Value(Cmd, TEXT("MIRROR"), val))
		{
			if ((val >= 0) && (val <= 2))
			{
				WindowMirrorMode = val;
			}
			else
		{
				Ar.Logf(TEXT("HMD MIRROR accepts values from 0 though 2"));
		}

			return true;
		}
	}
	else if (FParse::Command(&Cmd, TEXT("UNCAPFPS")))
	{
		GEngine->bSmoothFrameRate = false;
		return true;
	}

	return false;
}

void FSteamVRHMD::OnScreenModeChange(EWindowMode::Type WindowMode)
{
	EnableStereo(WindowMode != EWindowMode::Windowed);
}

bool FSteamVRHMD::IsPositionalTrackingEnabled() const
{
	return bHmdPosTracking;
}

bool FSteamVRHMD::EnablePositionalTracking(bool enable)
{
	bHmdPosTracking = enable;
	return IsPositionalTrackingEnabled();
}

bool FSteamVRHMD::IsHeadTrackingAllowed() const
{
	return GEngine->IsStereoscopic3D();
}

bool FSteamVRHMD::IsInLowPersistenceMode() const
{
	return true;
}

void FSteamVRHMD::EnableLowPersistenceMode(bool Enable)
{
}


void FSteamVRHMD::ResetOrientationAndPosition(float yaw)
{
	ResetOrientation(yaw);
	ResetPosition();
}

void FSteamVRHMD::ResetOrientation(float Yaw)
{
	FRotator ViewRotation;
	ViewRotation = FRotator(TrackingFrame.DeviceOrientation[vr::k_unTrackedDeviceIndex_Hmd]);
	ViewRotation.Pitch = 0;
	ViewRotation.Roll = 0;

	if (Yaw != 0.f)
	{
		// apply optional yaw offset
		ViewRotation.Yaw -= Yaw;
		ViewRotation.Normalize();
	}

	BaseOrientation = ViewRotation.Quaternion();
}
void FSteamVRHMD::ResetPosition()
{
	//@todo steamvr: ResetPosition()
}

void FSteamVRHMD::SetClippingPlanes(float NCP, float FCP)
{
}

void FSteamVRHMD::SetBaseRotation(const FRotator& BaseRot)
{
}
FRotator FSteamVRHMD::GetBaseRotation() const
{
	return FRotator::ZeroRotator;
}

void FSteamVRHMD::SetBaseOrientation(const FQuat& BaseOrient)
{
	BaseOrientation = BaseOrient;
}

FQuat FSteamVRHMD::GetBaseOrientation() const
{
	return BaseOrientation;
}

bool FSteamVRHMD::IsStereoEnabled() const
{
	return bStereoEnabled && bHmdEnabled;
}

bool FSteamVRHMD::EnableStereo(bool stereo)
{
	bStereoEnabled = (IsHMDEnabled()) ? stereo : false;

	FSystemResolution::RequestResolutionChange(1280, 720, (stereo) ? EWindowMode::WindowedMirror : EWindowMode::Windowed);

	return bStereoEnabled;
}

void FSteamVRHMD::AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	//@todo steamvr: get the actual rects from steamvr
	SizeX = SizeX / 2;
	if( StereoPass == eSSP_RIGHT_EYE )
	{
		X += SizeX;
	}
}

void FSteamVRHMD::CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, const FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
	if( StereoPassType != eSSP_FULL)
	{
		vr::Hmd_Eye HmdEye = (StereoPassType == eSSP_LEFT_EYE) ? vr::Eye_Left : vr::Eye_Right;
		vr::HmdMatrix34_t HeadFromEye = VRSystem->GetEyeToHeadTransform(HmdEye);

		// grab the eye position, currently ignoring the rotation supplied by GetHeadFromEyePose()
		FVector TotalOffset = FVector(-HeadFromEye.m[2][3], HeadFromEye.m[0][3], HeadFromEye.m[1][3]) * WorldToMeters;

		ViewLocation += ViewRotation.Quaternion().RotateVector(TotalOffset);

 		const FVector vHMDPosition = DeltaControlOrientation.RotateVector(TrackingFrame.DevicePosition[vr::k_unTrackedDeviceIndex_Hmd]);
		ViewLocation += vHMDPosition;
	}
}

FMatrix FSteamVRHMD::GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType, const float FOV) const
{
	check(IsStereoEnabled());

	vr::Hmd_Eye HmdEye = (StereoPassType == eSSP_LEFT_EYE) ? vr::Eye_Left : vr::Eye_Right;
	float Left, Right, Top, Bottom;

	VRSystem->GetProjectionRaw(HmdEye, &Right, &Left, &Top, &Bottom);
	Bottom *= -1.0f;
	Top *= -1.0f;
	Right *= -1.0f;
	Left *= -1.0f;

	float ZNear = GNearClippingPlane;

	float SumRL = (Right + Left);
	float SumTB = (Top + Bottom);
	float InvRL = (1.0f / (Right - Left));
	float InvTB = (1.0f / (Top - Bottom));

#if 1
	FMatrix Mat = FMatrix(
		FPlane((2.0f * InvRL), 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, (2.0f * InvTB), 0.0f, 0.0f),
		FPlane((SumRL * InvRL), (SumTB * InvTB), 0.0f, 1.0f),
		FPlane(0.0f, 0.0f, ZNear, 0.0f)
		);
#else
	vr::HmdMatrix44_t SteamMat = VRSystem->GetProjectionMatrix(HmdEye, ZNear, 10000.0f, vr::API_DirectX);
	FMatrix Mat = ToFMatrix(SteamMat);

	Mat.M[3][3] = 0.0f;
	Mat.M[2][3] = 1.0f;
	Mat.M[2][2] = 0.0f;
	Mat.M[3][2] = ZNear;
#endif

	return Mat;

}

void FSteamVRHMD::InitCanvasFromView(FSceneView* InView, UCanvas* Canvas)
{
}

void FSteamVRHMD::GetEyeRenderParams_RenderThread(const FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const
{
	if (Context.View.StereoPass == eSSP_LEFT_EYE)
	{
		EyeToSrcUVOffsetValue.X = 0.0f;
		EyeToSrcUVOffsetValue.Y = 0.0f;

		EyeToSrcUVScaleValue.X = 0.5f;
		EyeToSrcUVScaleValue.Y = 1.0f;
	}
	else
	{
		EyeToSrcUVOffsetValue.X = 0.5f;
		EyeToSrcUVOffsetValue.Y = 0.0f;

		EyeToSrcUVScaleValue.X = 0.5f;
		EyeToSrcUVScaleValue.Y = 1.0f;
	}
}


void FSteamVRHMD::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	InViewFamily.EngineShowFlags.MotionBlur = 0;
	InViewFamily.EngineShowFlags.HMDDistortion = false;
	InViewFamily.EngineShowFlags.StereoRendering = IsStereoEnabled();
}

void FSteamVRHMD::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	InView.BaseHmdOrientation = LastHmdOrientation;
	InView.BaseHmdLocation = LastHmdPosition;
	WorldToMetersScale = InView.WorldToMetersScale;
	InViewFamily.bUseSeparateRenderTarget = true;
}

void FSteamVRHMD::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& View)
{
	check(IsInRenderingThread());

	// The last view location used to set the view will be in BaseHmdOrientation.  We need to calculate the delta from that, so that
	// cameras that rely on game objects (e.g. other components) for their positions don't need to be updated on the render thread.
	const FQuat DeltaOrient = View.BaseHmdOrientation.Inverse() * TrackingFrame.DeviceOrientation[vr::k_unTrackedDeviceIndex_Hmd];
	View.ViewRotation = FRotator(View.ViewRotation.Quaternion() * DeltaOrient);
 	View.UpdateViewMatrix();
}

void FSteamVRHMD::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());
	GetActiveRHIBridgeImpl()->BeginRendering();

	FVector CurrentPosition;
	FQuat CurrentOrientation;
	GetCurrentPose(CurrentOrientation, CurrentPosition, vr::k_unTrackedDeviceIndex_Hmd, true);
}

void FSteamVRHMD::UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& InViewport, SViewport* ViewportWidget)
{
	check(IsInGameThread());

	FRHIViewport* const ViewportRHI = InViewport.GetViewportRHI().GetReference();

	if (!IsStereoEnabled())
	{
		if (!bUseSeparateRenderTarget)
		{
			ViewportRHI->SetCustomPresent(nullptr);
		}
		return;
	}

	GetActiveRHIBridgeImpl()->UpdateViewport(InViewport, ViewportRHI);
}

FSteamVRHMD::BridgeBaseImpl* FSteamVRHMD::GetActiveRHIBridgeImpl()
{
#if PLATFORM_WINDOWS
	if (pD3D11Bridge)
	{
		return pD3D11Bridge;
	}
#endif

	return nullptr;
}

void FSteamVRHMD::CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	check(IsInGameThread());

//	if (Flags.bScreenPercentageEnabled)
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage"));
		float value = CVar->GetValueOnGameThread();
		if (value > 0.0f)
		{
			InOutSizeX = FMath::CeilToInt(InOutSizeX * value / 100.f);
			InOutSizeY = FMath::CeilToInt(InOutSizeY * value / 100.f);
		}
	}
}

bool FSteamVRHMD::NeedReAllocateViewportRenderTarget(const FViewport& Viewport)
{
	check(IsInGameThread());

	if (IsStereoEnabled())
	{
		const uint32 InSizeX = Viewport.GetSizeXY().X;
		const uint32 InSizeY = Viewport.GetSizeXY().Y;
		FIntPoint RenderTargetSize;
		RenderTargetSize.X = Viewport.GetRenderTargetTexture()->GetSizeX();
		RenderTargetSize.Y = Viewport.GetRenderTargetTexture()->GetSizeY();

		uint32 NewSizeX = InSizeX, NewSizeY = InSizeY;
		CalculateRenderTargetSize(Viewport, NewSizeX, NewSizeY);
		if (NewSizeX != RenderTargetSize.X || NewSizeY != RenderTargetSize.Y)
		{
			return true;
		}
	}
	return false;
}

FSteamVRHMD::FSteamVRHMD(ISteamVRPlugin* SteamVRPlugin) :
	VRSystem(nullptr),
	bHmdEnabled(true),
	bStereoEnabled(false),
	bHmdPosTracking(true),
	bHaveVisionTracking(false),
	IPD(0.064f),
	WindowMirrorMode(1),
	CurHmdOrientation(FQuat::Identity),
	LastHmdOrientation(FQuat::Identity),
	BaseOrientation(FQuat::Identity),
	DeltaControlRotation(FRotator::ZeroRotator),
	DeltaControlOrientation(FQuat::Identity),
	CurHmdPosition(FVector::ZeroVector),
	WorldToMetersScale(100.0f),
	SteamVRPlugin(SteamVRPlugin),
	RendererModule(nullptr),
	OpenVRDLLHandle(nullptr),
	IdealScreenPercentage(100.0f)
{
	Startup();
}

FSteamVRHMD::~FSteamVRHMD()
{
	Shutdown();
}

bool FSteamVRHMD::IsInitialized() const
{
	return (VRSystem != nullptr);
}

void FSteamVRHMD::Startup()
{
	// load the OpenVR library
	if (!LoadOpenVRModule())
	{
		return;
	}

	// grab a pointer to the renderer module for displaying our mirror window
	static const FName RendererModuleName("Renderer");
	RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);

	vr::HmdError HmdErr = vr::HmdError_None;
	//VRSystem = vr::VR_Init(&HmdErr);
	VRSystem = (*VRInitFn)(&HmdErr);

	// make sure that the version of the HMD we're compiled against is correct
	if ((VRSystem != nullptr) && (HmdErr == vr::HmdError_None))
	{
		//VRSystem = (vr::IVRSystem*)vr::VR_GetGenericInterface(vr::IVRSystem_Version, &HmdErr);	//@todo steamvr: verify init error handling
		VRSystem = (vr::IVRSystem*)(*VRGetGenericInterfaceFn)(vr::IVRSystem_Version, &HmdErr);
	}

	// attach to the compositor
	if ((VRSystem != nullptr) && (HmdErr == vr::HmdError_None))
	{
		//VRCompositor = (vr::IVRCompositor*)vr::VR_GetGenericInterface(vr::IVRCompositor_Version, &HmdErr);
		VRCompositor = (vr::IVRCompositor*)(*VRGetGenericInterfaceFn)(vr::IVRCompositor_Version, &HmdErr);

		if ((VRCompositor != nullptr) && (HmdErr == vr::HmdError_None))
		{
			// determine our compositor type
			vr::Compositor_DeviceType CompositorDeviceType = vr::Compositor_DeviceType_None;
			if (IsPCPlatform(GMaxRHIShaderPlatform) && !IsOpenGLPlatform(GMaxRHIShaderPlatform))
			{
				CompositorDeviceType = vr::Compositor_DeviceType_D3D11;
			}
			else if (IsOpenGLPlatform(GMaxRHIShaderPlatform))
			{
				check(0);	//@todo steamvr: use old path for mac and linux until support is added
				CompositorDeviceType = vr::Compositor_DeviceType_OpenGL;
			}
		}
	}

	if ((VRSystem != nullptr) && (HmdErr == vr::HmdError_None))
	{
		// grab info about the attached display
		char Buf[128];
		FString DriverId;
		vr::TrackedPropertyError Error;

		VRSystem->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String, Buf, sizeof(Buf), &Error);
		if (Error == vr::TrackedProp_Success)
		{
			DriverId = FString(UTF8_TO_TCHAR(Buf));
		}

		VRSystem->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String, Buf, sizeof(Buf), &Error);
		if (Error == vr::TrackedProp_Success)
		{
			DisplayId = FString(UTF8_TO_TCHAR(Buf));
		}

		// determine our ideal screen percentage
		uint32 RecommendedWidth, RecommendedHeight;
		VRSystem->GetRecommendedRenderTargetSize(&RecommendedWidth, &RecommendedHeight);
		RecommendedWidth *= 2;

		int32 ScreenX, ScreenY;
		uint32 ScreenWidth, ScreenHeight;
		VRSystem->GetWindowBounds(&ScreenX, &ScreenY, &ScreenWidth, &ScreenHeight);

		float WidthPercentage = ((float)RecommendedWidth / (float)ScreenWidth) * 100.0f;
		float HeightPercentage = ((float)RecommendedHeight / (float)ScreenHeight) * 100.0f;

		float IdealScreenPercentage = FMath::Max(WidthPercentage, HeightPercentage);

		//@todo steamvr: move out of here
		static IConsoleVariable* CScrPercVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));

		if (FMath::RoundToInt(CScrPercVar->GetFloat()) != FMath::RoundToInt(IdealScreenPercentage))
		{
			CScrPercVar->Set(IdealScreenPercentage);
		}

		// disable vsync
		static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
		CVSyncVar->Set(false);

		// enforce finishcurrentframe
		static IConsoleVariable* CFCFVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.finishcurrentframe"));
		CFCFVar->Set(false);

		// Uncap fps to enable FPS higher than 62
		GEngine->bSmoothFrameRate = false;

		// Grab the chaperone
		vr::HmdError ChaperoneErr = vr::HmdError_None;
		//VRChaperone = (vr::IVRChaperone*)vr::VR_GetGenericInterface(vr::IVRChaperone_Version, &ChaperoneErr);
		VRChaperone = (vr::IVRChaperone*)(*VRGetGenericInterfaceFn)(vr::IVRChaperone_Version, &ChaperoneErr);
		if ((VRChaperone != nullptr) && (ChaperoneErr == vr::HmdError_None))
		{
			ChaperoneBounds = FChaperoneBounds(VRChaperone);
		}
		else
		{
			UE_LOG(LogHMD, Warning, TEXT("Failed to initialize Chaperone.  Error: %d"), (int32)ChaperoneErr);
		}

		// Initialize our controller to device index
		for (int32 i = 0; i < MAX_STEAMVR_CONTROLLERS; ++i)
		{
			ControllerToDeviceMap[i] = -1;
		}

#if PLATFORM_WINDOWS
		if (IsPCPlatform(GMaxRHIShaderPlatform) && !IsOpenGLPlatform(GMaxRHIShaderPlatform))
		{
			pD3D11Bridge = new D3D11Bridge(this);
		}
#endif

		LoadFromIni();

		UE_LOG(LogHMD, Log, TEXT("SteamVR initialized.  Driver: %s  Display: %s"), *DriverId, *DisplayId);
	}
	else
	{
		UE_LOG(LogHMD, Warning, TEXT("SteamVR failed to initialize.  Error: %d"), (int32)HmdErr);

		VRSystem = nullptr;
	}

	// share the IVRSystem interface out to the SteamVRController via the module layer
	SteamVRPlugin->SetVRSystem(VRSystem);
}

void FSteamVRHMD::LoadFromIni()
{
	const TCHAR* SteamVRSettings = TEXT("SteamVR.Settings");
	int32 i;

	if (GConfig->GetInt(SteamVRSettings, TEXT("WindowMirrorMode"), i, GEngineIni))
	{
		WindowMirrorMode = i;
	}
}

void FSteamVRHMD::SaveToIni()
{
	const TCHAR* SteamVRSettings = TEXT("SteamVR.Settings");
	GConfig->SetInt(SteamVRSettings, TEXT("WindowMirrorMode"), WindowMirrorMode, GEngineIni);
}

void FSteamVRHMD::Shutdown()
{
	if (VRSystem != nullptr)
	{
		// save any runtime configuration changes to the .ini
		SaveToIni();

		// shut down our headset
		VRSystem = nullptr;
		SteamVRPlugin->SetVRSystem(nullptr);
		//vr::VR_Shutdown();
		(*VRShutdownFn)();
	}

	// unload OpenVR library
	UnloadOpenVRModule();
}

bool FSteamVRHMD::LoadOpenVRModule()
{
#if PLATFORM_WINDOWS
	#if PLATFORM_64BITS
		FString RootOpenVRPath;
		TCHAR VROverridePath[MAX_PATH];
		FPlatformMisc::GetEnvironmentVariable(TEXT("VR_OVERRIDE"), VROverridePath, MAX_PATH);
	
		if (FCString::Strlen(VROverridePath) > 0)
		{
			RootOpenVRPath = FString::Printf(TEXT("%s\\bin\\win64\\"), VROverridePath);
		}
		else
		{
			RootOpenVRPath = FPaths::EngineDir() / FString::Printf(TEXT("Binaries/ThirdParty/OpenVR/%s/Win64/"), OPENVR_SDK_VER);
		}
		
		FPlatformProcess::PushDllDirectory(*RootOpenVRPath);
		OpenVRDLLHandle = FPlatformProcess::GetDllHandle(*(RootOpenVRPath + "openvr_api.dll"));
		FPlatformProcess::PopDllDirectory(*RootOpenVRPath);
#else
		FString RootOpenVRPath = FPaths::EngineDir() / FString::Printf(TEXT("Binaries/ThirdParty/OpenVR/%s/Win32/"), OPENVR_SDK_VER); 
		FPlatformProcess::PushDllDirectory(*RootOpenVRPath);
		OpenVRDLLHandle = FPlatformProcess::GetDllHandle(*(RootOpenVRPath + "openvr_api.dll"));
		FPlatformProcess::PopDllDirectory(*RootOpenVRPath);
	#endif
#elif PLATFORM_MAC
	OpenVRDLLHandle = FPlatformProcess::GetDllHandle(TEXT("libopenvr_api.dylib"));
#endif	//PLATFORM_WINDOWS

	if (!OpenVRDLLHandle)
	{
		UE_LOG(LogHMD, Warning, TEXT("Failed to load OpenVR library."));
		return false;
	}

	//@todo steamvr: Remove GetProcAddress() workaround once we update to Steamworks 1.33 or higher
	VRInitFn = (pVRInit)FPlatformProcess::GetDllExport(OpenVRDLLHandle, TEXT("VR_Init"));
	VRShutdownFn = (pVRShutdown)FPlatformProcess::GetDllExport(OpenVRDLLHandle, TEXT("VR_Shutdown"));
	VRIsHmdPresentFn = (pVRIsHmdPresent)FPlatformProcess::GetDllExport(OpenVRDLLHandle, TEXT("VR_IsHmdPresent"));
	VRGetStringForHmdErrorFn = (pVRGetStringForHmdError)FPlatformProcess::GetDllExport(OpenVRDLLHandle, TEXT("VR_GetStringForHmdError"));
	VRGetGenericInterfaceFn = (pVRGetGenericInterface)FPlatformProcess::GetDllExport(OpenVRDLLHandle, TEXT("VR_GetGenericInterface"));

	if (!VRInitFn || !VRShutdownFn || !VRIsHmdPresentFn || !VRGetStringForHmdErrorFn || !VRGetGenericInterfaceFn)
	{
		UE_LOG(LogHMD, Warning, TEXT("Failed to GetProcAddress() on openvr_api.dll"));
		UnloadOpenVRModule();
		return false;
	}

	return true;
}

void FSteamVRHMD::UnloadOpenVRModule()
{
	if (OpenVRDLLHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(OpenVRDLLHandle);
		OpenVRDLLHandle = nullptr;
	}
}

#endif //STEAMVR_SUPPORTED_PLATFORMS
