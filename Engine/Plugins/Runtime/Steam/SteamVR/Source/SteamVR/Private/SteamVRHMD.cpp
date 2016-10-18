// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SteamVRPrivatePCH.h"
#include "SteamVRHMD.h"

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneViewport.h"
#include "PostProcess/PostProcessHMD.h"
#include "Classes/SteamVRFunctionLibrary.h"

#include "SteamVRMeshAssets.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#endif

/** Helper function for acquiring the appropriate FSceneViewport */
FSceneViewport* FindSceneViewport()
{
	if (!GIsEditor)
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		return GameEngine->SceneViewport.Get();
	}
#if WITH_EDITOR
	else
	{
		UEditorEngine* EditorEngine = CastChecked<UEditorEngine>( GEngine );
		FSceneViewport* PIEViewport = (FSceneViewport*)EditorEngine->GetPIEViewport();
		if( PIEViewport != nullptr && PIEViewport->IsStereoRenderingAllowed() )
		{
			// PIE is setup for stereo rendering
			return PIEViewport;
		}
		else
		{
			// Check to see if the active editor viewport is drawing in stereo mode
			// @todo vreditor: Should work with even non-active viewport!
			FSceneViewport* EditorViewport = (FSceneViewport*)EditorEngine->GetActiveViewport();
			if( EditorViewport != nullptr && EditorViewport->IsStereoRenderingAllowed() )
			{
				return EditorViewport;
			}
		}
	}
#endif
	return nullptr;
}

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
#if !STEAMVR_SUPPORTED_PLATFORMS
	{
	}
#else //STEAMVR_SUPPORTED_PLATFORMS
		: VRSystem(nullptr)
	{
        LoadOpenVRModule();
	}

	virtual void StartupModule() override
	{
		IHeadMountedDisplayModule::StartupModule();
	
		LoadOpenVRModule();
	}
    
    virtual void ShutdownModule() override
    {
        IHeadMountedDisplayModule::ShutdownModule();
        
        UnloadOpenVRModule();
    }

	virtual vr::IVRSystem* GetVRSystem() const override
	{
		return VRSystem;
	}
    
    bool LoadOpenVRModule()
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
            UE_LOG(LogHMD, Log, TEXT("Failed to load OpenVR library."));
            return false;
        }
        
        //@todo steamvr: Remove GetProcAddress() workaround once we update to Steamworks 1.33 or higher
        FSteamVRHMD::VRIsHmdPresentFn = (pVRIsHmdPresent)FPlatformProcess::GetDllExport(OpenVRDLLHandle, TEXT("VR_IsHmdPresent"));
        FSteamVRHMD::VRGetGenericInterfaceFn = (pVRGetGenericInterface)FPlatformProcess::GetDllExport(OpenVRDLLHandle, TEXT("VR_GetGenericInterface"));
        vr::VR_SetGenericInterfaceCallback(FSteamVRHMD::VRGetGenericInterfaceFn);

        // Verify that we've bound correctly to the DLL functions
        if (!FSteamVRHMD::VRIsHmdPresentFn || !FSteamVRHMD::VRGetGenericInterfaceFn)
        {
            UE_LOG(LogHMD, Log, TEXT("Failed to GetProcAddress() on openvr_api.dll"));
            UnloadOpenVRModule();
            
            return false;
        }
        
        return true;
    }
    
    void UnloadOpenVRModule()
    {
        if (OpenVRDLLHandle != nullptr)
        {
            FPlatformProcess::FreeDllHandle(OpenVRDLLHandle);
            OpenVRDLLHandle = nullptr;
        }
    }

	virtual void SetUnrealControllerIdAndHandToDeviceIdMap(int32 InUnrealControllerIdAndHandToDeviceIdMap[MAX_STEAMVR_CONTROLLER_PAIRS][2]) override
	{
		if (!GEngine->HMDDevice.IsValid() || (GEngine->HMDDevice->GetHMDDeviceType() != EHMDDeviceType::DT_SteamVR))
		{
			// no valid SteamVR HMD found
			return;
		}

		FSteamVRHMD* SteamVRHMD = static_cast<FSteamVRHMD*>(GEngine->HMDDevice.Get());

		SteamVRHMD->SetUnrealControllerIdAndHandToDeviceIdMap(InUnrealControllerIdAndHandToDeviceIdMap);
	}

	virtual bool IsHMDConnected() override
	{
		return FSteamVRHMD::VRIsHmdPresentFn ? (bool)(*FSteamVRHMD::VRIsHmdPresentFn)() : false;
	}

	virtual void Reset() override
	{
		VRSystem = nullptr;
	}


private:
	vr::IVRSystem* VRSystem;
    
    void* OpenVRDLLHandle;
#endif // STEAMVR_SUPPORTED_PLATFORMS
};

IMPLEMENT_MODULE( FSteamVRPlugin, SteamVR )

TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > FSteamVRPlugin::CreateHeadMountedDisplay()
{
#if STEAMVR_SUPPORTED_PLATFORMS
	TSharedPtr< FSteamVRHMD, ESPMode::ThreadSafe > SteamVRHMD( new FSteamVRHMD(this) );
	if( SteamVRHMD->IsInitialized() )
	{
		VRSystem = SteamVRHMD->GetVRSystem();
		return SteamVRHMD;
	}
#endif//STEAMVR_SUPPORTED_PLATFORMS
	return nullptr;
}


//---------------------------------------------------
// SteamVR IHeadMountedDisplay Implementation
//---------------------------------------------------

#if STEAMVR_SUPPORTED_PLATFORMS

bool FSteamVRHMD::bIsQuitting = false;

pVRIsHmdPresent FSteamVRHMD::VRIsHmdPresentFn = nullptr;
pVRGetGenericInterface FSteamVRHMD::VRGetGenericInterfaceFn = nullptr;

bool FSteamVRHMD::IsHMDEnabled() const
{
	return bHmdEnabled;
}

EHMDWornState::Type FSteamVRHMD::GetHMDWornState()
{
	//HmdWornState is set in OnStartGameFrame's event loop
	return HmdWornState;
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
		GetWindowBounds(&X, &Y, &Width, &Height);

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

void FSteamVRHMD::RebaseObjectOrientationAndPosition(FVector& OutPosition, FQuat& OutOrientation) const
{
}

void FSteamVRHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
}

float FSteamVRHMD::GetInterpupillaryDistance() const
{
	return 0.064f;
}

void FSteamVRHMD::GetCurrentPose(FQuat& CurrentOrientation, FVector& CurrentPosition, uint32 DeviceId, EPoseRefreshMode RefreshMode/* = EPoseRefreshMode::None*/, float ForceRefreshWorldToMetersScale /* = 0.0f */ )
{
	if (VRSystem == nullptr)
	{
		return;
	}

	check(DeviceId >= 0 && DeviceId < vr::k_unMaxTrackedDeviceCount);

	FTrackingFrame& TrackingFrame = const_cast<FTrackingFrame&>(GetTrackingFrame());
	if (RefreshMode != EPoseRefreshMode::None)
	{
		TrackingFrame.FrameNumber = GFrameNumberRenderThread;

		vr::TrackedDevicePose_t Poses[vr::k_unMaxTrackedDeviceCount];
		if (RefreshMode == EPoseRefreshMode::RenderRefresh)
		{
			vr::EVRCompositorError PoseError = VRCompositor->WaitGetPoses(Poses, ARRAYSIZE(Poses) , NULL, 0);
		}
		else
		{
			check(RefreshMode == EPoseRefreshMode::GameRefresh);
			VRSystem->GetDeviceToAbsoluteTrackingPose(VRCompositor->GetTrackingSpace(), 0.0f, Poses, ARRAYSIZE(Poses));
		}

		bHaveVisionTracking = false;
		for (uint32 i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
		{
			TrackingFrame.bDeviceIsConnected[i] = Poses[i].bDeviceIsConnected;
			TrackingFrame.bPoseIsValid[i] = Poses[i].bPoseIsValid;

			bHaveVisionTracking |= Poses[i].eTrackingResult == vr::ETrackingResult::TrackingResult_Running_OK;

			FVector LocalCurrentPosition;
			FQuat LocalCurrentOrientation;
			PoseToOrientationAndPosition(Poses[i].mDeviceToAbsoluteTracking, ForceRefreshWorldToMetersScale, LocalCurrentOrientation, LocalCurrentPosition);

			TrackingFrame.WorldToMetersScale = ForceRefreshWorldToMetersScale;

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
	else
	{
		CurrentOrientation = FQuat::Identity;
		CurrentPosition = FVector::ZeroVector;
	}
}

void FSteamVRHMD::GetWindowBounds(int32* X, int32* Y, uint32* Width, uint32* Height)
{
	if (vr::IVRExtendedDisplay *VRExtDisplay = vr::VRExtendedDisplay())
	{
		VRExtDisplay->GetWindowBounds(X, Y, Width, Height);
	}
	else
	{
		*X = 0;
		*Y = 0;
		*Width = WindowMirrorBoundsWidth;
		*Height = WindowMirrorBoundsHeight;
	}
}

bool FSteamVRHMD::IsInsideBounds()
{
	if (VRChaperone)
	{
		const FTrackingFrame& TrackingFrame = GetTrackingFrame();
		vr::HmdMatrix34_t VRPose = TrackingFrame.RawPoses[vr::k_unTrackedDeviceIndex_Hmd];
		FMatrix Pose = ToFMatrix(VRPose);
		
		const FVector HMDLocation(Pose.M[3][0], 0.f, Pose.M[3][2]);

		bool bLastWasNegative = false;

		// Since the order of the soft bounds are points on a plane going clockwise, wind around the sides, checking the crossproduct of the affine side to the affine HMD position.  If they're all on the same side, we're in the bounds
		for (uint8 i = 0; i < 4; ++i)
		{
			const FVector PointA = ChaperoneBounds.Bounds.Corners[i];
			const FVector PointB = ChaperoneBounds.Bounds.Corners[(i + 1) % 4];

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

TArray<FVector> FSteamVRHMD::GetBounds() const
{
	return ConvertBoundsToUnrealSpace(ChaperoneBounds.Bounds, GetWorldToMetersScale());
}

void FSteamVRHMD::SetTrackingOrigin(EHMDTrackingOrigin::Type NewOrigin)
{
	if(VRCompositor)
	{
		vr::TrackingUniverseOrigin NewSteamOrigin;

		switch (NewOrigin)
		{
			case EHMDTrackingOrigin::Eye:
				NewSteamOrigin = vr::TrackingUniverseOrigin::TrackingUniverseSeated;
				break;
			case EHMDTrackingOrigin::Floor:
			default:
				NewSteamOrigin = vr::TrackingUniverseOrigin::TrackingUniverseStanding;
				break;
		}

		VRCompositor->SetTrackingSpace(NewSteamOrigin);
	}
}

EHMDTrackingOrigin::Type FSteamVRHMD::GetTrackingOrigin()
{
	if(VRCompositor)
	{
		const vr::TrackingUniverseOrigin CurrentOrigin = VRCompositor->GetTrackingSpace();

		switch(CurrentOrigin)
		{
		case vr::TrackingUniverseOrigin::TrackingUniverseSeated:
			return EHMDTrackingOrigin::Eye;
		case vr::TrackingUniverseOrigin::TrackingUniverseStanding:
		default:
			return EHMDTrackingOrigin::Floor;
		}
	}

	// By default, assume standing
	return EHMDTrackingOrigin::Floor;
}

void FSteamVRHMD::SetUnrealControllerIdAndHandToDeviceIdMap(int32 InUnrealControllerIdAndHandToDeviceIdMap[ MAX_STEAMVR_CONTROLLER_PAIRS ][ 2 ] )
{
	for( int32 UnrealControllerIndex = 0; UnrealControllerIndex < MAX_STEAMVR_CONTROLLER_PAIRS; ++UnrealControllerIndex )
	{
		for( int32 HandIndex = 0; HandIndex < 2; ++HandIndex )
		{
			UnrealControllerIdAndHandToDeviceIdMap[ UnrealControllerIndex ][ HandIndex ] = InUnrealControllerIdAndHandToDeviceIdMap[ UnrealControllerIndex ][ HandIndex ];
		}
	}
}

void FSteamVRHMD::PoseToOrientationAndPosition(const vr::HmdMatrix34_t& InPose, const float WorldToMetersScale, FQuat& OutOrientation, FVector& OutPosition) const
{
	FMatrix Pose = ToFMatrix(InPose);
	FQuat Orientation(Pose);
 
	OutOrientation.X = -Orientation.Z;
	OutOrientation.Y = Orientation.X;
	OutOrientation.Z = Orientation.Y;
 	OutOrientation.W = -Orientation.W;	

	FVector Position = ((FVector(-Pose.M[3][2], Pose.M[3][0], Pose.M[3][1]) - BaseOffset) * WorldToMetersScale);
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

float FSteamVRHMD::GetWorldToMetersScale() const
{
	const FTrackingFrame& TrackingFrame = GetTrackingFrame();
	return TrackingFrame.bPoseIsValid ? TrackingFrame.WorldToMetersScale : 100.0f;
}

ESteamVRTrackedDeviceType FSteamVRHMD::GetTrackedDeviceType(uint32 DeviceId) const
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


void FSteamVRHMD::GetTrackedDeviceIds(ESteamVRTrackedDeviceType DeviceType, TArray<int32>& TrackedIds)
{
	TrackedIds.Empty();

	const FTrackingFrame& TrackingFrame = GetTrackingFrame();
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
	bool bHasValidPose = false;

	const FTrackingFrame& TrackingFrame = GetTrackingFrame();
	if (DeviceId < vr::k_unMaxTrackedDeviceCount)
	{
		CurrentOrientation = TrackingFrame.DeviceOrientation[DeviceId];
		CurrentPosition = TrackingFrame.DevicePosition[DeviceId];

		bHasValidPose = TrackingFrame.bPoseIsValid[DeviceId] && TrackingFrame.bDeviceIsConnected[DeviceId];
	}

	return bHasValidPose;
}

ETrackingStatus FSteamVRHMD::GetControllerTrackingStatus(uint32 DeviceId) const
{
	ETrackingStatus TrackingStatus = ETrackingStatus::NotTracked;

	const FTrackingFrame& TrackingFrame = GetTrackingFrame();
	if (DeviceId < vr::k_unMaxTrackedDeviceCount && TrackingFrame.bPoseIsValid[DeviceId] && TrackingFrame.bDeviceIsConnected[DeviceId])
	{
		TrackingStatus = ETrackingStatus::Tracked;
	}

	return TrackingStatus;
}

bool FSteamVRHMD::GetControllerHandPositionAndOrientation( const int32 ControllerIndex, EControllerHand Hand, FVector& OutPosition, FQuat& OutOrientation )
{
	if ((ControllerIndex < 0) || (ControllerIndex >= MAX_STEAMVR_CONTROLLER_PAIRS) || Hand < EControllerHand::Left || Hand > EControllerHand::Right)
	{
		return false;
	}

	const int32 DeviceId = UnrealControllerIdAndHandToDeviceIdMap[ ControllerIndex ][ (int32)Hand ];
	return GetTrackedObjectOrientationAndPosition(DeviceId, OutOrientation, OutPosition);
}

ETrackingStatus FSteamVRHMD::GetControllerTrackingStatus(int32 ControllerIndex, EControllerHand DeviceHand) const
{
	if ((ControllerIndex < 0) || (ControllerIndex >= MAX_STEAMVR_CONTROLLER_PAIRS) || DeviceHand < EControllerHand::Left || DeviceHand > EControllerHand::Right)
	{
		return ETrackingStatus::NotTracked;
	}

	const int32 DeviceId = UnrealControllerIdAndHandToDeviceIdMap[ ControllerIndex ][ (int32)DeviceHand ];
	return GetControllerTrackingStatus(DeviceId);
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
	LastHmdPosition = CurHmdPosition;

	const FRotator DeltaRot = ViewRotation - PC->GetControlRotation();
	DeltaControlRotation = (DeltaControlRotation + DeltaRot).GetNormalized();

	// Pitch from other sources is never good, because there is an absolute up and down that must be respected to avoid motion sickness.
	// Same with roll.
	DeltaControlRotation.Pitch = 0;
	DeltaControlRotation.Roll = 0;
	DeltaControlOrientation = DeltaControlRotation.Quaternion();

	ViewRotation = FRotator(DeltaControlOrientation * CurHmdOrientation);
}

bool FSteamVRHMD::UpdatePlayerCamera(FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	GetCurrentPose(CurHmdOrientation, CurHmdPosition, vr::k_unTrackedDeviceIndex_Hmd, EPoseRefreshMode::GameRefresh, GWorld->GetWorldSettings()->WorldToMeters);
	LastHmdOrientation = CurHmdOrientation;
	LastHmdPosition = CurHmdPosition;

	if( !bImplicitHmdPosition && GEnableVREditorHacks )
	{
		DeltaControlOrientation = CurrentOrientation;
		DeltaControlRotation = DeltaControlOrientation.Rotator();
	}

	CurrentOrientation = CurHmdOrientation;
	CurrentPosition = CurHmdPosition;

	return true;
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
			return true;
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
		GEngine->bForceDisableFrameRateSmoothing = true;
		return true;
	}

	return false;
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

void FSteamVRHMD::OnEndPlay(FWorldContext& InWorldContext)
{
	EnableStereo(false);
}

bool FSteamVRHMD::OnStartGameFrame(FWorldContext& WorldContext)
{
	
	
	if (VRSystem == nullptr)
	{
		return false;
	}

	if (bStereoEnabled != bStereoDesired)
	{
		bStereoEnabled = EnableStereo(bStereoDesired);
	}

	FQuat Orientation;
	FVector Position;
	GetCurrentPose(Orientation, Position, vr::k_unTrackedDeviceIndex_Hmd, EPoseRefreshMode::GameRefresh, GWorld->GetWorldSettings()->WorldToMeters);

	float TimeDeltaSeconds = FApp::GetDeltaTime();

	// Poll SteamVR events
	vr::VREvent_t VREvent;
	while (VRSystem->PollNextEvent(&VREvent, sizeof(VREvent)))
	{
		switch (VREvent.eventType)
		{
		case vr::VREvent_Quit:
			bIsQuitting = true;
			break;
		case vr::VREvent_InputFocusCaptured:
			FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
			break;
		case vr::VREvent_InputFocusReleased:
			FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
			break;
		case vr::VREvent_TrackedDeviceUserInteractionStarted:
			// if the event was sent by the HMD
			if ((VREvent.trackedDeviceIndex == vr::k_unTrackedDeviceIndex_Hmd) )
			{
				// Save the position we are currently at and put us in the state where we could move to a worn state
				bShouldCheckHMDPosition = true;
				HMDStartLocation = Position;
			}
			break;
		case vr::VREvent_TrackedDeviceUserInteractionEnded:
			// if the event was sent by the HMD. 
			if ((VREvent.trackedDeviceIndex == vr::k_unTrackedDeviceIndex_Hmd)) 
			{
				// Don't check to see if we might be wearing the HMD anymore.
				bShouldCheckHMDPosition = false;
				// Don't change our state to "not worn" unless we are currently wearing it.
				if (HmdWornState == EHMDWornState::Worn)
				{
					HmdWornState = EHMDWornState::NotWorn;
				}
			}
			break;
		}
	}


	// SteamVR gives 5 seconds from VREvent_Quit till its process is killed
	if (bIsQuitting)
	{
		bShouldCheckHMDPosition = false;
		Shutdown();
		GEngine->HMDDevice.Reset();
		GEngine->StereoRenderingDevice.Reset();
	}

	// If the HMD is being interacted with, but we haven't decided the HMD is worn yet.  
	if (bShouldCheckHMDPosition)
	{
		if (FVector::Dist(HMDStartLocation, Position) > HMDWornMovementThreshold)
		{
			HmdWornState = EHMDWornState::Worn;
			bShouldCheckHMDPosition = false;
		}
	}


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
	const FTrackingFrame& TrackingFrame = GetTrackingFrame();

	FRotator ViewRotation;
	ViewRotation = FRotator(TrackingFrame.DeviceOrientation[vr::k_unTrackedDeviceIndex_Hmd]);
	ViewRotation.Pitch = 0;
	ViewRotation.Roll = 0;
	ViewRotation.Yaw += BaseOrientation.Rotator().Yaw;

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
	const FTrackingFrame& TrackingFrame = GetTrackingFrame();
	FMatrix Pose = ToFMatrix(TrackingFrame.RawPoses[vr::k_unTrackedDeviceIndex_Hmd]);
	BaseOffset = FVector(-Pose.M[3][2], Pose.M[3][0], Pose.M[3][1]);
}

void FSteamVRHMD::SetClippingPlanes(float NCP, float FCP)
{
}

void FSteamVRHMD::SetBaseRotation(const FRotator& BaseRot)
{
	BaseOrientation = BaseRot.Quaternion();
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

bool FSteamVRHMD::EnableStereo(bool bStereo)
{
	bStereoDesired = (IsHMDEnabled()) ? bStereo : false;

	// Set the viewport to match that of the HMD display
	FSceneViewport* SceneVP = FindSceneViewport();
	if (VRSystem && SceneVP)
	{
		TSharedPtr<SWindow> Window = SceneVP->FindWindow();
		if (Window.IsValid() && SceneVP->GetViewportWidget().IsValid())
		{
			int32 ResX = 2160;
			int32 ResY = 1200;

			MonitorInfo MonitorDesc;
			if (GetHMDMonitorInfo(MonitorDesc))
			{
				ResX = MonitorDesc.ResolutionX;
				ResY = MonitorDesc.ResolutionY;
			}
			FSystemResolution::RequestResolutionChange(ResX, ResY, EWindowMode::WindowedFullscreen);

			if( bStereo )
			{
				int32 PosX, PosY;
				uint32 Width, Height;
				GetWindowBounds( &PosX, &PosY, &Width, &Height );
				SceneVP->SetViewportSize( Width, Height );
			}
			else
			{
				FVector2D size = SceneVP->FindWindow()->GetSizeInScreen();
				SceneVP->SetViewportSize( size.X, size.Y );
				Window->SetViewportSizeDrivenByWindow( true );
			}

			bStereoEnabled = bStereoDesired;
		}
	}

	// Uncap fps to enable FPS higher than 62
	GEngine->bForceDisableFrameRateSmoothing = bStereo;
	
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

		if (!bImplicitHmdPosition)
		{
			const FTrackingFrame& TrackingFrame = GetTrackingFrame();
 			const FVector vHMDPosition = DeltaControlOrientation.RotateVector(TrackingFrame.DevicePosition[vr::k_unTrackedDeviceIndex_Hmd]);
			ViewLocation += vHMDPosition;
		}
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

void FSteamVRHMD::GetOrthoProjection(int32 RTWidth, int32 RTHeight, float OrthoDistance, FMatrix OrthoProjection[2]) const
{
	const float HudOffset = 50.0f;
	OrthoProjection[0] = FTranslationMatrix(FVector(HudOffset, 0.f, 0.f));
	OrthoProjection[1] = FTranslationMatrix(FVector(-HudOffset + RTWidth * .5, 0.f, 0.f));
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
	InViewFamily.bUseSeparateRenderTarget = true;
}

void FSteamVRHMD::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& View)
{
	check(IsInRenderingThread());

	const FTrackingFrame& TrackingFrame = GetTrackingFrame();

	// The last view location used to set the view will be in BaseHmdOrientation.  We need to calculate the delta from that, so that
	// cameras that rely on game objects (e.g. other components) for their positions don't need to be updated on the render thread.
	const FQuat DeltaOrient = View.BaseHmdOrientation.Inverse() * TrackingFrame.DeviceOrientation[vr::k_unTrackedDeviceIndex_Hmd];
	View.ViewRotation = FRotator(View.ViewRotation.Quaternion() * DeltaOrient);

	if (bImplicitHmdPosition)
	{
		const FQuat LocalDeltaControlOrientation =  View.ViewRotation.Quaternion() * TrackingFrame.DeviceOrientation[vr::k_unTrackedDeviceIndex_Hmd].Inverse();
		const FVector DeltaPosition = TrackingFrame.DevicePosition[vr::k_unTrackedDeviceIndex_Hmd] - View.BaseHmdLocation;
		View.ViewLocation += LocalDeltaControlOrientation.RotateVector(DeltaPosition);
	}

 	View.UpdateViewMatrix();
}

void FSteamVRHMD::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
{
	if (bIsQuitting)
	{
		GEngine->HMDDevice.Reset();
		GEngine->StereoRenderingDevice.Reset();
	}

	check(IsInRenderingThread());
	GetActiveRHIBridgeImpl()->BeginRendering();

	const FSceneView* MainView = ViewFamily.Views[0];
	const float WorldToMetersScale = MainView->WorldToMetersScale;

	const FTransform OldRelativeTransform(MainView->BaseHmdOrientation, MainView->BaseHmdLocation);

	FVector NewPosition;
	FQuat NewOrientation;
	GetCurrentPose(NewOrientation, NewPosition, vr::k_unTrackedDeviceIndex_Hmd, EPoseRefreshMode::RenderRefresh, WorldToMetersScale);
	const FTransform NewRelativeTransform(NewOrientation, NewPosition);

	ApplyLateUpdate(ViewFamily.Scene, OldRelativeTransform, NewRelativeTransform);
}

void FSteamVRHMD::UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& InViewport, SViewport* ViewportWidget)
{
	check(IsInGameThread());

	FRHIViewport* const ViewportRHI = InViewport.GetViewportRHI().GetReference();
	if (!ViewportRHI)
	{
		return;
	}

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
	HmdWornState(EHMDWornState::Unknown),
	bStereoDesired(false),
	bStereoEnabled(false),
	bHmdPosTracking(true),
	bHaveVisionTracking(false),
	IPD(0.064f),
	WindowMirrorMode(1),
	WindowMirrorBoundsWidth(2160),
	WindowMirrorBoundsHeight(1200),
	HMDWornMovementThreshold(50.0f),
	CurHmdOrientation(FQuat::Identity),
	LastHmdOrientation(FQuat::Identity),
	LastHmdPosition(FVector::ZeroVector),
	HMDStartLocation(FVector::ZeroVector),
	BaseOrientation(FQuat::Identity),
	BaseOffset(FVector::ZeroVector),
	DeltaControlRotation(FRotator::ZeroRotator),
	DeltaControlOrientation(FQuat::Identity),
	CurHmdPosition(FVector::ZeroVector),
	SteamVRPlugin(SteamVRPlugin),
	RendererModule(nullptr),
	bShouldCheckHMDPosition(false)
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
	// grab a pointer to the renderer module for displaying our mirror window
	static const FName RendererModuleName("Renderer");
	RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);

	vr::EVRInitError VRInitErr = vr::VRInitError_None;
	// Attempt to initialize the VRSystem device
	VRSystem = vr::VR_Init(&VRInitErr, vr::VRApplication_Scene);
	if (!VRSystem || (VRInitErr != vr::VRInitError_None))
	{
		UE_LOG(LogHMD, Log, TEXT("Failed to initialize OpenVR with code %d"), (int32)VRInitErr);
		return;
	}

	// Make sure that the version of the HMD we're compiled against is correct.  This will fill out the proper vtable!
	VRSystem = (vr::IVRSystem*)(*FSteamVRHMD::VRGetGenericInterfaceFn)(vr::IVRSystem_Version, &VRInitErr);
	if (!VRSystem || (VRInitErr != vr::VRInitError_None))
	{
		UE_LOG(LogHMD, Log, TEXT("Failed to initialize OpenVR (version mismatch) with code %d"), (int32)VRInitErr);
		return;
	}

	// attach to the compositor
	if ((VRSystem != nullptr) && (VRInitErr == vr::VRInitError_None))
	{
		//VRCompositor = (vr::IVRCompositor*)vr::VR_GetGenericInterface(vr::IVRCompositor_Version, &HmdErr);
		VRCompositor = (vr::IVRCompositor*)(*VRGetGenericInterfaceFn)(vr::IVRCompositor_Version, &VRInitErr);
		VROverlay = (vr::IVROverlay*)(*VRGetGenericInterfaceFn)(vr::IVROverlay_Version, &VRInitErr);
	}

	if ((VRSystem != nullptr) && (VRInitErr == vr::VRInitError_None))
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
		GetWindowBounds(&ScreenX, &ScreenY, &ScreenWidth, &ScreenHeight);

		float WidthPercentage = ((float)RecommendedWidth / (float)ScreenWidth) * 100.0f;
		float HeightPercentage = ((float)RecommendedHeight / (float)ScreenHeight) * 100.0f;

		float ScreenPercentage = FMath::Max(WidthPercentage, HeightPercentage);

		//@todo steamvr: move out of here
		static IConsoleVariable* CScrPercVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));

		if (FMath::RoundToInt(CScrPercVar->GetFloat()) != FMath::RoundToInt(ScreenPercentage))
		{
			CScrPercVar->Set(ScreenPercentage);
		}

		// disable vsync
		static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
		CVSyncVar->Set(false);

		// enforce finishcurrentframe
		static IConsoleVariable* CFCFVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.finishcurrentframe"));
		CFCFVar->Set(false);

		// Grab the chaperone
		vr::EVRInitError ChaperoneErr = vr::VRInitError_None;
		//VRChaperone = (vr::IVRChaperone*)vr::VR_GetGenericInterface(vr::IVRChaperone_Version, &ChaperoneErr);
		VRChaperone = (vr::IVRChaperone*)(*VRGetGenericInterfaceFn)(vr::IVRChaperone_Version, &ChaperoneErr);
		if ((VRChaperone != nullptr) && (ChaperoneErr == vr::VRInitError_None))
		{
			ChaperoneBounds = FChaperoneBounds(VRChaperone);
		}
		else
		{
			UE_LOG(LogHMD, Log, TEXT("Failed to initialize Chaperone.  Error: %d"), (int32)ChaperoneErr);
		}

		// Initialize our controller to device index
		for (int32 UnrealControllerIndex = 0; UnrealControllerIndex < MAX_STEAMVR_CONTROLLER_PAIRS; ++UnrealControllerIndex)
		{
			for (int32 HandIndex = 0; HandIndex < 2; ++HandIndex)
			{
				UnrealControllerIdAndHandToDeviceIdMap[UnrealControllerIndex][HandIndex] = INDEX_NONE;
			}
		}

		SetupOcclusionMeshes();

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
		UE_LOG(LogHMD, Log, TEXT("SteamVR failed to initialize.  Err: %d"), (int32)VRInitErr);

		VRSystem = nullptr;
	}
}

void FSteamVRHMD::LoadFromIni()
{
	const TCHAR* SteamVRSettings = TEXT("SteamVR.Settings");
	int32 i;

	if (GConfig->GetInt(SteamVRSettings, TEXT("WindowMirrorMode"), i, GEngineIni))
	{
		WindowMirrorMode = i;
	}

	if (GConfig->GetInt(SteamVRSettings, TEXT("WindowMirrorBoundsWidth"), i, GEngineIni))
	{
		WindowMirrorBoundsWidth = i;
	}

	if (GConfig->GetInt(SteamVRSettings, TEXT("WindowMirrorBoundsHeight"), i, GEngineIni))
	{
		WindowMirrorBoundsHeight = i;
	}

	float ConfigFloat = 0.0f;

	if (GConfig->GetFloat(SteamVRSettings, TEXT("HMDWornMovementThreshold"), ConfigFloat, GEngineIni))
	{
		HMDWornMovementThreshold = ConfigFloat;
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
		VRCompositor = nullptr;
		VROverlay = nullptr;
		VRChaperone = nullptr;

		vr::VR_Shutdown();

		SteamVRPlugin->Reset();
	}
}

void FSteamVRHMD::SetupOcclusionMeshes()
{	
	const vr::HiddenAreaMesh_t LeftEyeMesh = VRSystem->GetHiddenAreaMesh(vr::Hmd_Eye::Eye_Left);
	const vr::HiddenAreaMesh_t RightEyeMesh = VRSystem->GetHiddenAreaMesh(vr::Hmd_Eye::Eye_Right);
	
	const uint32 VertexCount = LeftEyeMesh.unTriangleCount * 3;
	check(LeftEyeMesh.unTriangleCount == RightEyeMesh.unTriangleCount);

	// Copy mesh data from SteamVR format to ours, then initialize the meshes.
	if (VertexCount > 0)
	{
		FVector2D* const LeftEyePositions = new FVector2D[VertexCount];
		FVector2D* const RightEyePositions = new FVector2D[VertexCount];

		uint32 HiddenAreaMeshCrc = 0;
		uint32 DataIndex = 0;
		for (uint32 TriangleIter = 0; TriangleIter < LeftEyeMesh.unTriangleCount; ++TriangleIter)
		{
			for (uint32 VertexIter = 0; VertexIter < 3; ++VertexIter)
			{
				const vr::HmdVector2_t& LeftSrc = LeftEyeMesh.pVertexData[DataIndex];
				const vr::HmdVector2_t& RightSrc = RightEyeMesh.pVertexData[DataIndex];

				FVector2D& LeftDst = LeftEyePositions[DataIndex];
				FVector2D& RightDst = RightEyePositions[DataIndex];

				LeftDst.X = LeftSrc.v[0];
				LeftDst.Y = LeftSrc.v[1];

				RightDst.X = RightSrc.v[0];
				RightDst.Y = RightSrc.v[1];

				HiddenAreaMeshCrc = FCrc::MemCrc32(&LeftDst, sizeof(FVector2D), HiddenAreaMeshCrc);

				++DataIndex;
			}
		}

		HiddenAreaMeshes[0].BuildMesh(LeftEyePositions, VertexCount, FHMDViewMesh::MT_HiddenArea);
		HiddenAreaMeshes[1].BuildMesh(RightEyePositions, VertexCount, FHMDViewMesh::MT_HiddenArea);

		// If the hidden area mesh from the SteamVR runtime matches the mesh used to generate the Vive's visible area mesh, initialize it.
		// The visible area mesh is a hand crafted inverse of the hidden area mesh we are getting from the steamvr runtime. Since the runtime data
		// may change, we need to sanity check it matches our hand crafted mesh before using it.
		if (HiddenAreaMeshCrc == ViveHiddenAreaMeshCrc)
		{
			VisibleAreaMeshes[0].BuildMesh(Vive_LeftEyeVisibleAreaPositions, VisibleAreaVertexCount, FHMDViewMesh::MT_VisibleArea);
			VisibleAreaMeshes[1].BuildMesh(Vive_RightEyeVisibleAreaPositions, VisibleAreaVertexCount, FHMDViewMesh::MT_VisibleArea);
		}

		delete[] LeftEyePositions;
		delete[] RightEyePositions;
	}
}

const FSteamVRHMD::FTrackingFrame& FSteamVRHMD::GetTrackingFrame() const
{
	if (IsInRenderingThread())
	{
		return RenderTrackingFrame;
	}
	else
	{
		return GameTrackingFrame;
	}
}

#endif //STEAMVR_SUPPORTED_PLATFORMS
