// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HMDPrivatePCH.h"
#include "OculusRiftHMD.h"
// Mac uses 0.5.0 SDK, while PC - 0.6.0. This version supports 0.5.0 (Mac/Linux).
#if PLATFORM_MAC
#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "SceneViewport.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#endif

//---------------------------------------------------
// Oculus Rift Plugin Implementation
//---------------------------------------------------

class FOculusRiftPlugin : public IOculusRiftPlugin
{
	/** IHeadMountedDisplayModule implementation */
	virtual TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > CreateHeadMountedDisplay() override;

	FString GetModulePriorityKeyName() const
	{
		return FString(TEXT("OculusRift"));
	}
};

IMPLEMENT_MODULE( FOculusRiftPlugin, OculusRift )

TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > FOculusRiftPlugin::CreateHeadMountedDisplay()
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	TSharedPtr< FOculusRiftHMD, ESPMode::ThreadSafe > OculusRiftHMD( new FOculusRiftHMD() );
	if( OculusRiftHMD->IsInitialized() )
	{
		return OculusRiftHMD;
	}
#endif//OCULUS_RIFT_SUPPORTED_PLATFORMS
	return NULL;
}

//---------------------------------------------------
// Oculus Rift IHeadMountedDisplay Implementation
//---------------------------------------------------

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#if !UE_BUILD_SHIPPING
//////////////////////////////////////////////////////////////////////////
static void OVR_CDECL OvrLogCallback(int level, const char* message)
{
	const TCHAR* tbuf = ANSI_TO_TCHAR(message);
	const TCHAR* levelStr = TEXT("");
	switch (level)
	{
	case ovrLogLevel_Debug: levelStr = TEXT(" Debug:"); break;
	case ovrLogLevel_Info:  levelStr = TEXT(" Info:"); break;
	case ovrLogLevel_Error: levelStr = TEXT(" Error:"); break;
	default:;
	}
	GLog->Logf(TEXT("OCULUS:%s %s"), levelStr, tbuf);
}
#endif

//////////////////////////////////////////////////////////////////////////
FSettings::FSettings()
{
#ifndef OVR_VISION_ENABLED
	Flags.bHmdPosTracking = false;
#endif
#ifndef OVR_SDK_RENDERING
	Flags.bTimeWarp = false;
#else
#endif
	FMemory::Memset(EyeRenderDesc, 0);
	FMemory::Memset(EyeProjectionMatrices, 0);
	FMemory::Memset(EyeFov, 0);

	SupportedTrackingCaps = SupportedDistortionCaps = SupportedHmdCaps = 0;
	TrackingCaps = DistortionCaps = HmdCaps = 0;

#ifndef OVR_SDK_RENDERING
	FMemory::Memset(UVScaleOffset, 0);
#endif

#ifndef OVR_SDK_RENDERING
	for (unsigned i = 0; i < sizeof(pDistortionMesh) / sizeof(pDistortionMesh[0]); ++i)
	{
		pDistortionMesh[i] = nullptr;
	}
#endif
}

TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> FSettings::Clone() const
{
	TSharedPtr<FSettings, ESPMode::ThreadSafe> NewSettings = MakeShareable(new FSettings(*this));
	return NewSettings;
}

void FSettings::SetEyeRenderViewport(int OneEyeVPw, int OneEyeVPh)
{
	FHMDSettings::SetEyeRenderViewport(OneEyeVPw, OneEyeVPh);
	EyeRenderViewport[0].Max.X -= GetTexturePaddingPerEye();
	EyeRenderViewport[1].Min.X += GetTexturePaddingPerEye();
}

//////////////////////////////////////////////////////////////////////////
FGameFrame::FGameFrame()
{
	FMemory::Memset(CurEyeRenderPose, 0);
	FMemory::Memset(CurTrackingState, 0);
	FMemory::Memset(EyeRenderPose, 0);
	FMemory::Memset(HeadPose, 0);
}

TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> FGameFrame::Clone() const
{
	TSharedPtr<FGameFrame, ESPMode::ThreadSafe> NewFrame = MakeShareable(new FGameFrame(*this));
	return NewFrame;
}

//////////////////////////////////////////////////////////////////////////

// a hack to allow quickly check if Oculus is in Direct mode
bool FOculusRiftHMD::bDirectModeHack = false;

TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> FOculusRiftHMD::CreateNewGameFrame() const
{
	TSharedPtr<FGameFrame, ESPMode::ThreadSafe> Result(MakeShareable(new FGameFrame()));
	return Result;
}

TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> FOculusRiftHMD::CreateNewSettings() const
{
	TSharedPtr<FSettings, ESPMode::ThreadSafe> Result(MakeShareable(new FSettings()));
	return Result;
}

bool FOculusRiftHMD::OnStartGameFrame()
{
	bool rv = FHeadMountedDisplay::OnStartGameFrame();
	if (!rv)
	{
		return false;
	}

	FGameFrame* CurrentFrame = GetFrame();

	// need to make a copy of settings here, since settings could change.
	CurrentFrame->Settings = Settings->Clone();
	FSettings* CurrentSettings = CurrentFrame->GetSettings();

	if (Hmd)
	{
		// Save eye and head poses
		const ovrVector3f hmdToEyeViewOffset[2] = { CurrentSettings->EyeRenderDesc[0].HmdToEyeViewOffset, CurrentSettings->EyeRenderDesc[1].HmdToEyeViewOffset };
		ovrHmd_GetEyePoses(Hmd, CurrentFrame->FrameNumber, hmdToEyeViewOffset, CurrentFrame->CurEyeRenderPose, &CurrentFrame->CurTrackingState);
#ifdef OVR_VISION_ENABLED
		if (CurrentSettings->Flags.bHmdPosTracking)
		{
			CurrentFrame->Flags.bHaveVisionTracking = (CurrentFrame->CurTrackingState.StatusFlags & ovrStatus_PositionTracked) != 0;
			if (CurrentFrame->Flags.bHaveVisionTracking && !Flags.bHadVisionTracking)
			{
				UE_LOG(LogHMD, Warning, TEXT("Vision Tracking Acquired"));
			}
			if (!CurrentFrame->Flags.bHaveVisionTracking && Flags.bHadVisionTracking)
			{
				UE_LOG(LogHMD, Warning, TEXT("Lost Vision Tracking"));
			}
			Flags.bHadVisionTracking = CurrentFrame->Flags.bHaveVisionTracking;
		}
#endif // OVR_VISION_ENABLED
#if !UE_BUILD_SHIPPING
		{ // used for debugging, do not remove
			FQuat CurHmdOrientation;
			FVector CurHmdPosition;
			GetCurrentPose(CurHmdOrientation, CurHmdPosition, false, false);
			//UE_LOG(LogHMD, Log, TEXT("BFPOSE: Pos %.3f %.3f %.3f, fr: %d"), CurHmdPosition.X, CurHmdPosition.Y, CurHmdPosition.Y,(int)CurrentFrame->FrameNumber);
			//UE_LOG(LogHMD, Log, TEXT("BFPOSE: Yaw %.3f Pitch %.3f Roll %.3f, fr: %d"), CurHmdOrientation.Rotator().Yaw, CurHmdOrientation.Rotator().Pitch, CurHmdOrientation.Rotator().Roll,(int)CurrentFrame->FrameNumber);
		}
#endif
	}
	return true;
}

bool FOculusRiftHMD::IsHMDConnected()
{
	if (Settings->Flags.bHMDEnabled)
	{
		InitDevice();
		return Hmd != nullptr;
	}
	return false;
}

FGameFrame* FOculusRiftHMD::GetFrame()
{
	return static_cast<FGameFrame*>(GetCurrentFrame());
}

const FGameFrame* FOculusRiftHMD::GetFrame() const
{
	return static_cast<FGameFrame*>(GetCurrentFrame());
}

EHMDDeviceType::Type FOculusRiftHMD::GetHMDDeviceType() const
{
	return EHMDDeviceType::DT_OculusRift;
}

bool FOculusRiftHMD::GetHMDMonitorInfo(MonitorInfo& MonitorDesc)
{
	if (IsInitialized())
	{
		InitDevice();
	}
	if (Hmd)
	{
		MonitorDesc.MonitorName = Hmd->DisplayDeviceName;
		MonitorDesc.MonitorId	= Hmd->DisplayId;
		MonitorDesc.DesktopX	= Hmd->WindowsPos.x;
		MonitorDesc.DesktopY	= Hmd->WindowsPos.y;
		MonitorDesc.ResolutionX = Hmd->Resolution.w;
		MonitorDesc.ResolutionY = Hmd->Resolution.h;
		MonitorDesc.WindowSizeX = Settings->MirrorWindowSize.X;
		MonitorDesc.WindowSizeY = Settings->MirrorWindowSize.Y;
		return true;
	}
	else
	{
		MonitorDesc.MonitorName = "";
		MonitorDesc.MonitorId = 0;
		MonitorDesc.DesktopX = MonitorDesc.DesktopY = MonitorDesc.ResolutionX = MonitorDesc.ResolutionY = 0;
		MonitorDesc.WindowSizeX = MonitorDesc.WindowSizeY = 0;
	}
	return false;
}

bool FOculusRiftHMD::IsFullscreenAllowed()
{
	if (!Hmd)
	{
		InitDevice();
	}
	return ((Hmd && (Hmd->HmdCaps & ovrHmdCap_ExtendDesktop) != 0) || !Hmd) ? true : false;
}

bool FOculusRiftHMD::DoesSupportPositionalTracking() const
{
#ifdef OVR_VISION_ENABLED
	const FGameFrame* frame = GetFrame();
	const FSettings* OculusSettings = frame->GetSettings();
	return (frame && OculusSettings->Flags.bHmdPosTracking && (OculusSettings->SupportedTrackingCaps & ovrTrackingCap_Position) != 0);
#else
	return false;
#endif //OVR_VISION_ENABLED
}

bool FOculusRiftHMD::HasValidTrackingPosition()
{
#ifdef OVR_VISION_ENABLED
	const auto frame = GetFrame();
	return (frame && frame->Settings->Flags.bHmdPosTracking && frame->Flags.bHaveVisionTracking);
#else
	return false;
#endif //OVR_VISION_ENABLED
}

#define TRACKER_FOCAL_DISTANCE			1.00f // meters (focal point to origin for position)

void FOculusRiftHMD::GetPositionalTrackingCameraProperties(FVector& OutOrigin, FQuat& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const
{
	const auto frame = GetFrame();
	if (!frame)
	{
		return;
	}
	OutOrigin = FVector::ZeroVector;
	OutOrientation = FQuat::Identity;
	OutHFOV = OutVFOV = OutCameraDistance = OutNearPlane = OutFarPlane = 0;

	if (!Hmd)
	{
		return;
	}

	check(frame->WorldToMetersScale >= 0);
	OutCameraDistance = TRACKER_FOCAL_DISTANCE * frame->WorldToMetersScale;
	OutHFOV = FMath::RadiansToDegrees(Hmd->CameraFrustumHFovInRadians);
	OutVFOV = FMath::RadiansToDegrees(Hmd->CameraFrustumVFovInRadians);
	OutNearPlane = Hmd->CameraFrustumNearZInMeters * frame->WorldToMetersScale;
	OutFarPlane = Hmd->CameraFrustumFarZInMeters * frame->WorldToMetersScale;

	// Read the camera pose
	if (!(frame->CurTrackingState.StatusFlags & ovrStatus_CameraPoseTracked))
	{
		return;
	}
	const ovrPosef& cameraPose = frame->CurTrackingState.CameraPose;

	FQuat Orient;
	FVector Pos;
	frame->PoseToOrientationAndPosition(cameraPose, Orient, Pos);

	OutOrientation = Orient;
	OutOrigin = Pos;
}

bool FOculusRiftHMD::IsInLowPersistenceMode() const
{
	const auto frame = GetFrame();
	const auto FrameSettings = frame->GetSettings();
	return (frame && FrameSettings->Flags.bLowPersistenceMode && (FrameSettings->SupportedHmdCaps & ovrHmdCap_LowPersistence) != 0);
}

void FOculusRiftHMD::EnableLowPersistenceMode(bool Enable)
{
	Settings->Flags.bLowPersistenceMode = Enable;
	Flags.bNeedUpdateHmdCaps = true;
}

void FGameFrame::PoseToOrientationAndPosition(const ovrPosef& InPose, FQuat& OutOrientation, FVector& OutPosition) const
{
	OutOrientation = ToFQuat(InPose.Orientation);

	check(WorldToMetersScale >= 0);
	// correct position according to BaseOrientation and BaseOffset. 
	const FVector Pos = (ToFVector_M2U(OVR::Vector3f(InPose.Position), WorldToMetersScale) - (Settings->BaseOffset * WorldToMetersScale)) * CameraScale3D;
	OutPosition = Settings->BaseOrientation.Inverse().RotateVector(Pos);

	// apply base orientation correction to OutOrientation
	OutOrientation = Settings->BaseOrientation.Inverse() * OutOrientation;
	OutOrientation.Normalize();
}

void FOculusRiftHMD::GetCurrentHMDPose(FQuat& CurrentOrientation, FVector& CurrentPosition,
	bool bUseOrienationForPlayerCamera, bool bUsePositionForPlayerCamera, const FVector& PositionScale)
{
	// only supposed to be used from the game thread
	check(IsInGameThread());
	auto frame = GetFrame();
	if (!frame)
	{
		CurrentOrientation = FQuat::Identity;
		CurrentPosition = FVector::ZeroVector;
		return;
	}
	if (PositionScale != FVector::ZeroVector)
	{
		frame->CameraScale3D = PositionScale;
		frame->Flags.bCameraScale3DAlreadySet = true;
	}
	GetCurrentPose(CurrentOrientation, CurrentPosition, bUseOrienationForPlayerCamera, bUsePositionForPlayerCamera);
	if (bUseOrienationForPlayerCamera)
	{
		frame->LastHmdOrientation = CurrentOrientation;
		frame->Flags.bOrientationChanged = bUseOrienationForPlayerCamera;
	}
	if (bUsePositionForPlayerCamera)
	{
		frame->LastHmdPosition = CurrentPosition;
		frame->Flags.bPositionChanged = bUsePositionForPlayerCamera;
	}
}

void FOculusRiftHMD::GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	GetCurrentHMDPose(CurrentOrientation, CurrentPosition, false, false, FVector::ZeroVector);
}

FVector FOculusRiftHMD::GetNeckPosition(const FQuat& CurrentOrientation, const FVector& CurrentPosition, const FVector& PositionScale)
{
	const auto frame = GetFrame();
	if (!frame)
	{
		return FVector::ZeroVector;
	}
	FVector UnrotatedPos = CurrentOrientation.Inverse().RotateVector(CurrentPosition);
	UnrotatedPos.X -= frame->Settings->NeckToEyeInMeters.X * frame->WorldToMetersScale;
	UnrotatedPos.Z -= frame->Settings->NeckToEyeInMeters.Y * frame->WorldToMetersScale;
	return UnrotatedPos;
}

void FOculusRiftHMD::GetCurrentPose(FQuat& CurrentHmdOrientation, FVector& CurrentHmdPosition, bool bUseOrienationForPlayerCamera, bool bUsePositionForPlayerCamera)
{
	check(IsInGameThread());

	auto frame = GetFrame();
	check(frame);

	if (bUseOrienationForPlayerCamera || bUsePositionForPlayerCamera)
	{
		// if this pose is going to be used for camera update then save it.
		// This matters only if bUpdateOnRT is OFF.
		frame->EyeRenderPose[0] = frame->CurEyeRenderPose[0];
		frame->EyeRenderPose[1] = frame->CurEyeRenderPose[1];
		frame->HeadPose = frame->CurTrackingState.HeadPose.ThePose;
	}

	frame->PoseToOrientationAndPosition(frame->CurTrackingState.HeadPose.ThePose, CurrentHmdOrientation, CurrentHmdPosition);
	//UE_LOG(LogHMD, Log, TEXT("CRHDPS: Pos %.3f %.3f %.3f"), frame->CurTrackingState.HeadPose.ThePose.Position.x, frame->CurTrackingState.HeadPose.ThePose.Position.y, frame->CurTrackingState.HeadPose.ThePose.Position.z);
	//UE_LOG(LogHMD, Log, TEXT("CRPOSE: Pos %.3f %.3f %.3f"), CurrentHmdPosition.X, CurrentHmdPosition.Y, CurrentHmdPosition.Z);
	//UE_LOG(LogHMD, Log, TEXT("CRPOSE: Yaw %.3f Pitch %.3f Roll %.3f"), CurrentHmdOrientation.Rotator().Yaw, CurrentHmdOrientation.Rotator().Pitch, CurrentHmdOrientation.Rotator().Roll);
}

TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> FOculusRiftHMD::GetViewExtension()
{
 	TSharedPtr<FViewExtension, ESPMode::ThreadSafe> Result(MakeShareable(new FViewExtension(this)));
 	return Result;
}

void FOculusRiftHMD::ResetStereoRenderingParams()
{
	FHeadMountedDisplay::ResetStereoRenderingParams();
	Settings->InterpupillaryDistance = ovrHmd_GetFloat(Hmd, OVR_KEY_IPD, OVR_DEFAULT_IPD);
}

bool FOculusRiftHMD::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (FHeadMountedDisplay::Exec(InWorld, Cmd, Ar))
	{
		return true;
	}

	auto frame = GetFrame();

	if (FParse::Command(&Cmd, TEXT("HMD")))
	{
		if (FParse::Command(&Cmd, TEXT("LP"))) // low persistence mode
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (!CmdName.IsEmpty())
			{
				if (!FCString::Stricmp(*CmdName, TEXT("ON")))
				{
					Settings->Flags.bLowPersistenceMode = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Settings->Flags.bLowPersistenceMode = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE")))
				{
					Settings->Flags.bLowPersistenceMode = !Settings->Flags.bLowPersistenceMode;
				}
				else
				{
					return false;
				}
			}
			else
			{
				Settings->Flags.bLowPersistenceMode = !Settings->Flags.bLowPersistenceMode;
			}
			Flags.bNeedUpdateHmdCaps = true;
			Ar.Logf(TEXT("Low Persistence is currently %s"), (Settings->Flags.bLowPersistenceMode) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("MIRROR"))) // to mirror or not to mirror?...
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (!CmdName.IsEmpty())
			{
				if (!FCString::Stricmp(*CmdName, TEXT("ON")))
				{
					Settings->Flags.bMirrorToWindow = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Settings->Flags.bMirrorToWindow = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE"))) 
				{
					Settings->Flags.bMirrorToWindow = !Settings->Flags.bMirrorToWindow;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("RESET")))
				{
					Settings->Flags.bMirrorToWindow = true;
					Settings->MirrorWindowSize.X = Settings->MirrorWindowSize.Y = 0;
				}
				else
				{
					int32 X = FCString::Atoi(*CmdName);
					const TCHAR* CmdTemp = FCString::Strchr(*CmdName, 'x') ? FCString::Strchr(*CmdName, 'x') + 1 : FCString::Strchr(*CmdName, 'X') ? FCString::Strchr(*CmdName, 'X') + 1 : TEXT("");
					int32 Y = FCString::Atoi(CmdTemp);

					Settings->MirrorWindowSize.X = X;
					Settings->MirrorWindowSize.Y = Y;
				}
			}
			else
			{
				Settings->Flags.bMirrorToWindow = !Settings->Flags.bMirrorToWindow;
			}
			Flags.bNeedUpdateHmdCaps = true;
			Ar.Logf(TEXT("Mirroring is currently %s"), (Settings->Flags.bMirrorToWindow) ? TEXT("ON") : TEXT("OFF"));
			if (Settings->Flags.bMirrorToWindow && (Settings->MirrorWindowSize.X != 0 || Settings->MirrorWindowSize.Y != 0))
			{
				Ar.Logf(TEXT("Mirror window size is %d x %d"), Settings->MirrorWindowSize.X, Settings->MirrorWindowSize.Y);
			}
			return true;
		}
#ifdef OVR_SDK_RENDERING
		else if (FParse::Command(&Cmd, TEXT("TIMEWARP"))) 
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (!CmdName.IsEmpty())
			{
				if (!FCString::Stricmp(*CmdName, TEXT("ON")))
				{
					Settings->Flags.bTimeWarp = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Settings->Flags.bTimeWarp = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE")))
				{
					Settings->Flags.bTimeWarp = !Settings->Flags.bTimeWarp;
				}
				else
				{
					return false;
				}
			}
			else
			{
				Settings->Flags.bTimeWarp = !Settings->Flags.bTimeWarp;
			}
			Flags.bNeedUpdateDistortionCaps = true;
			Ar.Logf(TEXT("TimeWarp is currently %s"), (Settings->Flags.bTimeWarp) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
#endif // #ifdef OVR_SDK_RENDERING
#if !UE_BUILD_SHIPPING
		else if (FParse::Command(&Cmd, TEXT("STATS"))) // status / statistics
		{
			Settings->Flags.bShowStats = !Settings->Flags.bShowStats;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("GRID"))) // grid
		{
			Settings->Flags.bDrawGrid = !Settings->Flags.bDrawGrid;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("PROFILE"))) // profile
		{
			Settings->Flags.bProfiling = !Settings->Flags.bProfiling;
			Flags.bNeedUpdateDistortionCaps = true;
			Ar.Logf(TEXT("Profiling mode is currently %s"), (Settings->Flags.bProfiling) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
#endif //UE_BUILD_SHIPPING
	}
	else if (FParse::Command(&Cmd, TEXT("HMDMAG")))
	{
		if (FParse::Command(&Cmd, TEXT("ON")))
		{
			Settings->Flags.bYawDriftCorrectionEnabled = true;
			Flags.bNeedUpdateHmdCaps = true;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("OFF")))
		{
			Settings->Flags.bYawDriftCorrectionEnabled = false;
			Flags.bNeedUpdateHmdCaps = true;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("SHOW")))
		{
			Ar.Logf(TEXT("mag %s"), Settings->Flags.bYawDriftCorrectionEnabled ?
				TEXT("on") : TEXT("off"));
			return true;
		}
		return false;
	}
	else if (FParse::Command(&Cmd, TEXT("HMDWARP")))
    {
#ifndef OVR_SDK_RENDERING
        if (FParse::Command( &Cmd, TEXT("ON") ))
        {
			Settings->Flags.bHmdDistortion = true;
            return true;
        }
        else if (FParse::Command( &Cmd, TEXT("OFF") ))
        {
			Settings->Flags.bHmdDistortion = false;
            return true;
        }
#endif //OVR_SDK_RENDERING
		if (FParse::Command(&Cmd, TEXT("CHA")))
		{
			Settings->Flags.bChromaAbCorrectionEnabled = true;
			Flags.bNeedUpdateDistortionCaps = true;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("NOCHA")))
		{
			Settings->Flags.bChromaAbCorrectionEnabled = false;
			Flags.bNeedUpdateDistortionCaps = true;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("SHOW")))
		{
			Ar.Logf(TEXT("hmdwarp %s sc=%f %s"), (Settings->Flags.bHmdDistortion ? TEXT("on") : TEXT("off"))
				, Settings->IdealScreenPercentage / 100.f
				, (Settings->Flags.bChromaAbCorrectionEnabled ? TEXT("cha") : TEXT("nocha")));
		}
		return true;
    }
	else if (FParse::Command(&Cmd, TEXT("OVRVERSION")))
	{
		// deprecated. Use 'hmdversion' instead
		Ar.Logf(*GetVersionString());
		return true;
	}

	return false;
}

FString FOculusRiftHMD::GetVersionString() const
{
	const char* Results = ovr_GetVersionString();
	FString s = FString::Printf(TEXT("%s, LibOVR: %s, built %s, %s"), *GEngineVersion.ToString(), UTF8_TO_TCHAR(Results),
		UTF8_TO_TCHAR(__DATE__), UTF8_TO_TCHAR(__TIME__));
	return s;
}

void FOculusRiftHMD::RecordAnalytics()
{
	if (FEngineAnalytics::IsAvailable())
	{
		// prepare and send analytics data
		TArray<FAnalyticsEventAttribute> EventAttributes;

		FHeadMountedDisplay::MonitorInfo MonitorInfo;
		GetHMDMonitorInfo(MonitorInfo);
		if (Hmd)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DeviceName"), FString::Printf(TEXT("%s - %s"), ANSI_TO_TCHAR(Hmd->Manufacturer), ANSI_TO_TCHAR(Hmd->ProductName))));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DisplayDeviceName"), MonitorInfo.MonitorName));
#if PLATFORM_MAC // On OS X MonitorId is the CGDirectDisplayID aka uint64, not a string
		FString DisplayId(FString::Printf(TEXT("%llu"), MonitorInfo.MonitorId));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DisplayId"), DisplayId));
#else
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DisplayId"), MonitorInfo.MonitorId));
#endif
		FString MonResolution(FString::Printf(TEXT("(%d, %d)"), MonitorInfo.ResolutionX, MonitorInfo.ResolutionY));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Resolution"), MonResolution));

		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ChromaAbCorrectionEnabled"), Settings->Flags.bChromaAbCorrectionEnabled));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("MagEnabled"), Settings->Flags.bYawDriftCorrectionEnabled));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DevSettingsEnabled"), Settings->Flags.bDevSettingsEnabled));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("OverrideInterpupillaryDistance"), Settings->Flags.bOverrideIPD));
		if (Settings->Flags.bOverrideIPD)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("InterpupillaryDistance"), GetInterpupillaryDistance()));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("OverrideStereo"), Settings->Flags.bOverrideStereo));
		if (Settings->Flags.bOverrideStereo)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("HFOV"), Settings->HFOVInRadians));
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("VFOV"), Settings->VFOVInRadians));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("OverrideVSync"), Settings->Flags.bOverrideVSync));
		if (Settings->Flags.bOverrideVSync)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("VSync"), Settings->Flags.bVSync));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("OverrideScreenPercentage"), Settings->Flags.bOverrideScreenPercentage));
		if (Settings->Flags.bOverrideScreenPercentage)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ScreenPercentage"), Settings->ScreenPercentage));
		}
		if (Settings->Flags.bWorldToMetersOverride)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("WorldToMetersScale"), Settings->WorldToMetersScale));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("InterpupillaryDistance"), Settings->InterpupillaryDistance));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("TimeWarp"), Settings->Flags.bTimeWarp));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("AllowFinishCurrentFrame"), Settings->Flags.bAllowFinishCurrentFrame));
#ifdef OVR_VISION_ENABLED
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("HmdPosTracking"), Settings->Flags.bHmdPosTracking));
#endif
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("LowPersistenceMode"), Settings->Flags.bLowPersistenceMode));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("UpdateOnRT"), Settings->Flags.bUpdateOnRT));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("MirrorToWindow"), Settings->Flags.bMirrorToWindow));

		FString OutStr(TEXT("Editor.VR.DeviceInitialised"));
		FEngineAnalytics::GetProvider().RecordEvent(OutStr, EventAttributes);
	}
}

class FSceneViewport* FOculusRiftHMD::FindSceneViewport()
{
	if (!GIsEditor)
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		return GameEngine->SceneViewport.Get();
	}
#if WITH_EDITOR
	else
	{
		UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
		return (FSceneViewport*)(EditorEngine->GetPIEViewport());
	}
#endif
	return nullptr;
}

//---------------------------------------------------
// Oculus Rift IStereoRendering Implementation
//---------------------------------------------------
bool FOculusRiftHMD::DoEnableStereo(bool bStereo, bool bApplyToHmd)
{
	check(IsInGameThread());

	FSceneViewport* SceneVP = FindSceneViewport();
	if (bStereo && (!SceneVP || !SceneVP->IsStereoRenderingAllowed()))
	{
		return false;
	}

	bool stereoToBeEnabled = (Settings->Flags.bHMDEnabled) ? bStereo : false;

	if ((Settings->Flags.bStereoEnabled && stereoToBeEnabled) || (!Settings->Flags.bStereoEnabled && !stereoToBeEnabled))
	{
		// already in the desired mode
		return Settings->Flags.bStereoEnabled;
	}

	TSharedPtr<SWindow> Window;
	if (SceneVP)
	{
		Window = SceneVP->FindWindow();
	}

	if (stereoToBeEnabled)
	{
		// check if we already have a window; if not, queue enable stereo to the next frames and exit
		if (!Window.IsValid())
		{
			Flags.bNeedEnableStereo = true;
			Flags.bEnableStereoToHmd = bApplyToHmd || !IsFullscreenAllowed();
			return Settings->Flags.bStereoEnabled;
		}
	}

	bool wasFullscreenAllowed = IsFullscreenAllowed();
	if (OnOculusStateChange(stereoToBeEnabled))
	{
		Settings->Flags.bStereoEnabled = stereoToBeEnabled;

		if (SceneVP && SceneVP->GetViewportWidget().IsValid())
		{
			if (!IsFullscreenAllowed() && stereoToBeEnabled)
			{
				if (Hmd)
				{
					// keep window size, but set viewport size to Rift resolution
					SceneVP->SetViewportSize(Hmd->Resolution.w, Hmd->Resolution.h);
				}
			}
			else if (!wasFullscreenAllowed && !stereoToBeEnabled)
			{
				// restoring original viewport size (to be equal to window size).
				if (Window.IsValid())
				{
					FVector2D size = Window->GetSizeInScreen();
					SceneVP->SetViewportSize(size.X, size.Y);
					Window->SetViewportSizeDrivenByWindow(true);
				}
			}

			if (SceneVP)
			{
				if (Window.IsValid())
				{
					if (bApplyToHmd && IsFullscreenAllowed())
					{
						{
							FVector2D size = Window->GetSizeInScreen();
							SceneVP->SetViewportSize(size.X, size.Y);
							Window->SetViewportSizeDrivenByWindow(true);
						}

						if (stereoToBeEnabled)
						{
							EWindowMode::Type wm = (!GIsEditor) ? EWindowMode::Fullscreen : EWindowMode::WindowedFullscreen;
							FVector2D size = Window->GetSizeInScreen();
							SceneVP->ResizeFrame(size.X, size.Y, wm, 0, 0);
						}
						else
						{
							// In Editor we cannot use ResizeFrame trick since it is called too late and App::IsGame
							// returns false.
							if (GIsEditor)
							{
								FSlateRect PreFullScreenRect;
								PopPreFullScreenRect(PreFullScreenRect);
								if (PreFullScreenRect.GetSize().X > 0 && PreFullScreenRect.GetSize().Y > 0 && IsFullscreenAllowed())
								{
									Window->MoveWindowTo(FVector2D(PreFullScreenRect.Left, PreFullScreenRect.Top));
								}
							}
							else
							{
								FVector2D size = Window->GetSizeInScreen();
								SceneVP->ResizeFrame(size.X, size.Y, EWindowMode::Windowed, 0, 0);
							}
						}
					}
					else if (!IsFullscreenAllowed())
					{
						// a special case when 'stereo on' or 'stereo hmd' is used in Direct mode. We must set proper window mode, otherwise
						// it will be lost once window loses and regains the focus.
						FVector2D size = Window->GetSizeInScreen();
						FSystemResolution::RequestResolutionChange(size.X, size.Y, (stereoToBeEnabled) ? EWindowMode::WindowedMirror : EWindowMode::Windowed);
					}
				}
			}
		}
	}
	return Settings->Flags.bStereoEnabled;
}

bool FOculusRiftHMD::OnOculusStateChange(bool bIsEnabledNow)
{
	Settings->Flags.bHmdDistortion = bIsEnabledNow;
	if (!bIsEnabledNow)
	{
		// Switching from stereo
		ReleaseDevice();

		ResetControlRotation();
		RestoreSystemValues();
		return true;
	}
	else
	{
		// Switching to stereo
		InitDevice();

		if (Hmd)
		{
			SaveSystemValues();
			Flags.bApplySystemOverridesOnStereo = true;

			UpdateStereoRenderingParams();
			return true;
		}
		DeltaControlRotation = FRotator::ZeroRotator;
	}
	return false;
}

void FOculusRiftHMD::ApplySystemOverridesOnStereo(bool bForce)
{
	if (Settings->Flags.bStereoEnabled || bForce)
	{
		// Set the current VSync state
		if (Settings->Flags.bOverrideVSync)
		{
			static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
			CVSyncVar->Set(Settings->Flags.bVSync != 0);
		}
		else
		{
			static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
			Settings->Flags.bVSync = CVSyncVar->GetInt() != 0;
		}
		UpdateHmdCaps();

#ifndef OVR_SDK_RENDERING
		static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
		CFinishFrameVar->Set(Settings->Flags.bAllowFinishCurrentFrame != 0);
#endif
	}
}

void FOculusRiftHMD::SaveSystemValues()
{
	static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	Settings->Flags.bSavedVSync = CVSyncVar->GetInt() != 0;
}

void FOculusRiftHMD::RestoreSystemValues()
{
	static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	// todo: bad - cvars are a user wish, this should be changed
	CVSyncVar->Set(Settings->Flags.bSavedVSync != 0);

	static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
	// todo: bad - cvars are a user wish, this should be changed
	CFinishFrameVar->Set(false);
}

void FOculusRiftHMD::UpdatePostProcessSettings(FPostProcessSettings* Settings)
{
	const auto frame = GetFrame();
	if (frame && frame->Flags.bScreenPercentageEnabled)
	{
		Settings->ScreenPercentage = GetActualScreenPercentage();
	}
}

void FOculusRiftHMD::CalculateStereoViewOffset(const EStereoscopicPass StereoPassType, const FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
	check(WorldToMeters != 0.f);

	const int idx = (StereoPassType == eSSP_LEFT_EYE) ? 0 : 1;

	if (IsInGameThread())
	{
		const auto frame = GetFrame();
		if (!frame)
		{
			return;
		}

		// This method is called from GetProjectionData on a game thread.
		// The modified ViewLocation is used ONLY for ViewMatrix composition, it is not
		// stored modified in the ViewInfo. ViewInfo.ViewLocation remains unmodified.

		if (StereoPassType != eSSP_FULL || frame->Settings->Flags.bHeadTrackingEnforced)
		{
			if (!frame->Flags.bOrientationChanged)
			{
				UE_LOG(LogHMD, Log, TEXT("Orientation wasn't applied to a camera in frame %d"), GFrameCounter);
			}

			FVector CurEyePosition;
			FQuat CurEyeOrient;
			frame->PoseToOrientationAndPosition(frame->EyeRenderPose[idx], CurEyeOrient, CurEyePosition);

			FVector HeadPosition = FVector::ZeroVector;
			// If we use PlayerController->bFollowHmd then we must apply full EyePosition (HeadPosition == 0).
			// Otherwise, we will apply only a difference between EyePosition and HeadPosition, since
			// HeadPosition is supposedly already applied.
			if (!frame->Flags.bPlayerControllerFollowsHmd)
			{
				FQuat HeadOrient;
				frame->PoseToOrientationAndPosition(frame->HeadPose, HeadOrient, HeadPosition);
			}

			// apply stereo disparity to ViewLocation. Note, ViewLocation already contains HeadPose.Position, thus
			// we just need to apply delta between EyeRenderPose.Position and the HeadPose.Position. 
			// EyeRenderPose and HeadPose are captured by the same call to GetEyePoses.
			const FVector HmdToEyeOffset = CurEyePosition - HeadPosition;

			// Calculate the difference between the final ViewRotation and EyeOrientation:
			// we need to rotate the HmdToEyeOffset by this differential quaternion.
			// When bPlayerControllerFollowsHmd == true, the DeltaControlOrientation already contains
			// the proper value (see ApplyHmdRotation)
			//FRotator r = ViewRotation - CurEyeOrient.Rotator();
			const FQuat ViewOrient = ViewRotation.Quaternion();
			const FQuat DeltaControlOrientation =  ViewOrient * CurEyeOrient.Inverse();

			//UE_LOG(LogHMD, Log, TEXT("EYEROT: Yaw %.3f Pitch %.3f Roll %.3f"), CurEyeOrient.Rotator().Yaw, CurEyeOrient.Rotator().Pitch, CurEyeOrient.Rotator().Roll);
			//UE_LOG(LogHMD, Log, TEXT("VIEROT: Yaw %.3f Pitch %.3f Roll %.3f"), ViewRotation.Yaw, ViewRotation.Pitch, ViewRotation.Roll);
			//UE_LOG(LogHMD, Log, TEXT("DLTROT: Yaw %.3f Pitch %.3f Roll %.3f"), DeltaControlOrientation.Rotator().Yaw, DeltaControlOrientation.Rotator().Pitch, DeltaControlOrientation.Rotator().Roll);

			// The HMDPosition already has HMD orientation applied.
			// Apply rotational difference between HMD orientation and ViewRotation
			// to HMDPosition vector. 
			const FVector vEyePosition = DeltaControlOrientation.RotateVector(HmdToEyeOffset);
			ViewLocation += vEyePosition;
			
			//UE_LOG(LogHMD, Log, TEXT("DLTPOS: %.3f %.3f %.3f"), vEyePosition.X, vEyePosition.Y, vEyePosition.Z);
		}
	}
}

void FOculusRiftHMD::ResetOrientationAndPosition(float yaw)
{
	ResetOrientation(yaw);
	ResetPosition();
}

void FOculusRiftHMD::ResetOrientation(float yaw)
{
	const auto frame = GetFrame();
	OVR::Quatf orientation;
	if (frame)
	{
		orientation = OVR::Quatf(frame->CurTrackingState.HeadPose.ThePose.Orientation);
	}
	else
	{
		const ovrTrackingState ss = ovrHmd_GetTrackingState(Hmd, ovr_GetTimeInSeconds());
		const ovrPosef& pose = ss.HeadPose.ThePose;
		orientation = OVR::Quatf(pose.Orientation);
	}

	FRotator ViewRotation;
	ViewRotation = FRotator(ToFQuat(orientation));
	ViewRotation.Pitch = 0;
	ViewRotation.Roll = 0;

	if (yaw != 0.f)
	{
		// apply optional yaw offset
		ViewRotation.Yaw -= yaw;
		ViewRotation.Normalize();
	}

	Settings->BaseOrientation = ViewRotation.Quaternion();
}

void FOculusRiftHMD::ResetPosition()
{
	// Reset position
	auto frame = GetFrame();
	OVR::Quatf orientation;
	if (frame)
	{
		orientation = OVR::Quatf(frame->CurTrackingState.HeadPose.ThePose.Orientation);
		Settings->BaseOffset = ToFVector(frame->CurTrackingState.HeadPose.ThePose.Position);
	}
	else
	{
		const ovrTrackingState ss = ovrHmd_GetTrackingState(Hmd, ovr_GetTimeInSeconds());
		const ovrPosef& pose = ss.HeadPose.ThePose;
		orientation = OVR::Quatf(pose.Orientation);
		Settings->BaseOffset = ToFVector(pose.Position);
	}
}

FMatrix FOculusRiftHMD::GetStereoProjectionMatrix(EStereoscopicPass StereoPassType, const float FOV) const
{
	auto frame = GetFrame();
	check(frame);
	check(IsStereoEnabled());

	const FSettings* FrameSettings = frame->GetSettings();

	const int idx = (StereoPassType == eSSP_LEFT_EYE) ? 0 : 1;

	FMatrix proj = ToFMatrix(FrameSettings->EyeProjectionMatrices[idx]);

	// correct far and near planes for reversed-Z projection matrix
	const float InNearZ = (FrameSettings->NearClippingPlane) ? FrameSettings->NearClippingPlane : GNearClippingPlane;
	const float InFarZ = (FrameSettings->FarClippingPlane) ? FrameSettings->FarClippingPlane : GNearClippingPlane;
	proj.M[3][3] = 0.0f;
	proj.M[2][3] = 1.0f;

	proj.M[2][2] = (InNearZ == InFarZ) ? 0.0f    : InNearZ / (InNearZ - InFarZ);
	proj.M[3][2] = (InNearZ == InFarZ) ? InNearZ : -InFarZ * InNearZ / (InNearZ - InFarZ);

	return proj;
}

void FOculusRiftHMD::GetOrthoProjection(int32 RTWidth, int32 RTHeight, float OrthoDistance, FMatrix OrthoProjection[2]) const
{
	const auto frame = GetFrame();
	check(frame);

	const FSettings* FrameSettings = frame->GetSettings();

	OrthoDistance /= frame->WorldToMetersScale; // This is meters from the camera (viewer) that we place the ortho plane.

    const OVR::Vector2f orthoScale[2] = 
	{
		OVR::Vector2f(1.f) / OVR::Vector2f(FrameSettings->EyeRenderDesc[0].PixelsPerTanAngleAtCenter),
		OVR::Vector2f(1.f) / OVR::Vector2f(FrameSettings->EyeRenderDesc[1].PixelsPerTanAngleAtCenter)
    };

	OVR::Matrix4f SubProjection[2];
	SubProjection[0] = ovrMatrix4f_OrthoSubProjection(FrameSettings->PerspectiveProjection[0], orthoScale[0], OrthoDistance, FrameSettings->EyeRenderDesc[0].HmdToEyeViewOffset.x);
	SubProjection[1] = ovrMatrix4f_OrthoSubProjection(FrameSettings->PerspectiveProjection[1], orthoScale[1], OrthoDistance, FrameSettings->EyeRenderDesc[1].HmdToEyeViewOffset.x);

    //Translate the subprojection for half of the screen . map it from [0,1] to [-1,1] . The total translation is translated * 0.25.
	OrthoProjection[0] = FTranslationMatrix(FVector(SubProjection[0].M[0][3] * RTWidth * .25 , 0 , 0));
    //Right eye gets translated to right half of screen
	OrthoProjection[1] = FTranslationMatrix(FVector(SubProjection[1].M[0][3] * RTWidth * .25 + RTWidth * .5 + FrameSettings->TexturePaddingPerEye*2, 0 , 0));

	if (FrameSettings->TexturePaddingPerEye > 0)
	{
		// Apply scale to compensate the texture padding between two views.
		FMatrix ScaleMatrix(
			FPlane(1, 0, 0, 0),
			FPlane(0, 0, 0, 0),
			FPlane(0, 0, 0, 0),
			FPlane(0, 0, 0, 0));

		ScaleMatrix = ScaleMatrix.ApplyScale(float(RTWidth) / (RTWidth + FrameSettings->TexturePaddingPerEye*2));
		ScaleMatrix.M[1][1] = 1.0f;
		ScaleMatrix.M[2][2] = 1.0f;
		ScaleMatrix.M[3][3] = 1.0f;

		OrthoProjection[0] *= ScaleMatrix;
		OrthoProjection[1] *= ScaleMatrix;
	}
}

void FOculusRiftHMD::InitCanvasFromView(FSceneView* InView, UCanvas* Canvas)
{
	// This is used for placing small HUDs (with names)
	// over other players (for example, in Capture Flag).
	// HmdOrientation should be initialized by GetCurrentOrientation (or
	// user's own value).
}

//---------------------------------------------------
// Oculus Rift ISceneViewExtension Implementation
//---------------------------------------------------

void FOculusRiftHMD::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	auto frame = GetFrame();
	check(frame);

	InViewFamily.EngineShowFlags.MotionBlur = 0;
#ifndef OVR_SDK_RENDERING
	InViewFamily.EngineShowFlags.HMDDistortion = frame->Settings->Flags.bHmdDistortion;
#else
	InViewFamily.EngineShowFlags.HMDDistortion = false;
#endif
	InViewFamily.EngineShowFlags.StereoRendering = IsStereoEnabled();

	frame->Flags.bScreenPercentageEnabled = InViewFamily.EngineShowFlags.ScreenPercentage;
}

void FOculusRiftHMD::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	auto frame = GetFrame();
	check(frame);

	InView.BaseHmdOrientation = frame->LastHmdOrientation;
	InView.BaseHmdLocation = frame->LastHmdPosition;

#ifndef OVR_SDK_RENDERING
	InViewFamily.bUseSeparateRenderTarget = false;

	// check and save texture size. 
	if (InView.StereoPass == eSSP_LEFT_EYE)
	{
		if (Settings->EyeRenderViewport[0].Size() != InView.ViewRect.Size())
		{
			Settings->SetEyeRenderViewport(InView.ViewRect.Size().X, InView.ViewRect.Size().Y);

			// patch EyeRenderViewport in the frame's settings as well
			FMemory::Memcpy(frame->GetSettings()->EyeRenderViewport, Settings->EyeRenderViewport);
			Flags.bNeedUpdateStereoRenderingParams = true;
		}
	}
#else
	InViewFamily.bUseSeparateRenderTarget = ShouldUseSeparateRenderTarget();

	if (InView.StereoPass == eSSP_RIGHT_EYE)
	{
		InView.ViewRect.Min.X += frame->GetSettings()->GetTexturePaddingPerEye()*2;
		InView.ViewRect.Max.X += frame->GetSettings()->GetTexturePaddingPerEye()*2;
	}

#endif

	const int eyeIdx = (InView.StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
	frame->CachedViewRotation[eyeIdx] = InView.ViewRotation;
}

bool FOculusRiftHMD::IsHeadTrackingAllowed() const
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		UEditorEngine* EdEngine = Cast<UEditorEngine>(GEngine);
		return Hmd && (!EdEngine || EdEngine->bUseVRPreviewForPlayWorld || GetDefault<ULevelEditorPlaySettings>()->ViewportGetsHMDControl) &&	(Settings->Flags.bHeadTrackingEnforced || GEngine->IsStereoscopic3D());
	}
#endif//WITH_EDITOR
	return Hmd && FHeadMountedDisplay::IsHeadTrackingAllowed();
}

//---------------------------------------------------
// Oculus Rift Specific
//---------------------------------------------------

FOculusRiftHMD::FOculusRiftHMD()
	:
	  Hmd(nullptr)
{
	OCFlags.Raw = 0;
	DeltaControlRotation = FRotator::ZeroRotator;

	Settings = MakeShareable(new FSettings);

	if (GIsEditor)
	{
		Settings->Flags.bOverrideScreenPercentage = true;
		Settings->ScreenPercentage = 100;
	}
	OSWindowHandle = nullptr;
	Startup();
}

FOculusRiftHMD::~FOculusRiftHMD()
{
	Shutdown();
}

void FOculusRiftHMD::Startup()
{
#if PLATFORM_MAC
	if (GIsEditor)
	{
		// no editor support for Mac yet
		return;
	}
#endif 
	if ((!IsRunningGame() && !GIsEditor) || (Settings->Flags.InitStatus & FSettings::eStartupExecuted) != 0)
	{
		// do not initialize plugin for server or if it was already initialized
		return;
	}
	Settings->Flags.InitStatus |= FSettings::eStartupExecuted;

	// Initializes LibOVR. 
	ovrInitParams initParams;
	FMemory::Memset(initParams, 0);
	initParams.Flags = ovrInit_RequestVersion;
	initParams.RequestedMinorVersion = OVR_MINOR_VERSION;
#if !UE_BUILD_SHIPPING
	initParams.LogCallback = OvrLogCallback;
#endif
	ovrBool bWasInitialized = ovr_Initialize(&initParams);

	if (GIsEditor)
	{
		Settings->Flags.bHeadTrackingEnforced = true;
	}

	bool bForced = FParse::Param(FCommandLine::Get(), TEXT("forcedrift"));
	bool bInitialized = false;
	if (!bForced)
	{
		bInitialized = InitDevice();
	}

	if (!bInitialized && !bForced)
	{
		ovr_Shutdown();
		Settings->Flags.InitStatus = 0;
		return;
	}

	// Uncap fps to enable FPS higher than 62
	GEngine->bSmoothFrameRate = false;

	SaveSystemValues();

#ifdef OVR_SDK_RENDERING
#if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)
	if (IsPCPlatform(GMaxRHIShaderPlatform) && !IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		pD3D11Bridge = new D3D11Bridge();
	}
#endif
#if defined(OVR_GL)
	if (IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		pOGLBridge = new OGLBridge();
	}
#endif
#endif // #ifdef OVR_SDK_RENDERING

	if (bForced || Hmd)
	{
		Settings->Flags.InitStatus |= FSettings::eInitialized;

		UE_LOG(LogHMD, Log, TEXT("Oculus plugin initialized. Version: %s"), *GetVersionString());
	}
}

void FOculusRiftHMD::Shutdown()
{
	if (!(Settings->Flags.InitStatus & FSettings::eInitialized))
	{
		return;
	}

	RestoreSystemValues();

#ifdef OVR_SDK_RENDERING
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(ShutdownRen,
	FOculusRiftHMD*, Plugin, this,
	{
		Plugin->ShutdownRendering();
	});
	FlushRenderingCommands();
#endif // OVR_SDK_RENDERING
	ReleaseDevice();
	
	Settings = nullptr;
	Frame = nullptr;
	RenderFrame = nullptr;

	ovr_Shutdown();
	UE_LOG(LogHMD, Log, TEXT("Oculus shutdown."));
}

bool FOculusRiftHMD::InitDevice()
{
	if (Hmd)
	{
		const ovrTrackingState ss = ovrHmd_GetTrackingState(Hmd, ovr_GetTimeInSeconds());
		if (!(ss.StatusFlags & ovrStatus_HmdConnected))
		{
			ReleaseDevice();
		}
		else
		{
			return true; // already created and present
		}
	}
	check(!Hmd);

	FSettings* CurrentSettings = GetSettings();

	Hmd = ovrHmd_Create(0);
	if (Hmd)
	{
		bDirectModeHack = (Hmd->HmdCaps & ovrHmdCap_ExtendDesktop) == 0;

		CurrentSettings->SupportedDistortionCaps = Hmd->DistortionCaps;
		CurrentSettings->SupportedHmdCaps = Hmd->HmdCaps;
		CurrentSettings->SupportedTrackingCaps = Hmd->TrackingCaps;

#ifndef OVR_SDK_RENDERING
		CurrentSettings->SupportedDistortionCaps &= ~ovrDistortionCap_Overdrive;
#endif
#ifndef OVR_VISION_ENABLED
		CurrentSettings->SupportedTrackingCaps &= ~ovrTrackingCap_Position;
#endif

		CurrentSettings->DistortionCaps = CurrentSettings->SupportedDistortionCaps & (ovrDistortionCap_TimeWarp | ovrDistortionCap_Vignette | ovrDistortionCap_Overdrive);
		CurrentSettings->TrackingCaps = CurrentSettings->SupportedTrackingCaps & (ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position);
		CurrentSettings->HmdCaps = CurrentSettings->SupportedHmdCaps & (ovrHmdCap_DynamicPrediction | ovrHmdCap_LowPersistence);
		CurrentSettings->HmdCaps |= (CurrentSettings->Flags.bVSync ? 0 : ovrHmdCap_NoVSync);

		if (!(CurrentSettings->SupportedDistortionCaps & ovrDistortionCap_TimeWarp))
		{
			CurrentSettings->Flags.bTimeWarp = false;
		}

		CurrentSettings->Flags.bHmdPosTracking = (CurrentSettings->SupportedTrackingCaps & ovrTrackingCap_Position) != 0;

		LoadFromIni();

		UpdateDistortionCaps();
		UpdateHmdRenderInfo();
		UpdateStereoRenderingParams();
		UpdateHmdCaps();
	}

	return Hmd != nullptr;
}

void FOculusRiftHMD::ReleaseDevice()
{
	if (Hmd)
	{
		SaveToIni();

		ovrHmd_AttachToWindow(Hmd, NULL, NULL, NULL);

		// Wait for all resources to be released
#ifdef OVR_SDK_RENDERING
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(ResetRen,
		FOculusRiftHMD*, Plugin, this,
		{
			if (Plugin->GetActiveRHIBridgeImpl())
			{
				Plugin->GetActiveRHIBridgeImpl()->Reset();
			}
		});
#endif 
		// Wait for all resources to be released
		FlushRenderingCommands();

		ovrHmd_Destroy(Hmd);
		Hmd = nullptr;
	}
}

void FOculusRiftHMD::UpdateDistortionCaps()
{
	FSettings* CurrentSettings = GetSettings();

	if (IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		CurrentSettings->DistortionCaps &= ~ovrDistortionCap_SRGB;
		CurrentSettings->DistortionCaps |= ovrDistortionCap_FlipInput;
	}
	(CurrentSettings->Flags.bTimeWarp) ? CurrentSettings->DistortionCaps |= ovrDistortionCap_TimeWarp : CurrentSettings->DistortionCaps &= ~ovrDistortionCap_TimeWarp;
	CurrentSettings->DistortionCaps |= ovrDistortionCap_Overdrive;
	//(CurrentSettings->Flags.bHQDistortion) ? CurrentSettings->DistortionCaps |= ovrDistortionCap_HqDistortion : CurrentSettings->DistortionCaps &= ~ovrDistortionCap_HqDistortion;
#if !UE_BUILD_SHIPPING
	(CurrentSettings->Flags.bProfiling) ? CurrentSettings->DistortionCaps |= ovrDistortionCap_ProfileNoSpinWaits : CurrentSettings->DistortionCaps &= ~ovrDistortionCap_ProfileNoSpinWaits;
#endif // #if !UE_BUILD_SHIPPING

#ifdef OVR_SDK_RENDERING 
	if (GetActiveRHIBridgeImpl())
	{
		GetActiveRHIBridgeImpl()->SetNeedReinitRendererAPI();
	}
#endif // OVR_SDK_RENDERING
	Flags.bNeedUpdateDistortionCaps = false;
}

void FOculusRiftHMD::UpdateHmdCaps()
{
	if (Hmd)
	{
		FSettings* CurrentSettings = GetSettings();

		CurrentSettings->TrackingCaps = ovrTrackingCap_Orientation;
		if (CurrentSettings->Flags.bYawDriftCorrectionEnabled)
		{
			CurrentSettings->TrackingCaps |= ovrTrackingCap_MagYawCorrection;
		}
		else
		{
			CurrentSettings->TrackingCaps &= ~ovrTrackingCap_MagYawCorrection;
		}
		if (CurrentSettings->Flags.bHmdPosTracking)
		{
			CurrentSettings->TrackingCaps |= ovrTrackingCap_Position;
		}
		else
		{
			CurrentSettings->TrackingCaps &= ~ovrTrackingCap_Position;
		}

		if (CurrentSettings->Flags.bLowPersistenceMode)
		{
			CurrentSettings->HmdCaps |= ovrHmdCap_LowPersistence;
		}
		else
		{
			CurrentSettings->HmdCaps &= ~ovrHmdCap_LowPersistence;
		}

		if (CurrentSettings->Flags.bVSync)
		{
			CurrentSettings->HmdCaps &= ~ovrHmdCap_NoVSync;
		}
		else
		{
			CurrentSettings->HmdCaps |= ovrHmdCap_NoVSync;
		}

		if (CurrentSettings->Flags.bMirrorToWindow)
		{
			CurrentSettings->HmdCaps &= ~ovrHmdCap_NoMirrorToWindow;
		}
		else
		{
			CurrentSettings->HmdCaps |= ovrHmdCap_NoMirrorToWindow;
		}
		ovrHmd_SetEnabledCaps(Hmd, CurrentSettings->HmdCaps);

		ovrHmd_ConfigureTracking(Hmd, CurrentSettings->TrackingCaps, 0);
		Flags.bNeedUpdateHmdCaps = false;
	}
}

FORCEINLINE static float GetVerticalFovRadians(const ovrFovPort& fov)
{
	return FMath::Atan(fov.UpTan) + FMath::Atan(fov.DownTan);
}

FORCEINLINE static float GetHorizontalFovRadians(const ovrFovPort& fov)
{
	return FMath::Atan(fov.LeftTan) + FMath::Atan(fov.RightTan);
}

void FOculusRiftHMD::UpdateHmdRenderInfo()
{
	check(Hmd);

	UE_LOG(LogHMD, Warning, TEXT("HMD %s, Monitor %s, res = %d x %d, windowPos = {%d, %d}"), ANSI_TO_TCHAR(Hmd->ProductName), 
		ANSI_TO_TCHAR(Hmd->DisplayDeviceName), Hmd->Resolution.w, Hmd->Resolution.h, Hmd->WindowsPos.x, Hmd->WindowsPos.y); 

	FSettings* CurrentSettings = GetSettings();

	// Calc FOV
	if (!CurrentSettings->Flags.bOverrideFOV)
	{
		// Calc FOV, symmetrical, for each eye. 
		CurrentSettings->EyeFov[0] = Hmd->DefaultEyeFov[0];
		CurrentSettings->EyeFov[1] = Hmd->DefaultEyeFov[1];

		// Calc FOV in radians
		CurrentSettings->VFOVInRadians = FMath::Max(GetVerticalFovRadians(CurrentSettings->EyeFov[0]), GetVerticalFovRadians(CurrentSettings->EyeFov[1]));
		CurrentSettings->HFOVInRadians = FMath::Max(GetHorizontalFovRadians(CurrentSettings->EyeFov[0]), GetHorizontalFovRadians(CurrentSettings->EyeFov[1]));
	}

	const ovrSizei recommenedTex0Size = ovrHmd_GetFovTextureSize(Hmd, ovrEye_Left, CurrentSettings->EyeFov[0], 1.0f);
	const ovrSizei recommenedTex1Size = ovrHmd_GetFovTextureSize(Hmd, ovrEye_Right, CurrentSettings->EyeFov[1], 1.0f);

	ovrSizei idealRenderTargetSize;
	idealRenderTargetSize.w = recommenedTex0Size.w + recommenedTex1Size.w;
	idealRenderTargetSize.h = FMath::Max(recommenedTex0Size.h, recommenedTex1Size.h);

	CurrentSettings->IdealScreenPercentage = FMath::Max(float(idealRenderTargetSize.w) / float(Hmd->Resolution.w) * 100.f,
														float(idealRenderTargetSize.h) / float(Hmd->Resolution.h) * 100.f);

	// Override eye distance by the value from HMDInfo (stored in Profile).
	if (!CurrentSettings->Flags.bOverrideIPD)
	{
		CurrentSettings->InterpupillaryDistance = ovrHmd_GetFloat(Hmd, OVR_KEY_IPD, OVR_DEFAULT_IPD);
	}

	// cache eye to neck distance.
	float neck2eye[2] = { OVR_DEFAULT_NECK_TO_EYE_HORIZONTAL, OVR_DEFAULT_NECK_TO_EYE_VERTICAL };
	ovrHmd_GetFloatArray(Hmd, OVR_KEY_NECK_TO_EYE_DISTANCE, neck2eye, 2);
	CurrentSettings->NeckToEyeInMeters = FVector2D(neck2eye[0], neck2eye[1]);

	// Default texture size (per eye) is equal to half of W x H resolution. Will be overridden in SetupView.
	//CurrentSettings->SetViewportSize(Hmd->Resolution.w / 2, Hmd->Resolution.h);

	Flags.bNeedUpdateStereoRenderingParams = true;
}

void FOculusRiftHMD::UpdateStereoRenderingParams()
{
	check(IsInGameThread());

	FSettings* CurrentSettings = GetSettings();

	if ((!CurrentSettings->IsStereoEnabled() && !CurrentSettings->Flags.bHeadTrackingEnforced))
	{
		return;
	}
	if (IsInitialized() && Hmd)
	{
		//!AB: note, for Direct Rendering EyeRenderDesc is calculated twice, once
		// here and another time in BeginRendering_RenderThread. I need to have EyeRenderDesc
		// on a game thread for ViewAdjust (for StereoViewOffset calculation).
		CurrentSettings->EyeRenderDesc[0] = ovrHmd_GetRenderDesc(Hmd, ovrEye_Left, CurrentSettings->EyeFov[0]);
		CurrentSettings->EyeRenderDesc[1] = ovrHmd_GetRenderDesc(Hmd, ovrEye_Right, CurrentSettings->EyeFov[1]);
		if (CurrentSettings->Flags.bOverrideIPD)
		{
			CurrentSettings->EyeRenderDesc[0].HmdToEyeViewOffset.x = CurrentSettings->InterpupillaryDistance * 0.5f;
			CurrentSettings->EyeRenderDesc[1].HmdToEyeViewOffset.x = -CurrentSettings->InterpupillaryDistance * 0.5f;
		}

		const unsigned int ProjModifiers = ovrProjection_None; //@TODO revise to use ovrProjection_FarClipAtInfinity and/or ovrProjection_FarLessThanNear
		// Far and Near clipping planes will be modified in GetStereoProjectionMatrix()
		CurrentSettings->EyeProjectionMatrices[0] = ovrMatrix4f_Projection(CurrentSettings->EyeFov[0], 0.01f, 10000.0f, ProjModifiers);
		CurrentSettings->EyeProjectionMatrices[1] = ovrMatrix4f_Projection(CurrentSettings->EyeFov[1], 0.01f, 10000.0f, ProjModifiers);

		CurrentSettings->PerspectiveProjection[0] = ovrMatrix4f_Projection(CurrentSettings->EyeFov[0], 0.01f, 10000.f, ProjModifiers | ovrProjection_RightHanded);
		CurrentSettings->PerspectiveProjection[1] = ovrMatrix4f_Projection(CurrentSettings->EyeFov[1], 0.01f, 10000.f, ProjModifiers | ovrProjection_RightHanded);

 		PrecalculateDistortionMesh();
		Flags.bNeedUpdateStereoRenderingParams = false;
	}
}

void FOculusRiftHMD::LoadFromIni()
{
	const TCHAR* OculusSettings = TEXT("Oculus.Settings");
	bool v;
	float f;
	if (GConfig->GetBool(OculusSettings, TEXT("bChromaAbCorrectionEnabled"), v, GEngineIni))
	{
		Settings->Flags.bChromaAbCorrectionEnabled = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bYawDriftCorrectionEnabled"), v, GEngineIni))
	{
		Settings->Flags.bYawDriftCorrectionEnabled = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bDevSettingsEnabled"), v, GEngineIni))
	{
		Settings->Flags.bDevSettingsEnabled = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bOverrideIPD"), v, GEngineIni))
	{
		Settings->Flags.bOverrideIPD = v;
		if (Settings->Flags.bOverrideIPD)
		{
			if (GConfig->GetFloat(OculusSettings, TEXT("IPD"), f, GEngineIni))
			{
				SetInterpupillaryDistance(f);
			}
		}
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bOverrideStereo"), v, GEngineIni))
	{
		Settings->Flags.bOverrideStereo = v;
		if (Settings->Flags.bOverrideStereo)
		{
			if (GConfig->GetFloat(OculusSettings, TEXT("HFOV"), f, GEngineIni))
			{
				Settings->HFOVInRadians = f;
			}
			if (GConfig->GetFloat(OculusSettings, TEXT("VFOV"), f, GEngineIni))
			{
				Settings->VFOVInRadians = f;
			}
		}
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bOverrideVSync"), v, GEngineIni))
	{
		Settings->Flags.bOverrideVSync = v;
		if (GConfig->GetBool(OculusSettings, TEXT("bVSync"), v, GEngineIni))
		{
			Settings->Flags.bVSync = v;
		}
	}
	if (!GIsEditor)
	{
		if (GConfig->GetBool(OculusSettings, TEXT("bOverrideScreenPercentage"), v, GEngineIni))
		{
			Settings->Flags.bOverrideScreenPercentage = v;
			if (GConfig->GetFloat(OculusSettings, TEXT("ScreenPercentage"), f, GEngineIni))
			{
				Settings->ScreenPercentage = f;
			}
		}
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bAllowFinishCurrentFrame"), v, GEngineIni))
	{
		Settings->Flags.bAllowFinishCurrentFrame = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bLowPersistenceMode"), v, GEngineIni))
	{
		Settings->Flags.bLowPersistenceMode = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bUpdateOnRT"), v, GEngineIni))
	{
		Settings->Flags.bUpdateOnRT = v;
	}
	if (GConfig->GetFloat(OculusSettings, TEXT("FarClippingPlane"), f, GEngineIni))
	{
		Settings->FarClippingPlane = f;
	}
	if (GConfig->GetFloat(OculusSettings, TEXT("NearClippingPlane"), f, GEngineIni))
	{
		Settings->NearClippingPlane = f;
	}
#if !UE_BUILD_SHIPPING
	FString s;
	if (GConfig->GetString(OculusSettings, TEXT("CubeMeshName"), s, GEngineIni))
	{
		CubeMeshName = s;
	}
	if (GConfig->GetString(OculusSettings, TEXT("CubeMaterialName"), s, GEngineIni))
	{
		CubeMaterialName = s;
	}
#endif
}

void FOculusRiftHMD::SaveToIni()
{
	const TCHAR* OculusSettings = TEXT("Oculus.Settings");
	GConfig->SetBool(OculusSettings, TEXT("bChromaAbCorrectionEnabled"), Settings->Flags.bChromaAbCorrectionEnabled, GEngineIni);
	GConfig->SetBool(OculusSettings, TEXT("bYawDriftCorrectionEnabled"), Settings->Flags.bYawDriftCorrectionEnabled, GEngineIni);
	GConfig->SetBool(OculusSettings, TEXT("bDevSettingsEnabled"), Settings->Flags.bDevSettingsEnabled, GEngineIni);

	GConfig->SetBool(OculusSettings, TEXT("bOverrideIPD"), Settings->Flags.bOverrideIPD, GEngineIni);
	if (Settings->Flags.bOverrideIPD)
	{
		GConfig->SetFloat(OculusSettings, TEXT("IPD"), GetInterpupillaryDistance(), GEngineIni);
	}
	GConfig->SetBool(OculusSettings, TEXT("bOverrideStereo"), Settings->Flags.bOverrideStereo, GEngineIni);
	if (Settings->Flags.bOverrideStereo)
	{
		GConfig->SetFloat(OculusSettings, TEXT("HFOV"), Settings->HFOVInRadians, GEngineIni);
		GConfig->SetFloat(OculusSettings, TEXT("VFOV"), Settings->VFOVInRadians, GEngineIni);
	}

	GConfig->SetBool(OculusSettings, TEXT("bOverrideVSync"), Settings->Flags.bOverrideVSync, GEngineIni);
	if (Settings->Flags.bOverrideVSync)
	{
		GConfig->SetBool(OculusSettings, TEXT("VSync"), Settings->Flags.bVSync, GEngineIni);
	}

	if (!GIsEditor)
	{
		GConfig->SetBool(OculusSettings, TEXT("bOverrideScreenPercentage"), Settings->Flags.bOverrideScreenPercentage, GEngineIni);
		if (Settings->Flags.bOverrideScreenPercentage)
		{
			// Save the current ScreenPercentage state
			GConfig->SetFloat(OculusSettings, TEXT("ScreenPercentage"), Settings->ScreenPercentage, GEngineIni);
		}
	}
	GConfig->SetBool(OculusSettings, TEXT("bAllowFinishCurrentFrame"), Settings->Flags.bAllowFinishCurrentFrame, GEngineIni);

	GConfig->SetBool(OculusSettings, TEXT("bLowPersistenceMode"), Settings->Flags.bLowPersistenceMode, GEngineIni);

	GConfig->SetBool(OculusSettings, TEXT("bUpdateOnRT"), Settings->Flags.bUpdateOnRT, GEngineIni);

	if (Settings->Flags.bClippingPlanesOverride)
	{
		GConfig->SetFloat(OculusSettings, TEXT("FarClippingPlane"), Settings->FarClippingPlane, GEngineIni);
		GConfig->SetFloat(OculusSettings, TEXT("NearClippingPlane"), Settings->NearClippingPlane, GEngineIni);
	}
}

bool FOculusRiftHMD::HandleInputKey(UPlayerInput* pPlayerInput,
	const FKey& Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
	if (Hmd && EventType == IE_Pressed && Settings->IsStereoEnabled())
	{
		if (!Key.IsMouseButton())
		{
			ovrHmd_DismissHSWDisplay(Hmd);
		}
	}
	return false;
}

void FOculusRiftHMD::OnBeginPlay()
{
	// @TODO: add more values here.
	// This call make sense when 'Play' is used from the Editor;
	if (GIsEditor)
	{
		Settings->BaseOrientation = FQuat::Identity;
		Settings->BaseOffset = FVector::ZeroVector;
		Settings->WorldToMetersScale = 100.f;
		Settings->Flags.bWorldToMetersOverride = false;
		InitDevice();
	}
}

void FOculusRiftHMD::OnEndPlay()
{
	if (GIsEditor)
	{
		EnableStereo(false);
		ReleaseDevice();
	}
}

void FOculusRiftHMD::GetRawSensorData(SensorData& OutData)
{
	FMemory::Memset(OutData, 0);
	InitDevice();
	if (Hmd)
	{
		const ovrTrackingState ss = ovrHmd_GetTrackingState(Hmd, ovr_GetTimeInSeconds());
		OutData.Accelerometer	= ToFVector(ss.RawSensorData.Accelerometer);
		OutData.Gyro			= ToFVector(ss.RawSensorData.Gyro);
		OutData.Magnetometer	= ToFVector(ss.RawSensorData.Magnetometer);
		OutData.Temperature		= ss.RawSensorData.Temperature;
		OutData.TimeInSeconds   = ss.RawSensorData.TimeInSeconds;
	}
}

bool FOculusRiftHMD::GetUserProfile(UserProfile& OutProfile) 
{
	InitDevice();
	if (Hmd)
	{
		OutProfile.Name = ovrHmd_GetString(Hmd, OVR_KEY_USER, "");
		OutProfile.Gender = ovrHmd_GetString(Hmd, OVR_KEY_GENDER, OVR_DEFAULT_GENDER);
		OutProfile.PlayerHeight = ovrHmd_GetFloat(Hmd, OVR_KEY_PLAYER_HEIGHT, OVR_DEFAULT_PLAYER_HEIGHT);
		OutProfile.EyeHeight = ovrHmd_GetFloat(Hmd, OVR_KEY_EYE_HEIGHT, OVR_DEFAULT_EYE_HEIGHT);
		OutProfile.IPD = ovrHmd_GetFloat(Hmd, OVR_KEY_IPD, OVR_DEFAULT_IPD);
		float neck2eye[2] = {OVR_DEFAULT_NECK_TO_EYE_HORIZONTAL, OVR_DEFAULT_NECK_TO_EYE_VERTICAL};
		ovrHmd_GetFloatArray(Hmd, OVR_KEY_NECK_TO_EYE_DISTANCE, neck2eye, 2);
		OutProfile.NeckToEyeDistance = FVector2D(neck2eye[0], neck2eye[1]);
		OutProfile.ExtraFields.Reset();

		int EyeRelief = ovrHmd_GetInt(Hmd, OVR_KEY_EYE_RELIEF_DIAL, OVR_DEFAULT_EYE_RELIEF_DIAL);
		OutProfile.ExtraFields.Add(FString(OVR_KEY_EYE_RELIEF_DIAL)) = FString::FromInt(EyeRelief);
		
		float eye2nose[2] = { 0.f, 0.f };
		ovrHmd_GetFloatArray(Hmd, OVR_KEY_EYE_TO_NOSE_DISTANCE, eye2nose, 2);
		OutProfile.ExtraFields.Add(FString(OVR_KEY_EYE_RELIEF_DIAL"Horizontal")) = FString::SanitizeFloat(eye2nose[0]);
		OutProfile.ExtraFields.Add(FString(OVR_KEY_EYE_RELIEF_DIAL"Vertical")) = FString::SanitizeFloat(eye2nose[1]);

		float maxEye2Plate[2] = { 0.f, 0.f };
		ovrHmd_GetFloatArray(Hmd, OVR_KEY_MAX_EYE_TO_PLATE_DISTANCE, maxEye2Plate, 2);
		OutProfile.ExtraFields.Add(FString(OVR_KEY_MAX_EYE_TO_PLATE_DISTANCE"Horizontal")) = FString::SanitizeFloat(maxEye2Plate[0]);
		OutProfile.ExtraFields.Add(FString(OVR_KEY_MAX_EYE_TO_PLATE_DISTANCE"Vertical")) = FString::SanitizeFloat(maxEye2Plate[1]);

		OutProfile.ExtraFields.Add(FString(ovrHmd_GetString(Hmd, OVR_KEY_EYE_CUP, "")));
		return true;
	}
	return false; 
}

//////////////////////////////////////////////////////////////////////////
FViewExtension::FViewExtension(FHeadMountedDisplay* InDelegate) 
	: FHMDViewExtension(InDelegate)
	, ShowFlags(ESFIM_All0)
	, bFrameBegun(false)
{
	auto OculusHMD = static_cast<FOculusRiftHMD*>(InDelegate);
	Hmd = OculusHMD->Hmd;
#ifdef OVR_SDK_RENDERING
	pPresentBridge = OculusHMD->GetActiveRHIBridgeImpl();
#endif
}

#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS
#endif //#if PLATFORM_MAC
