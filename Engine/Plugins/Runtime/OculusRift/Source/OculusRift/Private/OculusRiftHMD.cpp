// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "HMDPrivatePCH.h"
#include "OculusRiftHMD.h"
#include "OculusRiftMeshAssets.h"

#if !PLATFORM_MAC // Mac uses 0.5/OculusRiftHMD_05.cpp

#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "SceneViewport.h"
#include "PostProcess/SceneRenderTargets.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#endif

//---------------------------------------------------
// Oculus Rift Plugin Implementation
//---------------------------------------------------

#if OCULUS_RIFT_SUPPORTED_PLATFORMS
#if !UE_BUILD_SHIPPING
static void OVR_CDECL OvrLogCallback(uintptr_t userData, int level, const char* message)
{
	FString tbuf = ANSI_TO_TCHAR(message);
	const TCHAR* levelStr = TEXT("");
	switch (level)
	{
	case ovrLogLevel_Debug: levelStr = TEXT(" Debug:"); break;
	case ovrLogLevel_Info:  levelStr = TEXT(" Info:"); break;
	case ovrLogLevel_Error: levelStr = TEXT(" Error:"); break;
	}

	GLog->Logf(TEXT("OCULUS:%s %s"), levelStr, *tbuf);
}
#endif // !UE_BUILD_SHIPPING
#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS

class FOculusRiftPlugin : public IOculusRiftPlugin
{
public:
	FOculusRiftPlugin()
	{
		bInitialized = false;
		bInitializeCalled = false;
	}

protected:
	bool Initialize()
	{
		if(!bInitializeCalled)
		{
			bInitialized = false;
			bInitializeCalled = true;

#if OCULUS_RIFT_SUPPORTED_PLATFORMS
			ovrInitParams initParams;
			FMemory::Memset(initParams, 0);
			initParams.Flags = ovrInit_RequestVersion;
			initParams.RequestedMinorVersion = OVR_MINOR_VERSION;
#if !UE_BUILD_SHIPPING
			initParams.LogCallback = OvrLogCallback;
#endif
			ovrResult result = ovr_Initialize(&initParams);

			if(OVR_SUCCESS(result))
			{
				bInitialized = true;
			}
			else if(ovrError_LibLoad == result)
			{
				// Can't load library!
				UE_LOG(LogHMD, Log, TEXT("Can't find Oculus library %s: is proper Runtime installed? Version: %s"), 
					TEXT(OVR_FILE_DESCRIPTION_STRING), TEXT(OVR_VERSION_STRING));
			}
#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
		}

		return bInitialized;
	}

	/** IModuleInterface implementation */
	virtual void ShutdownModule() override
	{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
		if(bInitialized)
		{
			ovr_Shutdown();
			UE_LOG(LogHMD, Log, TEXT("Oculus shutdown."));

			bInitialized = false;
			bInitializeCalled = false;
		}
#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
	}

	/** IHeadMountedDisplayModule implementation */
	virtual void PreInit() override
	{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
		if(Initialize())
		{
			FOculusRiftHMD::PreInit();
		}
#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
	}

	virtual TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > CreateHeadMountedDisplay() override
	{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
		// Only init plugin when running Game or Editor
		if(IsRunningGame() || GIsEditor)
		{
			if (Initialize())
			{
				TSharedPtr< FOculusRiftHMD, ESPMode::ThreadSafe > OculusRiftHMD(new FOculusRiftHMD());
				if (OculusRiftHMD->IsInitialized())
				{
					HeadMountedDisplay = OculusRiftHMD;
					return OculusRiftHMD;
				}
			}
		}
		else if (bInitialized)
		{
			ovr_Shutdown();
			bInitialized = true;
		}
#endif//OCULUS_RIFT_SUPPORTED_PLATFORMS
		HeadMountedDisplay = nullptr;
		return NULL;
	}

	FString GetModulePriorityKeyName() const
	{
		return FString(TEXT("OculusRift"));
	}

	/** IOculusRiftPlugin implementation */
	virtual bool PoseToOrientationAndPosition(const ovrPosef& Pose, FQuat& OutOrientation, FVector& OutPosition) const override
	{
		check(IsInGameThread());
		bool bRetVal = false;

#if OCULUS_RIFT_SUPPORTED_PLATFORMS
		IHeadMountedDisplay* HMD = HeadMountedDisplay.Pin().Get();
		if (HMD && HMD->GetHMDDeviceType() == EHMDDeviceType::DT_OculusRift)
		{
			FOculusRiftHMD* OculusHMD = static_cast<FOculusRiftHMD*>(HMD);

			auto Frame = OculusHMD->GetFrame();
			if (Frame != nullptr)
			{
				Frame->PoseToOrientationAndPosition(Pose, /* Out */ OutOrientation, /* Out */ OutPosition);
				bRetVal = true;
			}
		}
#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS
		return bRetVal;
	}

	virtual ovrSession GetSession() override
	{
		check(IsInGameThread());
		ovrSession bRetVal = nullptr;

#if OCULUS_RIFT_SUPPORTED_PLATFORMS
		IHeadMountedDisplay* HMD = HeadMountedDisplay.Pin().Get();
		if (HMD && HMD->GetHMDDeviceType() == EHMDDeviceType::DT_OculusRift)
		{
			FOculusRiftHMD* OculusHMD = static_cast<FOculusRiftHMD*>(HMD);
			bRetVal = OculusHMD->OvrSession;
		}
#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS
		return bRetVal;
	}

	virtual bool GetCurrentTrackingState(ovrTrackingState* TrackingState) override
	{
		check(IsInGameThread());
		bool bRetVal = false;

#if OCULUS_RIFT_SUPPORTED_PLATFORMS
		IHeadMountedDisplay* HMD = HeadMountedDisplay.Pin().Get();
		if (TrackingState && HMD && HMD->GetHMDDeviceType() == EHMDDeviceType::DT_OculusRift)
		{
			FOculusRiftHMD* OculusHMD = static_cast<FOculusRiftHMD*>(HMD);
			auto Frame = OculusHMD->GetFrame();
			if (Frame != nullptr)
			{
				*TrackingState = Frame->InitialTrackingState;
				bRetVal = true;
			}
		}
#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS
		return bRetVal;
	}

protected:
	bool bInitialized;
	bool bInitializeCalled;
	TWeakPtr< IHeadMountedDisplay, ESPMode::ThreadSafe > HeadMountedDisplay;
};

IMPLEMENT_MODULE( FOculusRiftPlugin, OculusRift )

//---------------------------------------------------
// Oculus Rift IHeadMountedDisplay Implementation
//---------------------------------------------------

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

//////////////////////////////////////////////////////////////////////////
FSettings::FSettings()
{
	FMemory::Memset(EyeRenderDesc, 0);
	FMemory::Memset(EyeProjectionMatrices, 0);
	FMemory::Memset(EyeFov, 0);

	SupportedTrackingCaps = SupportedHmdCaps = 0;
	TrackingCaps = HmdCaps = 0;

	MirrorWindowMode = eMirrorWindow_Distorted;

	PixelDensity = 1.0f;

	RenderTargetSize = FIntPoint(0, 0);
	QueueAheadStatus = EQA_Default;
}

TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> FSettings::Clone() const
{
	TSharedPtr<FSettings, ESPMode::ThreadSafe> NewSettings = MakeShareable(new FSettings(*this));
	return NewSettings;
}

//////////////////////////////////////////////////////////////////////////
FDetectHmd::FDetectHmd(FOculusRiftHMD* InOculusRiftHMD, bool bForced) :
	OculusRiftHMD(InOculusRiftHMD)
{
	OculusRiftHMD->HmdDetected = bForced || ovr_GetHmdDesc(nullptr).Type != ovrHmd_None;

	StopEvent = FPlatformProcess::CreateSynchEvent();

	if(StopEvent)
	{
		RunnableThread = FRunnableThread::Create(this, TEXT("DetectHMDThread"), 0, TPri_BelowNormal);
	}
}

FDetectHmd::~FDetectHmd()
{	
	if(RunnableThread)
	{
		RunnableThread->Kill(true);
	}
}

uint32 FDetectHmd::Run()
{
	while(!StopEvent->Wait(1000, true))
	{
		if (!OculusRiftHMD->HmdDetected &&
			OculusRiftHMD->Settings->Flags.bHMDEnabled && 
			ovr_GetHmdDesc(nullptr).Type != ovrHmd_None)
		{
			OculusRiftHMD->HmdDetected = true;
		}
	}
	
	return 0;
}

void FDetectHmd::Stop()
{
	if(StopEvent)
	{
		StopEvent->Trigger();
	}
}

//////////////////////////////////////////////////////////////////////////
FGameFrame::FGameFrame()
{
	FMemory::Memset(InitialTrackingState, 0);
	FMemory::Memset(CurEyeRenderPose, 0);
	FMemory::Memset(CurHeadPose, 0);
	FMemory::Memset(EyeRenderPose, 0);
	FMemory::Memset(HeadPose, 0);
}

TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> FGameFrame::Clone() const
{
	TSharedPtr<FGameFrame, ESPMode::ThreadSafe> NewFrame = MakeShareable(new FGameFrame(*this));
	return NewFrame;
}

//////////////////////////////////////////////////////////////////////////
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

bool FOculusRiftHMD::OnStartGameFrame( FWorldContext& WorldContext )
{
	// check if HMD is marked as invalid and needs to be killed.
	if (pCustomPresent && pCustomPresent->NeedsToKillHmd() && OvrSession)
	{
		if (IsStereoEnabled())
		{
			EnableStereo(false);
		}
		ReleaseDevice();
	}

	if (!FHeadMountedDisplay::OnStartGameFrame( WorldContext ))
	{
		return false;
	}

	FGameFrame* CurrentFrame = GetFrame();

	// need to make a copy of settings here, since settings could change.
	CurrentFrame->Settings = Settings->Clone();
	FSettings* CurrentSettings = CurrentFrame->GetSettings();

	if (OvrSession)
	{
		// Save eye and head poses
		CurrentFrame->InitialTrackingState = CurrentFrame->GetTrackingState(OvrSession);
		CurrentFrame->GetHeadAndEyePoses(CurrentFrame->InitialTrackingState, CurrentFrame->CurHeadPose, CurrentFrame->CurEyeRenderPose);
		if (CurrentSettings->Flags.bHmdPosTracking)
		{
			CurrentFrame->Flags.bHaveVisionTracking = (CurrentFrame->InitialTrackingState.StatusFlags & ovrStatus_PositionTracked) != 0;
			if (CurrentFrame->Flags.bHaveVisionTracking && !Flags.bHadVisionTracking)
			{
				UE_LOG(LogHMD, Log, TEXT("Vision Tracking Acquired"));
			}
			if (!CurrentFrame->Flags.bHaveVisionTracking && Flags.bHadVisionTracking)
			{
				UE_LOG(LogHMD, Log, TEXT("Lost Vision Tracking"));
			}
			Flags.bHadVisionTracking = CurrentFrame->Flags.bHaveVisionTracking;
		}
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
	if(!HmdDetected || !Settings->Flags.bHMDEnabled)
	{
		return false;
	}

	if(OvrSession && pCustomPresent && pCustomPresent->NeedsToKillHmd())
	{
		HmdDetected = false;
		return false;
	}

	return true;
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
	MonitorDesc.MonitorName = "";
	MonitorDesc.MonitorId = 0;
	MonitorDesc.DesktopX = MonitorDesc.DesktopY = MonitorDesc.ResolutionX = MonitorDesc.ResolutionY = 0;
	MonitorDesc.WindowSizeX = MonitorDesc.WindowSizeY = 0;
	
	ovrHmdDesc Desc = (OvrSession) ? HmdDesc : ovr_GetHmdDesc(nullptr);
	if (Desc.Type != ovrHmd_None)
	{
		MonitorDesc.ResolutionX = Desc.Resolution.w;
		MonitorDesc.ResolutionY = Desc.Resolution.h;
		MonitorDesc.WindowSizeX = Settings->MirrorWindowSize.X;
		MonitorDesc.WindowSizeY = Settings->MirrorWindowSize.Y;
		return true;
	}
	return false;
}

bool FOculusRiftHMD::IsFullscreenAllowed()
{
	return false;
}

bool FOculusRiftHMD::DoesSupportPositionalTracking() const
{
	const FGameFrame* frame = GetFrame();
	const FSettings* OculusSettings = frame->GetSettings();
	return (frame && OculusSettings->Flags.bHmdPosTracking && (OculusSettings->SupportedTrackingCaps & ovrTrackingCap_Position) != 0);
}

bool FOculusRiftHMD::HasValidTrackingPosition()
{
	const auto frame = GetFrame();
	return (frame && frame->Settings->Flags.bHmdPosTracking && frame->Flags.bHaveVisionTracking);
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

	if (!OvrSession)
	{
		return;
	}

	check(frame->WorldToMetersScale >= 0);
	OutCameraDistance = TRACKER_FOCAL_DISTANCE * frame->WorldToMetersScale;
	OutHFOV = FMath::RadiansToDegrees(HmdDesc.CameraFrustumHFovInRadians);
	OutVFOV = FMath::RadiansToDegrees(HmdDesc.CameraFrustumVFovInRadians);
	OutNearPlane = HmdDesc.CameraFrustumNearZInMeters * frame->WorldToMetersScale;
	OutFarPlane = HmdDesc.CameraFrustumFarZInMeters * frame->WorldToMetersScale;

	// Read the camera pose
	if (!(frame->InitialTrackingState.StatusFlags & ovrStatus_CameraPoseTracked))
	{
		return;
	}
	const ovrPosef& cameraPose = frame->InitialTrackingState.CameraPose;

	FQuat Orient;
	FVector Pos;
	frame->PoseToOrientationAndPosition(cameraPose, Orient, Pos);

	OutOrientation = Orient;
	OutOrigin = Pos + frame->Settings->PositionOffset;
}

void FOculusRiftHMD::RebaseObjectOrientationAndPosition(FVector& OutPosition, FQuat& OutOrientation) const
{
}

bool FOculusRiftHMD::IsInLowPersistenceMode() const
{
	return true;
}

void FOculusRiftHMD::EnableLowPersistenceMode(bool Enable)
{
	Settings->Flags.bLowPersistenceMode = Enable;
	Flags.bNeedUpdateHmdCaps = true;
}

// Returns tracking state for current frame
ovrTrackingState FGameFrame::GetTrackingState(ovrSession InOvrSession) const
{
	const FSettings* CurrentSettings = GetSettings();
	const double DisplayTime = ovr_GetPredictedDisplayTime(InOvrSession, FrameNumber);
	const bool LatencyMarker = (IsInRenderingThread() || !CurrentSettings->Flags.bUpdateOnRT);
	return ovr_GetTrackingState(InOvrSession, DisplayTime, LatencyMarker);
}

// Returns HeadPose and EyePoses calculated from TrackingState
void FGameFrame::GetHeadAndEyePoses(const ovrTrackingState& TrackingState, ovrPosef& outHeadPose, ovrPosef outEyePoses[2]) const
{
	const FSettings* CurrentSettings = GetSettings();
	const ovrVector3f hmdToEyeViewOffset[2] = { CurrentSettings->EyeRenderDesc[0].HmdToEyeViewOffset, CurrentSettings->EyeRenderDesc[1].HmdToEyeViewOffset };

	outHeadPose = TrackingState.HeadPose.ThePose;
	ovr_CalcEyePoses(TrackingState.HeadPose.ThePose, hmdToEyeViewOffset, outEyePoses);
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
		frame->HeadPose = frame->CurHeadPose;
	}

	frame->PoseToOrientationAndPosition(frame->CurHeadPose, CurrentHmdOrientation, CurrentHmdPosition);
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
	Settings->InterpupillaryDistance = ovr_GetFloat(OvrSession, OVR_KEY_IPD, OVR_DEFAULT_IPD);
}

bool FOculusRiftHMD::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (FHeadMountedDisplay::Exec(InWorld, Cmd, Ar))
	{
		if (FParse::Command(&Cmd, TEXT("HMD")))
		{
			if (FParse::Command(&Cmd, TEXT("SP")) ||              // screen percentage is deprecated 
				FParse::Command(&Cmd, TEXT("SCREENPERCENTAGE")))  // use pd - pixel density
			{
				// need to convert screenpercentage to pixel density. Set PixelDensity to 0 to indicate that.
				if (Settings->Flags.bOverrideScreenPercentage)
				{
					GetSettings()->PixelDensity = 0.f;
				}
				else
				{
					GetSettings()->PixelDensity = 1.f; // SP RESET. Set PD to 1.f
				}
				Flags.bNeedUpdateStereoRenderingParams = true;
			}
		}
		else if (FParse::Command(&Cmd, TEXT("HMDPOS")))
		{
			if (FParse::Command(&Cmd, TEXT("ENFORCE")))
			{
				// need to init device
				if (Settings->Flags.bHeadTrackingEnforced)
				{
					InitDevice();
				}
			}
		}
		return true;
	}

	auto frame = GetFrame();

	if (FParse::Command(&Cmd, TEXT("HMD")))
	{
		if (FParse::Command(&Cmd, TEXT("PD"))) // pixel density
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (CmdName.IsEmpty())
				return false;

			float pd = FCString::Atof(*CmdName);
			if (pd > 0 && pd <= 3)
			{
				GetSettings()->PixelDensity = pd;
				Flags.bNeedUpdateStereoRenderingParams = true;
			}
			else
			{
				Ar.Logf(TEXT("Value is out of range (0.0..3.0f]"));
			}
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("QAHEAD"))) // pixel density
		{
			FString CmdName = FParse::Token(Cmd, 0);

			FSettings::EQueueAheadStatus qaPrev = GetSettings()->QueueAheadStatus;
			if (!FCString::Stricmp(*CmdName, TEXT("ON")))
			{
				GetSettings()->QueueAheadStatus = FSettings::EQA_Enabled;
			}
			else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
			{
				GetSettings()->QueueAheadStatus = FSettings::EQA_Disabled;
			}
			else if (!FCString::Stricmp(*CmdName, TEXT("RESET")))
			{
				GetSettings()->QueueAheadStatus = FSettings::EQA_Default;
			}
			else
			{
				GetSettings()->QueueAheadStatus = (GetSettings()->QueueAheadStatus == FSettings::EQA_Enabled) ? FSettings::EQA_Disabled : FSettings::EQA_Enabled;
			}

			if (GetSettings()->QueueAheadStatus != qaPrev)
			{
				ovr_SetBool(OvrSession, "QueueAheadEnabled", (GetSettings()->QueueAheadStatus == FSettings::EQA_Disabled) ? ovrFalse : ovrTrue);
			}
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
				else if (!FCString::Stricmp(*CmdName, TEXT("MODE"))) 
				{
					FString ModeName = FParse::Token(Cmd, 0);
					int32 i = FCString::Atoi(*ModeName);
					GetSettings()->MirrorWindowMode = FSettings::MirrorWindowModeType(FMath::Clamp(i, 0, (int32)FSettings::eMirrorWindow_Total_));
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
#endif //UE_BUILD_SHIPPING
		else
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (CmdName.StartsWith(TEXT("SET")))
			{
				FString ValueNameStr = FParse::Token(Cmd, 0);
				FString ValueStr = FParse::Token(Cmd, 0);

				ovrBool res = ovrTrue;
				CmdName = CmdName.Replace(TEXT("SET"), TEXT(""));
				if (CmdName.Equals(TEXT("INT"), ESearchCase::IgnoreCase))
				{
					int v = FCString::Atoi(*ValueStr);
					res = ovr_SetInt(OvrSession, TCHAR_TO_ANSI(*ValueNameStr), v);
				}
				else if (CmdName.Equals(TEXT("FLOAT"), ESearchCase::IgnoreCase))
				{
					float v = FCString::Atof(*ValueStr);
					res = ovr_SetFloat(OvrSession, TCHAR_TO_ANSI(*ValueNameStr), v);
				}
				else if (CmdName.Equals(TEXT("BOOL"), ESearchCase::IgnoreCase))
				{
					ovrBool v;
					if (ValueStr == TEXT("0") || ValueStr.Equals(TEXT("false"), ESearchCase::IgnoreCase))
					{
						v = ovrFalse;
					}
					else
					{
						v = ovrTrue;
					}
					res = ovr_SetBool(OvrSession, TCHAR_TO_ANSI(*ValueNameStr), v);
				}
				else if (CmdName.Equals(TEXT("STRING"), ESearchCase::IgnoreCase))
				{
					res = ovr_SetString(OvrSession, TCHAR_TO_ANSI(*ValueNameStr), TCHAR_TO_ANSI(*ValueStr));
				}
#if !UE_BUILD_SHIPPING
				if (!res)
				{
					Ar.Logf(TEXT("HMD parameter %s was not set to value %s"), *ValueNameStr, *ValueStr);
				}
#endif // #if !UE_BUILD_SHIPPING
				return true;
			}
#if !UE_BUILD_SHIPPING
			else if (CmdName.StartsWith(TEXT("GET")))
			{
				FString ValueNameStr = FParse::Token(Cmd, 0);

				CmdName = CmdName.Replace(TEXT("GET"), TEXT(""));
				FString ValueStr;
				if (CmdName.Equals(TEXT("INT"), ESearchCase::IgnoreCase))
				{
					int v = ovr_GetInt(OvrSession, TCHAR_TO_ANSI(*ValueNameStr), 0);
					TCHAR buf[32];
					FCString::Snprintf(buf, sizeof(buf) / sizeof(buf[0]), TEXT("%d"), v);
					ValueStr = buf;
				}
				else if (CmdName.Equals(TEXT("FLOAT"), ESearchCase::IgnoreCase))
				{
					float v = ovr_GetFloat(OvrSession, TCHAR_TO_ANSI(*ValueNameStr), 0);
					TCHAR buf[32];
					FCString::Snprintf(buf, sizeof(buf)/sizeof(buf[0]), TEXT("%f"), v);
					ValueStr = buf;
				}
				else if (CmdName.Equals(TEXT("BOOL"), ESearchCase::IgnoreCase))
				{
					ovrBool v = ovr_GetBool(OvrSession, TCHAR_TO_ANSI(*ValueNameStr), ovrFalse);
					ValueStr = (v == ovrFalse) ? TEXT("false") : TEXT("true");
				}
				else if (CmdName.Equals(TEXT("STRING"), ESearchCase::IgnoreCase))
				{
					ValueStr = ANSI_TO_TCHAR(ovr_GetString(OvrSession, TCHAR_TO_ANSI(*ValueNameStr), ""));
				}
				Ar.Logf(TEXT("HMD parameter %s is set to value %s"), *ValueNameStr, *ValueStr);

				return true;
			}
#endif // #if !UE_BUILD_SHIPPING
		}
	}
	else if (FParse::Command(&Cmd, TEXT("HMDPOS")))
	{
		if (FParse::Command(&Cmd, TEXT("RESETBACK")))
		{
			ovr_ResetBackOfHeadTracking(OvrSession);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("RESETMULTICAM")))
		{
			ovr_ResetMulticameraTracking(OvrSession);
			return true;
		}
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
	else if (FParse::Command(&Cmd, TEXT("OVRVERSION")))
	{
		// deprecated. Use 'hmdversion' instead
		Ar.Logf(*GetVersionString());
		return true;
	}
#if !UE_BUILD_SHIPPING
	else if (FParse::Command(&Cmd, TEXT("TESTL")))
	{
		static uint32 LID1 = 0, LID2 = 0;
		IStereoLayers* StereoL = this;
		check(StereoL);
		if (FParse::Command(&Cmd, TEXT("OFF")))
		{
			if (FParse::Command(&Cmd, TEXT("1")))
			{
				StereoL->DestroyLayer(LID1);
				LID1 = 0;
			}
			else if (FParse::Command(&Cmd, TEXT("2")))
			{
				StereoL->DestroyLayer(LID2);
				LID2 = 0;
			}
			else
			{
				StereoL->DestroyLayer(LID1);
				StereoL->DestroyLayer(LID2);
				LID1 = LID2 = 0;
			}
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("MOD")))
		{
			if (LID2)
			{
				FTransform tr(FRotator(0, -30, 0), FVector(100, 0, 0));
				StereoL->SetTransform(LID2, tr);
				StereoL->SetQuadSize(LID2, FVector2D(25, 25));
			}
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("VP")))
		{
			if (LID1)
			{
				StereoL->SetTextureViewport(LID1, FBox2D(FVector2D(0.25, 0.25), FVector2D(0.75, 0.75)));
			}
			return true;
		}
		const TCHAR* iconPath = TEXT("/Game/Tuscany_OculusCube.Tuscany_OculusCube");
		UE_LOG(LogHMD, Log, TEXT("Loading texture for loading icon %s..."), iconPath);
		UTexture2D* LoadingTexture = LoadObject<UTexture2D>(NULL, iconPath, NULL, LOAD_None, NULL);
		UE_LOG(LogHMD, Log, TEXT("...EEE"));
		if (LoadingTexture != nullptr)
		{
			LoadingTexture->AddToRoot();
			UE_LOG(LogHMD, Log, TEXT("...Success. "));

			if (LID1 == 0)
			{
				LID1 = StereoL->CreateLayer(LoadingTexture, 10);
				FTransform tr(FVector(500, 30, 160));
				StereoL->SetTransform(LID1, tr);
				StereoL->SetQuadSize(LID1, FVector2D(200, 200));
			}

			if (LID2 == 0)
			{
				LID2 = StereoL->CreateLayer(LoadingTexture, 11, true);
				FTransform tr(FRotator(0, 30, 0), FVector(300, 0, 0));
				StereoL->SetTransform(LID2, tr);
				StereoL->SetQuadSize(LID2, FVector2D(100, 100));
			}

#if 0
			uint32 ID;
			auto l = GetLayerManager()->AddLayer(FHMDLayerDesc::Quad, 10, false, ID);
			l->SetTexture(LoadingTexture);
			FTransform tr(FVector(500, 30, 160));
			l->SetTransform(tr);
			l->SetQuadSize(FVector2D(200, 200));

			{
				auto l = GetLayerManager()->AddLayer(FHMDLayerDesc::Quad, 11, true, ID);
				l->SetTexture(LoadingTexture);
				FTransform tr(FRotator(0, 30, 0), FVector(300, 0, 0));
				l->SetTransform(tr);
				l->SetQuadSize(FVector2D(100, 100));
			}
#endif
		}
		return true;
	}
#endif // !UE_BUILD_SHIPPING
	return false;
}

FString FOculusRiftHMD::GetVersionString() const
{
	const char* Results = ovr_GetVersionString();
	FString s = FString::Printf(TEXT("%s, LibOVR: %s, built %s, %s"), *FEngineVersion::Current().ToString(), UTF8_TO_TCHAR(Results),
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
		if (OvrSession)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DeviceName"), FString::Printf(TEXT("%s - %s"), ANSI_TO_TCHAR(HmdDesc.Manufacturer), ANSI_TO_TCHAR(HmdDesc.ProductName))));
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
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("HmdPosTracking"), Settings->Flags.bHmdPosTracking));
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

	// Uncap fps to enable FPS higher than 62
	GEngine->bForceDisableFrameRateSmoothing = bStereo;

	bool wasFullscreenAllowed = IsFullscreenAllowed();
	if (OnOculusStateChange(stereoToBeEnabled))
	{
		Settings->Flags.bStereoEnabled = stereoToBeEnabled;

		if (SceneVP && SceneVP->GetViewportWidget().IsValid())
		{
			if (!IsFullscreenAllowed() && stereoToBeEnabled)
			{
				if (OvrSession)
				{
					// keep window size, but set viewport size to Rift resolution
					SceneVP->SetViewportSize(HmdDesc.Resolution.w, HmdDesc.Resolution.h);
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
								FSlateRect LocalPreFullScreenRect;
								PopPreFullScreenRect(LocalPreFullScreenRect);
								if (LocalPreFullScreenRect.GetSize().X > 0 && LocalPreFullScreenRect.GetSize().Y > 0 && IsFullscreenAllowed())
								{
									Window->MoveWindowTo(FVector2D(LocalPreFullScreenRect.Left, LocalPreFullScreenRect.Top));
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
		return true;
	}
	else
	{
		// Switching to stereo
		InitDevice();

		if (OvrSession)
		{
			Flags.bApplySystemOverridesOnStereo = true;
			UpdateStereoRenderingParams();
			return true;
		}
		DeltaControlRotation = FRotator::ZeroRotator;
	}
	return false;
}

float FOculusRiftHMD::GetVsyncToNextVsync() const
{
	return GetSettings()->VsyncToNextVsync;
}

FPerformanceStats FOculusRiftHMD::GetPerformanceStats() const
{
	return PerformanceStats;
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
			frame->PlayerLocation = ViewLocation;

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
			const FVector vEyePosition = DeltaControlOrientation.RotateVector(HmdToEyeOffset) + frame->Settings->PositionOffset;
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
		orientation = OVR::Quatf(frame->CurHeadPose.Orientation);
	}
	else
	{
		const ovrTrackingState ss = ovr_GetTrackingState(OvrSession, ovr_GetTimeInSeconds(), false);
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
		orientation = OVR::Quatf(frame->CurHeadPose.Orientation);
		Settings->BaseOffset = ToFVector(frame->CurHeadPose.Position);
	}
	else
	{
		const ovrTrackingState ss = ovr_GetTrackingState(OvrSession, ovr_GetTimeInSeconds(), false);
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
	InViewFamily.EngineShowFlags.HMDDistortion = false;
	InViewFamily.EngineShowFlags.StereoRendering = IsStereoEnabled();
}

void FOculusRiftHMD::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	auto frame = GetFrame();
	check(frame);

	InView.BaseHmdOrientation = frame->LastHmdOrientation;
	InView.BaseHmdLocation = frame->LastHmdPosition;

	InViewFamily.bUseSeparateRenderTarget = ShouldUseSeparateRenderTarget();

	const int eyeIdx = (InView.StereoPass == eSSP_LEFT_EYE) ? 0 : 1;

	InView.ViewRect = frame->GetSettings()->EyeRenderViewport[eyeIdx];

	frame->CachedViewRotation[eyeIdx] = InView.ViewRotation;
}

bool FOculusRiftHMD::IsHeadTrackingAllowed() const
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		UEditorEngine* EdEngine = Cast<UEditorEngine>(GEngine);
		return OvrSession && (!EdEngine || EdEngine->bUseVRPreviewForPlayWorld || GetDefault<ULevelEditorPlaySettings>()->ViewportGetsHMDControl) &&	(Settings->Flags.bHeadTrackingEnforced || GEngine->IsStereoscopic3D());
	}
#endif//WITH_EDITOR
	return OvrSession && FHeadMountedDisplay::IsHeadTrackingAllowed();
}

//---------------------------------------------------
// Oculus Rift Specific
//---------------------------------------------------

FOculusRiftHMD::FOculusRiftHMD()
	:
	  OvrSession(nullptr)
{
	OCFlags.Raw = 0;
	DeltaControlRotation = FRotator::ZeroRotator;
	HmdDesc.Type = ovrHmd_None;

	Settings = MakeShareable(new FSettings);

	if (GIsEditor)
	{
		Settings->Flags.bOverrideScreenPercentage = true;
		Settings->ScreenPercentage = 100;
	}
	RendererModule = nullptr;
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
	HmdDesc.Type = ovrHmd_None;

	Settings->Flags.InitStatus |= FSettings::eStartupExecuted;

	check(!DetectHmd);
	DetectHmd = new FDetectHmd(this, FParse::Param(FCommandLine::Get(), TEXT("forcedrift")));

	// If there is no HMD detected at startup, don't continue, and allow other devices to be detected
	if (!DetectHmd->OculusRiftHMD || !DetectHmd->OculusRiftHMD->HmdDetected)
	{
		Settings->Flags.InitStatus = FSettings::eNotInitialized;
		return;
	}

	if (GIsEditor)
	{
		Settings->Flags.bHeadTrackingEnforced = true;
	}

#if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)
	if (IsPCPlatform(GMaxRHIShaderPlatform) && !IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		check(!pCustomPresent.GetReference())
		pCustomPresent = new D3D11Bridge(OvrSession);
	}
#endif
#if defined(OVR_GL)
	if (IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		check(!pCustomPresent.GetReference())
		pCustomPresent = new OGLBridge(OvrSession);
	}
#else
	if (IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		UE_LOG(LogHMD, Warning, TEXT("OpenGL is not currently supported by OculusRiftHMD plugin"));
		return;
	}
#endif

	Settings->Flags.InitStatus |= FSettings::eInitialized;

	UE_LOG(LogHMD, Log, TEXT("Oculus plugin initialized. Version: %s"), *GetVersionString());

	// grab a pointer to the renderer module for displaying our mirror window
	static const FName RendererModuleName("Renderer");
	RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);

	const bool bForcedVR = FParse::Param(FCommandLine::Get(), TEXT("vr"));
	if (bForcedVR)
	{
		Flags.bNeedEnableStereo = true;
	}
}

void FOculusRiftHMD::Shutdown()
{
	if (!(Settings->Flags.InitStatus & FSettings::eInitialized))
	{
		return;
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(ShutdownRen,
	FOculusRiftHMD*, Plugin, this,
	{
		Plugin->ShutdownRendering();
	});
	FlushRenderingCommands();

	ReleaseDevice();
	
	DetectHmd = nullptr;
	Settings = nullptr;
	Frame = nullptr;
	RenderFrame = nullptr;
}

bool FOculusRiftHMD::InitDevice()
{
	if (OvrSession)
	{
		const ovrTrackingState ss = ovr_GetTrackingState(OvrSession, ovr_GetTimeInSeconds(), false);
		if (!(ss.StatusFlags & ovrStatus_HmdConnected))
		{
			ReleaseDevice();
		}
		else
		{
			return true; // already created and present
		}
	}
	check(!OvrSession);

	FSettings* CurrentSettings = GetSettings();

	HmdDesc.Type = ovrHmd_None;

	ovrGraphicsLuid luid;
	ovrResult result = ovr_Create(&OvrSession, &luid);
	if (OVR_SUCCESS(result) && OvrSession)
	{
		SetHmdGraphicsAdapter(luid);

		if(IsRHIUsingHmdGraphicsAdapter(luid))
		{
			HmdDetected = true;
			HmdDesc = ovr_GetHmdDesc(OvrSession);

			if (pCustomPresent)
			{
				pCustomPresent->SetHmd(OvrSession);
			}
			CurrentSettings->SupportedHmdCaps = HmdDesc.AvailableHmdCaps;
			CurrentSettings->SupportedTrackingCaps = HmdDesc.AvailableTrackingCaps;

			CurrentSettings->TrackingCaps = HmdDesc.DefaultTrackingCaps;
			CurrentSettings->HmdCaps = HmdDesc.DefaultHmdCaps;

			CurrentSettings->Flags.bHmdPosTracking = (CurrentSettings->SupportedTrackingCaps & ovrTrackingCap_Position) != 0;

			LoadFromIni();

			UpdateDistortionCaps();
			UpdateHmdRenderInfo();
			UpdateStereoRenderingParams();
			UpdateHmdCaps();

			if (!HasHiddenAreaMesh())
			{
				SetupOcclusionMeshes();
			}

			if (CurrentSettings->QueueAheadStatus != FSettings::EQA_Default)
			{
				ovr_SetBool(OvrSession, "QueueAheadEnabled", (GetSettings()->QueueAheadStatus == FSettings::EQA_Disabled) ? ovrFalse : ovrTrue);
			}
		}
		else
		{
			// UNDONE Message that you need to restart application to use correct adapter
			ovr_Destroy(OvrSession);
			OvrSession = nullptr;

			// HMD is not in a usable state.  Mark it as not detected, and stop trying to detect it.
			DetectHmd = nullptr;
			HmdDetected = false;
		}
	}
	else
	{
		// Create failed.  Mark it as not detected, but keep trying to detect it, in case it gets reconnected later.
		HmdDetected = false;
		UE_LOG(LogHMD, Log, TEXT("HMD can't be initialized, err = %d"), int(result));
	}

	return OvrSession != nullptr;
}

void FOculusRiftHMD::ReleaseDevice()
{
	if (OvrSession)
	{
		SaveToIni();


		// Wait for all resources to be released

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(ResetRen,
		FOculusRiftHMD*, Plugin, this,
		{
			if (Plugin->pCustomPresent)
			{
				Plugin->pCustomPresent->Reset();
			}
		});

		// Wait for all resources to be released
		FlushRenderingCommands();

		ovr_Destroy(OvrSession);
		OvrSession = nullptr;
		FMemory::Memzero(HmdDesc);
	}
}

void FOculusRiftHMD::SetupOcclusionMeshes()
{
	if (HmdDesc.Type == ovrHmdType::ovrHmd_DK2)
	{
		HiddenAreaMeshes[0].BuildMesh(DK2_LeftEyeHiddenAreaPositions, HiddenAreaVertexCount, FHMDViewMesh::MT_HiddenArea);
		HiddenAreaMeshes[1].BuildMesh(DK2_RightEyeHiddenAreaPositions, HiddenAreaVertexCount, FHMDViewMesh::MT_HiddenArea);
		VisibleAreaMeshes[0].BuildMesh(DK2_LeftEyeVisibleAreaPositions, VisibleAreaVertexCount, FHMDViewMesh::MT_VisibleArea);
		VisibleAreaMeshes[1].BuildMesh(DK2_RightEyeVisibleAreaPositions, VisibleAreaVertexCount, FHMDViewMesh::MT_VisibleArea);
	}
	else if (HmdDesc.Type == ovrHmdType::ovrHmd_CB)
	{
		HiddenAreaMeshes[0].BuildMesh(CB_LeftEyeHiddenAreaPositions, HiddenAreaVertexCount, FHMDViewMesh::MT_HiddenArea);
		HiddenAreaMeshes[1].BuildMesh(CB_RightEyeHiddenAreaPositions, HiddenAreaVertexCount, FHMDViewMesh::MT_HiddenArea);
		VisibleAreaMeshes[0].BuildMesh(CB_LeftEyeVisibleAreaPositions, VisibleAreaVertexCount, FHMDViewMesh::MT_VisibleArea);
		VisibleAreaMeshes[1].BuildMesh(CB_RightEyeVisibleAreaPositions, VisibleAreaVertexCount, FHMDViewMesh::MT_VisibleArea);
	}
	else if (HmdDesc.Type == ovrHmdType::ovrHmd_E3_2015 || HmdDesc.Type == ovrHmdType::ovrHmd_ES06)
	{
		HiddenAreaMeshes[0].BuildMesh(EVT_LeftEyeHiddenAreaPositions, HiddenAreaVertexCount, FHMDViewMesh::MT_HiddenArea);
		HiddenAreaMeshes[1].BuildMesh(EVT_RightEyeHiddenAreaPositions, HiddenAreaVertexCount, FHMDViewMesh::MT_HiddenArea);
		VisibleAreaMeshes[0].BuildMesh(EVT_LeftEyeVisibleAreaPositions, VisibleAreaVertexCount, FHMDViewMesh::MT_VisibleArea);
		VisibleAreaMeshes[1].BuildMesh(EVT_RightEyeVisibleAreaPositions, VisibleAreaVertexCount, FHMDViewMesh::MT_VisibleArea);
	}
}

void FOculusRiftHMD::UpdateHmdCaps()
{
	if (OvrSession)
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

		ovr_SetEnabledCaps(OvrSession, CurrentSettings->HmdCaps);

		ovr_ConfigureTracking(OvrSession, CurrentSettings->TrackingCaps, 0);
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
	check(OvrSession);

	UE_LOG(LogHMD, Log, TEXT("HMD %s, res = %d x %d"), ANSI_TO_TCHAR(HmdDesc.ProductName), 
		HmdDesc.Resolution.w, HmdDesc.Resolution.h); 

	FSettings* CurrentSettings = GetSettings();

	// Calc FOV
	if (!CurrentSettings->Flags.bOverrideFOV)
	{
		// Calc FOV, symmetrical, for each eye. 
		CurrentSettings->EyeFov[0] = HmdDesc.DefaultEyeFov[0];
		CurrentSettings->EyeFov[1] = HmdDesc.DefaultEyeFov[1];

		// Calc FOV in radians
		CurrentSettings->VFOVInRadians = FMath::Max(GetVerticalFovRadians(CurrentSettings->EyeFov[0]), GetVerticalFovRadians(CurrentSettings->EyeFov[1]));
		CurrentSettings->HFOVInRadians = FMath::Max(GetHorizontalFovRadians(CurrentSettings->EyeFov[0]), GetHorizontalFovRadians(CurrentSettings->EyeFov[1]));
	}

	const ovrSizei recommenedTex0Size = ovr_GetFovTextureSize(OvrSession, ovrEye_Left, CurrentSettings->EyeFov[0], 1.0f);
	const ovrSizei recommenedTex1Size = ovr_GetFovTextureSize(OvrSession, ovrEye_Right, CurrentSettings->EyeFov[1], 1.0f);

	ovrSizei idealRenderTargetSize;
	idealRenderTargetSize.w = recommenedTex0Size.w + recommenedTex1Size.w;
	idealRenderTargetSize.h = FMath::Max(recommenedTex0Size.h, recommenedTex1Size.h);

	CurrentSettings->IdealScreenPercentage = FMath::Max(float(idealRenderTargetSize.w) / float(HmdDesc.Resolution.w) * 100.f,
														float(idealRenderTargetSize.h) / float(HmdDesc.Resolution.h) * 100.f);

	// Override eye distance by the value from HMDInfo (stored in Profile).
	if (!CurrentSettings->Flags.bOverrideIPD)
	{
		CurrentSettings->InterpupillaryDistance = ovr_GetFloat(OvrSession, OVR_KEY_IPD, OVR_DEFAULT_IPD);
	}

	// cache eye to neck distance.
	float neck2eye[2] = { OVR_DEFAULT_NECK_TO_EYE_HORIZONTAL, OVR_DEFAULT_NECK_TO_EYE_VERTICAL };
	ovr_GetFloatArray(OvrSession, OVR_KEY_NECK_TO_EYE_DISTANCE, neck2eye, 2);
	CurrentSettings->NeckToEyeInMeters = FVector2D(neck2eye[0], neck2eye[1]);

	// cache VsyncToNextVsync value
	CurrentSettings->VsyncToNextVsync = ovr_GetFloat(OvrSession, "VsyncToNextVsync", 0.0f);

	// Default texture size (per eye) is equal to half of W x H resolution. Will be overridden in SetupView.
	//CurrentSettings->SetViewportSize(HmdDesc.Resolution.w / 2, HmdDesc.Resolution.h);

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
	if (IsInitialized() && OvrSession)
	{
		//!AB: note, for Direct Rendering EyeRenderDesc is calculated twice, once
		// here and another time in BeginRendering_RenderThread. I need to have EyeRenderDesc
		// on a game thread for ViewAdjust (for StereoViewOffset calculation).
		CurrentSettings->EyeRenderDesc[0] = ovr_GetRenderDesc(OvrSession, ovrEye_Left, CurrentSettings->EyeFov[0]);
		CurrentSettings->EyeRenderDesc[1] = ovr_GetRenderDesc(OvrSession, ovrEye_Right, CurrentSettings->EyeFov[1]);
		if (CurrentSettings->Flags.bOverrideIPD)
		{
			CurrentSettings->EyeRenderDesc[0].HmdToEyeViewOffset.x = -CurrentSettings->InterpupillaryDistance * 0.5f;
			CurrentSettings->EyeRenderDesc[1].HmdToEyeViewOffset.x = CurrentSettings->InterpupillaryDistance * 0.5f;
		}

		const unsigned int ProjModifiers = ovrProjection_None; //@TODO revise to use ovrProjection_FarClipAtInfinity and/or ovrProjection_FarLessThanNear
		// Far and Near clipping planes will be modified in GetStereoProjectionMatrix()
		CurrentSettings->EyeProjectionMatrices[0] = ovrMatrix4f_Projection(CurrentSettings->EyeFov[0], 0.01f, 10000.0f, ProjModifiers);
		CurrentSettings->EyeProjectionMatrices[1] = ovrMatrix4f_Projection(CurrentSettings->EyeFov[1], 0.01f, 10000.0f, ProjModifiers);

		CurrentSettings->PerspectiveProjection[0] = ovrMatrix4f_Projection(CurrentSettings->EyeFov[0], 0.01f, 10000.f, ProjModifiers | ovrProjection_RightHanded);
		CurrentSettings->PerspectiveProjection[1] = ovrMatrix4f_Projection(CurrentSettings->EyeFov[1], 0.01f, 10000.f, ProjModifiers | ovrProjection_RightHanded);

		if (CurrentSettings->PixelDensity == 0.f)
		{
			check(CurrentSettings->IdealScreenPercentage > 0 && CurrentSettings->ScreenPercentage > 0);
			// calculate PixelDensity using ScreenPercentage and IdealScreenPercentage
			float pd = CurrentSettings->ScreenPercentage / CurrentSettings->IdealScreenPercentage;
			CurrentSettings->PixelDensity = pd;
		}

		const ovrSizei recommenedTex0Size = ovr_GetFovTextureSize(OvrSession, ovrEye_Left, CurrentSettings->EyeFov[0], CurrentSettings->PixelDensity);
		const ovrSizei recommenedTex1Size = ovr_GetFovTextureSize(OvrSession, ovrEye_Right, CurrentSettings->EyeFov[1], CurrentSettings->PixelDensity);
		const float texturePadding = CurrentSettings->GetTexturePaddingPerEye();
		CurrentSettings->RenderTargetSize.X = recommenedTex0Size.w + recommenedTex1Size.w + texturePadding*2;
		CurrentSettings->RenderTargetSize.Y = FMath::Max(recommenedTex0Size.h, recommenedTex1Size.h);
		FSceneRenderTargets::QuantizeBufferSize(CurrentSettings->RenderTargetSize.X, CurrentSettings->RenderTargetSize.Y);
		UE_CLOG(CurrentSettings->RenderTargetSize.X < 200 || CurrentSettings->RenderTargetSize.X > 10000 || CurrentSettings->RenderTargetSize.Y < 200 || CurrentSettings->RenderTargetSize.Y > 10000,
			LogHMD, Warning, 
			TEXT("The calculated render target size (%d x %d) looks strange. Are PixelDensity (%f) and EyeFov[0] (%f x %f) and EyeFov[1] (%f x %f) correct?"), 
			CurrentSettings->RenderTargetSize.X, CurrentSettings->RenderTargetSize.Y, float(CurrentSettings->PixelDensity), float(CurrentSettings->EyeFov[0].LeftTan + CurrentSettings->EyeFov[0].RightTan), float(CurrentSettings->EyeFov[0].UpTan + CurrentSettings->EyeFov[0].DownTan), 
			float(CurrentSettings->EyeFov[1].LeftTan + CurrentSettings->EyeFov[1].RightTan), float(CurrentSettings->EyeFov[1].UpTan + CurrentSettings->EyeFov[1].DownTan));

		const int32 RTSizeX = CurrentSettings->RenderTargetSize.X;
		const int32 RTSizeY = CurrentSettings->RenderTargetSize.Y;
		CurrentSettings->EyeRenderViewport[0] = FIntRect(0, 0, RTSizeX/2 - texturePadding, RTSizeY);
		CurrentSettings->EyeRenderViewport[1] = FIntRect(RTSizeX/2 + texturePadding, 0, RTSizeX, RTSizeY);

		Flags.bNeedUpdateStereoRenderingParams = false;
	}
}

void FOculusRiftHMD::LoadFromIni()
{
	const TCHAR* OculusSettings = TEXT("Oculus.Settings");
	bool v;
	float f;
	int i;
	FVector vec;
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
				check(!FMath::IsNaN(f));
				SetInterpupillaryDistance(FMath::Clamp(f, 0.0f, 1.0f));
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
				check(!FMath::IsNaN(f));
				Settings->HFOVInRadians = FMath::Clamp(f, FMath::DegreesToRadians(45), FMath::DegreesToRadians(200));
			}
			if (GConfig->GetFloat(OculusSettings, TEXT("VFOV"), f, GEngineIni))
			{
				check(!FMath::IsNaN(f));
				Settings->VFOVInRadians = FMath::Clamp(f, FMath::DegreesToRadians(45), FMath::DegreesToRadians(200));
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
	if (GConfig->GetFloat(OculusSettings, TEXT("PixelDensity"), f, GEngineIni))
	{
		check(!FMath::IsNaN(f));
		GetSettings()->PixelDensity = FMath::Clamp(f, 0.3f, 2.0f);
	}
	if (GConfig->GetInt(OculusSettings, TEXT("QueueAheadEnabled2"), i, GEngineIni))
	{
		if (i < FSettings::EQueueAheadStatus::EQA_Default || i > FSettings::EQueueAheadStatus::EQA_Disabled)
		{
			i = FSettings::EQueueAheadStatus::EQA_Default;
		}
		GetSettings()->QueueAheadStatus = FSettings::EQueueAheadStatus(i);
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
		check(!FMath::IsNaN(f));
		if (f < 0)
		{
			f = 0;
		}
		Settings->FarClippingPlane = f;
	}
	if (GConfig->GetFloat(OculusSettings, TEXT("NearClippingPlane"), f, GEngineIni))
	{
		check(!FMath::IsNaN(f));
		if (f < 0)
		{
			f = 0;
		}
		Settings->NearClippingPlane = f;
	}
	if (GConfig->GetVector(OculusSettings, TEXT("MirrorWindowSize"), vec, GEngineIni))
	{
		Settings->MirrorWindowSize = FIntPoint(FMath::Clamp(int(vec.X), 0, 5000), FMath::Clamp(int(vec.Y), 0, 5000));
	}
	if (GConfig->GetInt(OculusSettings, TEXT("MirrorWindowMode"), i, GEngineIni))
	{
		if (i < 0)
		{
			GetSettings()->MirrorWindowMode = FSettings::MirrorWindowModeType(FMath::Clamp(-i, 0, (int)FSettings::eMirrorWindow_Total_));
			GetSettings()->Flags.bMirrorToWindow = false;
		}
		else
		{
			GetSettings()->MirrorWindowMode = FSettings::MirrorWindowModeType(FMath::Clamp(i, 0, (int)FSettings::eMirrorWindow_Total_));
			GetSettings()->Flags.bMirrorToWindow = true;
		}
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
	if (GConfig->GetFloat(OculusSettings, TEXT("SideOfSingleCubeInMeters"), f, GEngineIni))
	{
		check(!FMath::IsNaN(f));
		SideOfSingleCubeInMeters = f;
	}
	if (GConfig->GetFloat(OculusSettings, TEXT("SeaOfCubesVolumeSizeInMeters"), f, GEngineIni))
	{
		check(!FMath::IsNaN(f));
		SeaOfCubesVolumeSizeInMeters = f;
	}
	if (GConfig->GetInt(OculusSettings, TEXT("NumberOfCubesInOneSide"), i, GEngineIni))
	{
		NumberOfCubesInOneSide = i;
	}
	if (GConfig->GetVector(OculusSettings, TEXT("CenterOffsetInMeters"), vec, GEngineIni))
	{
		check(!FMath::IsNaN(vec.X) && !FMath::IsNaN(vec.Y));
		CenterOffsetInMeters = vec;
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

	GConfig->SetFloat(OculusSettings, TEXT("PixelDensity"), GetSettings()->PixelDensity, GEngineIni);

	GConfig->SetInt(OculusSettings, TEXT("QueueAheadEnabled2"), GetSettings()->QueueAheadStatus, GEngineIni);

	GConfig->SetBool(OculusSettings, TEXT("bLowPersistenceMode"), Settings->Flags.bLowPersistenceMode, GEngineIni);

	GConfig->SetBool(OculusSettings, TEXT("bUpdateOnRT"), Settings->Flags.bUpdateOnRT, GEngineIni);

	if (Settings->Flags.bClippingPlanesOverride)
	{
		GConfig->SetFloat(OculusSettings, TEXT("FarClippingPlane"), Settings->FarClippingPlane, GEngineIni);
		GConfig->SetFloat(OculusSettings, TEXT("NearClippingPlane"), Settings->NearClippingPlane, GEngineIni);
	}

	if (Settings->Flags.bMirrorToWindow)
	{
		GConfig->SetInt(OculusSettings, TEXT("MirrorWindowMode"), GetSettings()->MirrorWindowMode, GEngineIni);
	}
	else
	{
		GConfig->SetInt(OculusSettings, TEXT("MirrorWindowMode"), -GetSettings()->MirrorWindowMode, GEngineIni);
	}
	GConfig->SetVector(OculusSettings, TEXT("MirrorWindowSize"), FVector(Settings->MirrorWindowSize.X, Settings->MirrorWindowSize.Y, 0), GEngineIni);
}

bool FOculusRiftHMD::HandleInputKey(UPlayerInput* pPlayerInput,
	const FKey& Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
	return false;
}

void FOculusRiftHMD::OnBeginPlay()
{
	CachedViewportWidget.Reset();
	CachedWindow.Reset();

	// @TODO: add more values here.
	// This call make sense when 'Play' is used from the Editor;
	if (GIsEditor)
	{
		Settings->PositionOffset = FVector::ZeroVector;
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
	if (OvrSession)
	{
		const ovrTrackingState ss = ovr_GetTrackingState(OvrSession, ovr_GetTimeInSeconds(), false);
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
	if (OvrSession)
	{
		OutProfile.Name = ovr_GetString(OvrSession, OVR_KEY_USER, "");
		OutProfile.Gender = ovr_GetString(OvrSession, OVR_KEY_GENDER, OVR_DEFAULT_GENDER);
		OutProfile.PlayerHeight = ovr_GetFloat(OvrSession, OVR_KEY_PLAYER_HEIGHT, OVR_DEFAULT_PLAYER_HEIGHT);
		OutProfile.EyeHeight = ovr_GetFloat(OvrSession, OVR_KEY_EYE_HEIGHT, OVR_DEFAULT_EYE_HEIGHT);
		OutProfile.IPD = ovr_GetFloat(OvrSession, OVR_KEY_IPD, OVR_DEFAULT_IPD);
		float neck2eye[2] = {OVR_DEFAULT_NECK_TO_EYE_HORIZONTAL, OVR_DEFAULT_NECK_TO_EYE_VERTICAL};
		ovr_GetFloatArray(OvrSession, OVR_KEY_NECK_TO_EYE_DISTANCE, neck2eye, 2);
		OutProfile.NeckToEyeDistance = FVector2D(neck2eye[0], neck2eye[1]);
		OutProfile.ExtraFields.Reset();

		int EyeRelief = ovr_GetInt(OvrSession, OVR_KEY_EYE_RELIEF_DIAL, OVR_DEFAULT_EYE_RELIEF_DIAL);
		OutProfile.ExtraFields.Add(FString(OVR_KEY_EYE_RELIEF_DIAL)) = FString::FromInt(EyeRelief);
		
		float eye2nose[2] = { 0.f, 0.f };
		ovr_GetFloatArray(OvrSession, OVR_KEY_EYE_TO_NOSE_DISTANCE, eye2nose, 2);
		OutProfile.ExtraFields.Add(FString(OVR_KEY_EYE_RELIEF_DIAL"Horizontal")) = FString::SanitizeFloat(eye2nose[0]);
		OutProfile.ExtraFields.Add(FString(OVR_KEY_EYE_RELIEF_DIAL"Vertical")) = FString::SanitizeFloat(eye2nose[1]);

		float maxEye2Plate[2] = { 0.f, 0.f };
		ovr_GetFloatArray(OvrSession, OVR_KEY_MAX_EYE_TO_PLATE_DISTANCE, maxEye2Plate, 2);
		OutProfile.ExtraFields.Add(FString(OVR_KEY_MAX_EYE_TO_PLATE_DISTANCE"Horizontal")) = FString::SanitizeFloat(maxEye2Plate[0]);
		OutProfile.ExtraFields.Add(FString(OVR_KEY_MAX_EYE_TO_PLATE_DISTANCE"Vertical")) = FString::SanitizeFloat(maxEye2Plate[1]);

		OutProfile.ExtraFields.Add(FString(ovr_GetString(OvrSession, OVR_KEY_EYE_CUP, "")));
		return true;
	}
	return false; 
}

void FOculusRiftHMD::ApplySystemOverridesOnStereo(bool force)
{
	check(IsInGameThread());
	// ALWAYS SET r.FinishCurrentFrame to 0! Otherwise the perf might be poor.
	// @TODO: revise the FD3D11DynamicRHI::RHIEndDrawingViewport code (and other renderers)
	// to ignore this var completely.
 	static const auto CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
 	CFinishFrameVar->Set(0);
}

//////////////////////////////////////////////////////////////////////////
FViewExtension::FViewExtension(FHeadMountedDisplay* InDelegate) 
	: FHMDViewExtension(InDelegate)
	, ShowFlags(ESFIM_All0)
	, bFrameBegun(false)
{
	auto OculusHMD = static_cast<FOculusRiftHMD*>(InDelegate);
	OvrSession = OculusHMD->OvrSession;

	pPresentBridge = OculusHMD->pCustomPresent;
}

#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS
#endif //#if !PLATFORM_MAC

