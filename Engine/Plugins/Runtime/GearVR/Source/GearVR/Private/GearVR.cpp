// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HMDPrivatePCH.h"
#include "GearVR.h"
#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "Android/AndroidJNI.h"
#include "RHIStaticStates.h"

#include "VrApi_Helpers.h"

#define DEFAULT_PREDICTION_IN_SECONDS 0.035

#if !UE_BUILD_SHIPPING
// Should be changed to CAPI when available.
#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif
#include "../Src/Kernel/OVR_Log.h"
#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

#define GL_CHECK_ERROR \
do \
{ \
int err; \
	while ((err = glGetError()) != GL_NO_ERROR) \
	{ \
		UE_LOG(LogHMD, Warning, TEXT("%s:%d GL error (#%x)\n"), ANSI_TO_TCHAR(__FILE__), __LINE__, err); \
	} \
} while (0) 

#else // #if !UE_BUILD_SHIPPING
#define GL_CHECK_ERROR (void)0
#endif // #if !UE_BUILD_SHIPPING


// call out to JNI to see if the application was packaged for GearVR
extern bool AndroidThunkCpp_IsGearVRApplication();


//---------------------------------------------------
// GearVR Plugin Implementation
//---------------------------------------------------

class FGearVRPlugin : public IGearVRPlugin
{
	/** IHeadMountedDisplayModule implementation */
	virtual TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > CreateHeadMountedDisplay() override;

	// Pre-init the HMD module
	virtual void PreInit() override;

	FString GetModulePriorityKeyName() const
	{
		return FString(TEXT("GearVR"));
	}
};

IMPLEMENT_MODULE( FGearVRPlugin, GearVR )

TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > FGearVRPlugin::CreateHeadMountedDisplay()
{
#if GEARVR_SUPPORTED_PLATFORMS
	if (!AndroidThunkCpp_IsGearVRApplication())
	{
		// don't do anything if we aren't packaged for GearVR
		return NULL;
	}

	TSharedPtr< FGearVR, ESPMode::ThreadSafe > GearVR(new FGearVR());
	if (GearVR->IsInitialized())
	{
		return GearVR;
	}
#endif//GEARVR_SUPPORTED_PLATFORMS
	return NULL;
}

void FGearVRPlugin::PreInit()
{
#if GEARVR_SUPPORTED_PLATFORMS
	if (!AndroidThunkCpp_IsGearVRApplication())
	{
		// don't do anything if we aren't packaged for GearVR
		return;
	}

	FGearVR::PreInit();
#endif//GEARVR_SUPPORTED_PLATFORMS
}


#if GEARVR_SUPPORTED_PLATFORMS
FSettings::FSettings()
	: RenderTargetWidth(2048)
	, RenderTargetHeight(1024)
	, MotionPredictionInSeconds(DEFAULT_PREDICTION_IN_SECONDS)
	, HeadModel(0.12f, 0.0f, 0.17f)
{
	HFOVInRadians = FMath::DegreesToRadians(90.f);
	VFOVInRadians = FMath::DegreesToRadians(90.f);
	HmdToEyeViewOffset[0] = HmdToEyeViewOffset[1] = OVR::Vector3f(0,0,0);
	IdealScreenPercentage = ScreenPercentage = SavedScrPerc = 100.f;
	InterpupillaryDistance = OVR_DEFAULT_IPD;
	FMemory::Memzero(VrModeParms);

	Flags.bStereoEnabled = Flags.bHMDEnabled = true;
	Flags.bUpdateOnRT = Flags.bTimeWarp = true;
}

TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> FSettings::Clone() const
{
	TSharedPtr<FSettings, ESPMode::ThreadSafe> NewSettings = MakeShareable(new FSettings(*this));
	return NewSettings;
}

//////////////////////////////////////////////////////////////////////////
FGameFrame::FGameFrame()
{
	FMemory::Memzero(CurEyeRenderPose);
	FMemory::Memzero(CurSensorState);
	FMemory::Memzero(EyeRenderPose);
	FMemory::Memzero(HeadPose);
	FMemory::Memzero(TanAngleMatrix);
}

TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> FGameFrame::Clone() const
{
	TSharedPtr<FGameFrame, ESPMode::ThreadSafe> NewFrame = MakeShareable(new FGameFrame(*this));
	return NewFrame;
}


//---------------------------------------------------
// GearVR IHeadMountedDisplay Implementation
//---------------------------------------------------

void FGearVR::PreInit()
{
	// ignore the first call as it's from the engine PreInit, we need to handle the second call which is from the Java UI thread
	static int NumCalls = 0;

	if (++NumCalls == 2)
	{
		ovr_OnLoad(GJavaVM);
		ovr_Init();
	}
}

TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> FGearVR::CreateNewGameFrame() const
{
	TSharedPtr<FGameFrame, ESPMode::ThreadSafe> Result(MakeShareable(new FGameFrame()));
	return Result;
}

TSharedPtr<FHMDSettings, ESPMode::ThreadSafe> FGearVR::CreateNewSettings() const
{
	TSharedPtr<FSettings, ESPMode::ThreadSafe> Result(MakeShareable(new FSettings()));
	return Result;
}

bool FGearVR::OnStartGameFrame()
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

	rv = GetEyePoses(*CurrentFrame, CurrentFrame->CurEyeRenderPose, CurrentFrame->CurSensorState);

	return rv;
}

FGameFrame* FGearVR::GetFrame() const
{
	return static_cast<FGameFrame*>(GetCurrentFrame());
}

EHMDDeviceType::Type FGearVR::GetHMDDeviceType() const
{
	return EHMDDeviceType::DT_GearVR;
}

bool FGearVR::GetHMDMonitorInfo(MonitorInfo& MonitorDesc)
{
	MonitorDesc.MonitorName = "";
	MonitorDesc.MonitorId = 0;
	MonitorDesc.DesktopX = MonitorDesc.DesktopY = 0;
	MonitorDesc.ResolutionX = GetSettings()->RenderTargetWidth;
	MonitorDesc.ResolutionY = GetSettings()->RenderTargetHeight;
	return true;
}

bool FGearVR::IsInLowPersistenceMode() const
{
	return true;
}

bool FGearVR::GetEyePoses(const FGameFrame& InFrame, ovrPosef outEyePoses[2], ovrSensorState& outSensorState)
{
	FScopeLock lock(&OvrMobileLock);
	if (!OvrMobile_RT)
	{
		return false;
	}

	double predictedTime = 0.0;
	const double now = ovr_GetTimeInSeconds();
	if (IsInGameThread())
	{
		if (OCFlags.NeedResetOrientationAndPosition)
		{
			ResetOrientationAndPosition(ResetToYaw);
		}

		// Get the latest head tracking state, predicted ahead to the midpoint of the time
		// it will be displayed.  It will always be corrected to the real values by
		// time warp, but the closer we get, the less black will be pulled in at the edges.
		static double prev;
		const double rawDelta = now - prev;
		prev = now;
		const double clampedPrediction = FMath::Min( 0.1, rawDelta * 2 );
		predictedTime = now + clampedPrediction;

		//UE_LOG(LogHMD, Log, TEXT("GT Frame %d, predicted time: %.6f, delta %.6f"), InFrame.FrameNumber, (float)(clampedPrediction), float(rawDelta));
	}
	else if (IsInRenderingThread())
	{
		predictedTime = ovr_GetPredictedDisplayTime(OvrMobile_RT, 1, 1);
		//UE_LOG(LogHMD, Log, TEXT("RT Frame %d, predicted time: %.6f"), InFrame.FrameNumber, (float)(predictedTime - now));
	}
	outSensorState = ovr_GetPredictedSensorState(OvrMobile_RT, predictedTime);

	OVR::Posef hmdPose = (OVR::Posef)outSensorState.Predicted.Pose;
	OVR::Vector3f transl0 = hmdPose.Orientation.Rotate(-((OVR::Vector3f)InFrame.GetSettings()->HmdToEyeViewOffset[0])) + hmdPose.Position;
	OVR::Vector3f transl1 = hmdPose.Orientation.Rotate(-((OVR::Vector3f)InFrame.GetSettings()->HmdToEyeViewOffset[1])) + hmdPose.Position;

	// Currently HmdToEyeViewOffset is only a 3D vector
	// (Negate HmdToEyeViewOffset because offset is a view matrix offset and not a camera offset)
	outEyePoses[0].Orientation = outEyePoses[1].Orientation = outSensorState.Predicted.Pose.Orientation;
	outEyePoses[0].Position = transl0;
	outEyePoses[1].Position = transl1;
	return true;
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

void FGearVR::GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition)
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
	//if (PositionScale != FVector::ZeroVector)
	//{
	//	frame->CameraScale3D = PositionScale;
	//	frame->Flags.bCameraScale3DAlreadySet = true;
	//}
	const bool bUseOrienationForPlayerCamera = false, bUsePositionForPlayerCamera = false;
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

FVector FGearVR::GetNeckPosition(const FQuat& CurrentOrientation, const FVector& CurrentPosition, const FVector& PositionScale)
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

void FGearVR::GetCurrentPose(FQuat& CurrentHmdOrientation, FVector& CurrentHmdPosition, bool bUseOrienationForPlayerCamera, bool bUsePositionForPlayerCamera)
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
		frame->HeadPose = frame->CurSensorState.Predicted.Pose;
	}

	frame->PoseToOrientationAndPosition(frame->CurSensorState.Predicted.Pose, CurrentHmdOrientation, CurrentHmdPosition);
	//UE_LOG(LogHMD, Log, TEXT("CRPOSE: Pos %.3f %.3f %.3f"), CurrentHmdPosition.X, CurrentHmdPosition.Y, CurrentHmdPosition.Z);
	//UE_LOG(LogHMD, Log, TEXT("CRPOSE: Yaw %.3f Pitch %.3f Roll %.3f"), CurrentHmdOrientation.Rotator().Yaw, CurrentHmdOrientation.Rotator().Pitch, CurrentHmdOrientation.Rotator().Roll);
}

TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> FGearVR::GetViewExtension()
{
	TSharedPtr<FViewExtension, ESPMode::ThreadSafe> Result(MakeShareable(new FViewExtension(this)));
	return Result;
}

void FGearVR::ResetStereoRenderingParams()
{
	FHeadMountedDisplay::ResetStereoRenderingParams();
	Settings->InterpupillaryDistance = OVR_DEFAULT_IPD;
}

bool FGearVR::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (FHeadMountedDisplay::Exec(InWorld, Cmd, Ar))
	{
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("OVRGLOBALMENU")))
	{
		// fire off the global menu from the render thread
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(OVRGlobalMenu,
			FGearVR*, Plugin, this,
			{
				Plugin->StartOVRGlobalMenu();
			});
	}
	return false;
}

FString FGearVR::GetVersionString() const
{
	static const char* Results = OVR_VERSION_STRING;
	FString s = FString::Printf(TEXT("%s, VrLib: %s, built %s, %s"), *GEngineVersion.ToString(), UTF8_TO_TCHAR(Results),
		UTF8_TO_TCHAR(__DATE__), UTF8_TO_TCHAR(__TIME__));
	return s;
}

void FGearVR::OnScreenModeChange(EWindowMode::Type WindowMode)
{
	EnableStereo(WindowMode != EWindowMode::Windowed);
	UpdateStereoRenderingParams();
}

bool FGearVR::IsPositionalTrackingEnabled() const
{
	return false;
}

bool FGearVR::EnablePositionalTracking(bool enable)
{
	return false;
}

bool FGearVR::DoEnableStereo(bool bStereo, bool bApplyToHmd)
{
	return true;
}

void FGearVR::ApplySystemOverridesOnStereo(bool bForce)
{
	if (Settings->Flags.bStereoEnabled || bForce)
	{
		// Set the current VSync state
		if (Settings->Flags.bOverrideVSync)
		{
			static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
			CVSyncVar->Set(Settings->Flags.bVSync);
		}
		else
		{
			static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
			Settings->Flags.bVSync = CVSyncVar->GetInt() != 0;
		}

		static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
		CFinishFrameVar->Set(Settings->Flags.bAllowFinishCurrentFrame);
	}
}

void FGearVR::SaveSystemValues()
{
	static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	Settings->Flags.bSavedVSync = CVSyncVar->GetInt() != 0;

	static IConsoleVariable* CScrPercVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	Settings->SavedScrPerc = CScrPercVar->GetFloat();
}

void FGearVR::RestoreSystemValues()
{
	static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	CVSyncVar->Set(Settings->Flags.bSavedVSync);

	static IConsoleVariable* CScrPercVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	CScrPercVar->Set(Settings->SavedScrPerc);

	static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
	CFinishFrameVar->Set(false);
}

void FGearVR::CalculateStereoViewOffset(const EStereoscopicPass StereoPassType, const FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
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

void FGearVR::ResetOrientationAndPosition(float yaw)
{
	check (IsInGameThread());

	auto frame = GetFrame();
	if (!frame)
	{
		OCFlags.NeedResetOrientationAndPosition = true;
		ResetToYaw = yaw;
		return;
	}

	const ovrPosef& pose = frame->CurSensorState.Recorded.Pose;
	const OVR::Quatf orientation = OVR::Quatf(pose.Orientation);

	// Reset position
	Settings->BaseOffset = FVector::ZeroVector;

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
	OCFlags.NeedResetOrientationAndPosition = false;
}

FMatrix FGearVR::GetStereoProjectionMatrix(enum EStereoscopicPass StereoPassType, const float FOV) const
{
	auto frame = GetFrame();
	check(frame);
	check(IsStereoEnabled());

	const FSettings* FrameSettings = frame->GetSettings();

	const float ProjectionCenterOffset = 0.0f;
	const float PassProjectionOffset = (StereoPassType == eSSP_LEFT_EYE) ? ProjectionCenterOffset : -ProjectionCenterOffset;

	const float HalfFov = FrameSettings->HFOVInRadians / 2.0f;
	const float InWidth = FrameSettings->RenderTargetWidth / 2.0f;
	const float InHeight = FrameSettings->RenderTargetHeight;
	const float XS = 1.0f / tan(HalfFov);
	const float YS = InWidth / tan(HalfFov) / InHeight;

	// correct far and near planes for reversed-Z projection matrix
	const float InNearZ = (FrameSettings->NearClippingPlane) ? FrameSettings->NearClippingPlane : GNearClippingPlane;
	const float InFarZ = (FrameSettings->FarClippingPlane) ? FrameSettings->FarClippingPlane : GNearClippingPlane;

	const float M_2_2 = (InNearZ == InFarZ) ? 0.0f    : InNearZ / (InNearZ - InFarZ);
	const float M_3_2 = (InNearZ == InFarZ) ? InNearZ : -InFarZ * InNearZ / (InNearZ - InFarZ);

	FMatrix proj = FMatrix(
		FPlane(XS,                      0.0f,								    0.0f,							0.0f),
		FPlane(0.0f,					YS,	                                    0.0f,							0.0f),
		FPlane(0.0f,	                0.0f,								    M_2_2,							1.0f),
		FPlane(0.0f,					0.0f,								    M_3_2,							0.0f))

		* FTranslationMatrix(FVector(PassProjectionOffset,0,0));
	
	ovrMatrix4f tanAngleMatrix = ToMatrix4f(proj);
	frame->TanAngleMatrix = TanAngleMatrixFromProjection( &tanAngleMatrix );
	return proj;
}

void FGearVR::InitCanvasFromView(FSceneView* InView, UCanvas* Canvas)
{
	// This is used for placing small HUDs (with names)
	// over other players (for example, in Capture Flag).
	// HmdOrientation should be initialized by GetCurrentOrientation (or
	// user's own value).
}

//---------------------------------------------------
// GearVR ISceneViewExtension Implementation
//---------------------------------------------------

void FGearVR::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	InViewFamily.EngineShowFlags.MotionBlur = 0;
	InViewFamily.EngineShowFlags.HMDDistortion = false;
	InViewFamily.EngineShowFlags.ScreenPercentage = true;
	InViewFamily.EngineShowFlags.StereoRendering = IsStereoEnabled();
}

void FGearVR::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	auto frame = GetFrame();
	check(frame);

	InView.BaseHmdOrientation = frame->LastHmdOrientation;
	InView.BaseHmdLocation = frame->LastHmdPosition;

	InViewFamily.bUseSeparateRenderTarget = ShouldUseSeparateRenderTarget();

	const int eyeIdx = (InView.StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
	frame->CachedViewRotation[eyeIdx] = InView.ViewRotation;
}

FGearVR::FGearVR()
	: DeltaControlRotation(FRotator::ZeroRotator)
	, OvrMobile_RT(nullptr)
{
	OCFlags.Raw = 0;
	DeltaControlRotation = FRotator::ZeroRotator;
	ResetToYaw = 0.f;

	Settings = MakeShareable(new FSettings);

	Startup();
}

FGearVR::~FGearVR()
{
	Shutdown();
}

void FGearVR::Startup()
{
	// grab the clock settings out of the ini
	const TCHAR* GearVRSettings = TEXT("GearVR.Settings");
	int CpuLevel = 2;
	int GpuLevel = 2;
	int MinimumVsyncs = 1;
	float HeadModelScale = 1.0f;
	GConfig->GetInt(GearVRSettings, TEXT("CpuLevel"), CpuLevel, GEngineIni);
	GConfig->GetInt(GearVRSettings, TEXT("GpuLevel"), GpuLevel, GEngineIni);
	GConfig->GetInt(GearVRSettings, TEXT("MinimumVsyncs"), MinimumVsyncs, GEngineIni);
	GConfig->GetFloat(GearVRSettings, TEXT("HeadModelScale"), HeadModelScale, GEngineIni);

	UE_LOG(LogHMD, Log, TEXT("GearVR starting with CPU: %d GPU: %d MinimumVsyncs: %d"), CpuLevel, GpuLevel, MinimumVsyncs);

	GetSettings()->VrModeParms.SkipWindowFullscreenReset = true;
	GetSettings()->VrModeParms.AsynchronousTimeWarp = true;
	GetSettings()->VrModeParms.DistortionFileName = NULL;
	GetSettings()->VrModeParms.EnableImageServer = false;
	GetSettings()->VrModeParms.GameThreadTid = gettid();
	GetSettings()->VrModeParms.CpuLevel = CpuLevel;
	GetSettings()->VrModeParms.GpuLevel = GpuLevel;
	GetSettings()->VrModeParms.ActivityObject = FJavaWrapper::GameActivityThis;

	GetSettings()->HeadModel *= HeadModelScale;
	GetSettings()->MinimumVsyncs = MinimumVsyncs;

	FPlatformMisc::MemoryBarrier();

	if (!IsRunningGame() || (Settings->Flags.InitStatus & FSettings::eStartupExecuted) != 0)
	{
		// do not initialize plugin for server or if it was already initialized
		return;
	}
	Settings->Flags.InitStatus |= FSettings::eStartupExecuted;

	// register our application lifetime delegates
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FGearVR::ApplicationPauseDelegate);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FGearVR::ApplicationResumeDelegate);

	Settings->Flags.InitStatus |= FSettings::eInitialized;

	UpdateHmdRenderInfo();
	UpdateStereoRenderingParams();

	// Uncap fps to enable FPS higher than 62
	GEngine->bSmoothFrameRate = false;

	pGearVRBridge = new FGearVRCustomPresent(MinimumVsyncs);

	LoadFromIni();
	SaveSystemValues();
}

void FGearVR::Shutdown()
{
	if (!(Settings->Flags.InitStatus & FSettings::eStartupExecuted))
	{
		return;
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(ShutdownRen,
		FGearVR*, Plugin, this,
		{
			Plugin->ShutdownRendering();
		});

	// Wait for all resources to be released
	FlushRenderingCommands();

	Settings->Flags.InitStatus = 0;
	UE_LOG(LogHMD, Log, TEXT("GearVR shutdown."));
}

void FGearVR::ApplicationPauseDelegate()
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("+++++++ GEARVR APP PAUSE ++++++"));

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(ShutdownRen,
		FGearVR*, Plugin, this,
		{
			Plugin->ShutdownRendering();
		});

	// Wait for all resources to be released
	FlushRenderingCommands();
}

void FGearVR::ApplicationResumeDelegate()
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("+++++++ GEARVR APP RESUME ++++++"));

	if(!pGearVRBridge)
	{
		pGearVRBridge = new FGearVRCustomPresent(GetSettings()->MinimumVsyncs);
	}
}

void FGearVR::UpdateHmdRenderInfo()
{
}

void FGearVR::UpdateStereoRenderingParams()
{
	FSettings* CurrentSettings = GetSettings();

	if ((!CurrentSettings->IsStereoEnabled() && !CurrentSettings->Flags.bHeadTrackingEnforced))
	{
		return;
	}
	if (IsInitialized())
	{
		CurrentSettings->HmdToEyeViewOffset[0] = CurrentSettings->HmdToEyeViewOffset[1] = OVR::Vector3f(0,0,0);
		CurrentSettings->HmdToEyeViewOffset[0].x = CurrentSettings->InterpupillaryDistance * 0.5f;
		CurrentSettings->HmdToEyeViewOffset[1].x = -CurrentSettings->InterpupillaryDistance * 0.5f;
	}

	Flags.bNeedUpdateStereoRenderingParams = false;
}

void FGearVR::LoadFromIni()
{
	FSettings* CurrentSettings = GetSettings();
	const TCHAR* GearVRSettings = TEXT("GearVR.Settings");
	bool v;
	float f;
	if (GConfig->GetBool(GearVRSettings, TEXT("bChromaAbCorrectionEnabled"), v, GEngineIni))
	{
		CurrentSettings->Flags.bChromaAbCorrectionEnabled = v;
	}
	if (GConfig->GetBool(GearVRSettings, TEXT("bDevSettingsEnabled"), v, GEngineIni))
	{
		CurrentSettings->Flags.bDevSettingsEnabled = v;
	}
	if (GConfig->GetFloat(GearVRSettings, TEXT("MotionPrediction"), f, GEngineIni))
	{
		CurrentSettings->MotionPredictionInSeconds = f;
	}
	if (GConfig->GetBool(GearVRSettings, TEXT("bOverrideIPD"), v, GEngineIni))
	{
		CurrentSettings->Flags.bOverrideIPD = v;
		if (CurrentSettings->Flags.bOverrideIPD)
		{
			if (GConfig->GetFloat(GearVRSettings, TEXT("IPD"), f, GEngineIni))
			{
				SetInterpupillaryDistance(f);
			}
		}
	}
	if (GConfig->GetBool(GearVRSettings, TEXT("bOverrideStereo"), v, GEngineIni))
	{
		CurrentSettings->Flags.bOverrideStereo = v;
		if (CurrentSettings->Flags.bOverrideStereo)
		{
			if (GConfig->GetFloat(GearVRSettings, TEXT("HFOV"), f, GEngineIni))
			{
				CurrentSettings->HFOVInRadians = f;
			}
			if (GConfig->GetFloat(GearVRSettings, TEXT("VFOV"), f, GEngineIni))
			{
				CurrentSettings->VFOVInRadians = f;
			}
		}
	}
	if (GConfig->GetBool(GearVRSettings, TEXT("bOverrideVSync"), v, GEngineIni))
	{
		CurrentSettings->Flags.bOverrideVSync = v;
		if (GConfig->GetBool(GearVRSettings, TEXT("bVSync"), v, GEngineIni))
		{
			CurrentSettings->Flags.bVSync = v;
		}
	}
	if (GConfig->GetBool(GearVRSettings, TEXT("bOverrideScreenPercentage"), v, GEngineIni))
	{
		CurrentSettings->Flags.bOverrideScreenPercentage = v;
		if (GConfig->GetFloat(GearVRSettings, TEXT("ScreenPercentage"), f, GEngineIni))
		{
			CurrentSettings->ScreenPercentage = f;
		}
	}
	if (GConfig->GetBool(GearVRSettings, TEXT("bAllowFinishCurrentFrame"), v, GEngineIni))
	{
		CurrentSettings->Flags.bAllowFinishCurrentFrame = v;
	}
	if (GConfig->GetBool(GearVRSettings, TEXT("bUpdateOnRT"), v, GEngineIni))
	{
		CurrentSettings->Flags.bUpdateOnRT = v;
	}
	if (GConfig->GetFloat(GearVRSettings, TEXT("FarClippingPlane"), f, GEngineIni))
	{
		CurrentSettings->FarClippingPlane = f;
	}
	if (GConfig->GetFloat(GearVRSettings, TEXT("NearClippingPlane"), f, GEngineIni))
	{
		CurrentSettings->NearClippingPlane = f;
	}
}

void FViewExtension::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& View)
{
	check(IsInRenderingThread());

	FViewExtension& RenderContext = *this; 
	const FGameFrame* CurrentFrame = static_cast<const FGameFrame*>(RenderContext.RenderFrame.Get());

	if (!RenderContext.ShowFlags.Rendering || !CurrentFrame || !CurrentFrame->Settings->IsStereoEnabled())
	{
		return;
	}

	if (RenderContext.ShowFlags.Rendering && CurrentFrame->Settings->Flags.bUpdateOnRT)
	{
		const unsigned eyeIdx = (View.StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
		FQuat	CurrentEyeOrientation;
		FVector	CurrentEyePosition;
		CurrentFrame->PoseToOrientationAndPosition(RenderContext.CurEyeRenderPose[eyeIdx], CurrentEyeOrientation, CurrentEyePosition);

		FQuat ViewOrientation = View.ViewRotation.Quaternion();

		// recalculate delta control orientation; it should match the one we used in CalculateStereoViewOffset on a game thread.
		FVector GameEyePosition;
		FQuat GameEyeOrient;

		CurrentFrame->PoseToOrientationAndPosition(CurrentFrame->EyeRenderPose[eyeIdx], GameEyeOrient, GameEyePosition);
		const FQuat DeltaControlOrientation =  ViewOrientation * GameEyeOrient.Inverse();
		// make sure we use the same viewrotation as we had on a game thread
		check(View.ViewRotation == CurrentFrame->CachedViewRotation[eyeIdx]);

		if (CurrentFrame->Flags.bOrientationChanged)
		{
			// Apply updated orientation to corresponding View at recalc matrices.
			// The updated position will be applied from inside of the UpdateViewMatrix() call.
			const FQuat DeltaOrient = View.BaseHmdOrientation.Inverse() * CurrentEyeOrientation;
			View.ViewRotation = FRotator(ViewOrientation * DeltaOrient);

			//UE_LOG(LogHMD, Log, TEXT("VIEWDLT: Yaw %.3f Pitch %.3f Roll %.3f"), DeltaOrient.Rotator().Yaw, DeltaOrient.Rotator().Pitch, DeltaOrient.Rotator().Roll);
		}

		if (!CurrentFrame->Flags.bPositionChanged)
		{
			// if no positional change applied then we still need to calculate proper stereo disparity.
			// use the current head pose for this calculation instead of the one that was saved on a game thread.
			FQuat HeadOrientation;
			CurrentFrame->PoseToOrientationAndPosition(RenderContext.CurHeadPose, HeadOrientation, View.BaseHmdLocation);
		}

		// The HMDPosition already has HMD orientation applied.
		// Apply rotational difference between HMD orientation and ViewRotation
		// to HMDPosition vector. 
		// PositionOffset should be already applied to View.ViewLocation on GT in PlayerCameraUpdate.
		const FVector DeltaPosition = CurrentEyePosition - View.BaseHmdLocation;
		const FVector vEyePosition = DeltaControlOrientation.RotateVector(DeltaPosition);
		View.ViewLocation += vEyePosition;

		//UE_LOG(LogHMD, Log, TEXT("VDLTPOS: %.3f %.3f %.3f"), vEyePosition.X, vEyePosition.Y, vEyePosition.Z);

		if (CurrentFrame->Flags.bOrientationChanged || CurrentFrame->Flags.bPositionChanged)
		{
			View.UpdateViewMatrix();
		}
	}
}

void FGearVR::EnterVrMode(const ovrModeParms& InVrModeParms)
{
	FScopeLock lock(&OvrMobileLock);
	if (!OvrMobile_RT)
	{
		// enter vr mode
		OvrMobile_RT = ovr_EnterVrMode(InVrModeParms, &HmdInfo_RT);
		UE_LOG(LogHMD, Log, TEXT("++++ENTERED VR MODE++++"));
	}
}

void FViewExtension::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());

	FViewExtension& RenderContext = *this;
	FGameFrame* CurrentFrame = static_cast<FGameFrame*>(RenderContext.RenderFrame.Get());

	if (bFrameBegun || !CurrentFrame || !CurrentFrame->Settings->IsStereoEnabled())
	{
		return;
	}
	FSettings* FrameSettings = CurrentFrame->GetSettings();
	RenderContext.ShowFlags = ViewFamily.EngineShowFlags;

	auto pGearVRPlugin = static_cast<FGearVR*>(Delegate);
	pGearVRPlugin->EnterVrMode(FrameSettings->VrModeParms);
	RenderContext.OvrMobile = pGearVRPlugin->OvrMobile_RT;


	RenderContext.CurHeadPose = CurrentFrame->HeadPose;

// 	if (FrameSettings->TexturePaddingPerEye != 0)
// 	{
// 		// clear the padding between two eyes
// 		const int32 GapMinX = ViewFamily.Views[0]->ViewRect.Max.X;
// 		const int32 GapMaxX = ViewFamily.Views[1]->ViewRect.Min.X;
// 
// 		const int ViewportSizeY = (ViewFamily.RenderTarget->GetRenderTargetTexture()) ? 
// 			ViewFamily.RenderTarget->GetRenderTargetTexture()->GetSizeY() : ViewFamily.RenderTarget->GetSizeXY().Y;
// 		RHICmdList.SetViewport(GapMinX, 0, 0, GapMaxX, ViewportSizeY, 1.0f);
// 		RHICmdList.Clear(true, FLinearColor::Black, false, 0, false, 0, FIntRect());
// 	}

	check(ViewFamily.RenderTarget->GetRenderTargetTexture());
	uint32 RenderTargetWidth = ViewFamily.RenderTarget->GetRenderTargetTexture()->GetSizeX();
	uint32 RenderTargetHeight= ViewFamily.RenderTarget->GetRenderTargetTexture()->GetSizeY();
	CurrentFrame->GetSettings()->SetEyeRenderViewport(RenderTargetWidth/2, RenderTargetHeight);
	pPresentBridge->BeginRendering(RenderContext, ViewFamily.RenderTarget->GetRenderTargetTexture());

	bFrameBegun = true;

	if (ShowFlags.Rendering)
	{
		// get latest orientation/position and cache it
		ovrPosef NewEyeRenderPose[2];
		ovrSensorState ss;
		if (!pGearVRPlugin->GetEyePoses(*CurrentFrame, NewEyeRenderPose, ss))
		{
			return;
		}
		const ovrPosef& pose = ss.Predicted.Pose;

		pPresentBridge->SwapParms.Images[0][0].Pose = ss.Predicted;
		pPresentBridge->SwapParms.Images[1][0].Pose = ss.Predicted;

		// Take new EyeRenderPose is bUpdateOnRT.
		// if !bOrientationChanged && !bPositionChanged then we still need to use new eye pose (for timewarp)
		if (FrameSettings->Flags.bUpdateOnRT ||
			(!CurrentFrame->Flags.bOrientationChanged && !CurrentFrame->Flags.bPositionChanged))
		{
			RenderContext.CurHeadPose = pose;
			FMemory::Memcpy(RenderContext.CurEyeRenderPose, NewEyeRenderPose);
		}
		else
		{
			FMemory::Memcpy<ovrPosef[2]>(RenderContext.CurEyeRenderPose, CurrentFrame->EyeRenderPose);
			// use previous EyeRenderPose for proper timewarp when !bUpdateOnRt
			pPresentBridge->SwapParms.Images[0][0].Pose.Pose = CurrentFrame->HeadPose;
			pPresentBridge->SwapParms.Images[1][0].Pose.Pose = CurrentFrame->HeadPose;
		}
	}
}

void FGearVR::CalculateRenderTargetSize(const FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	check(IsInGameThread());

	InOutSizeX = GetFrame()->GetSettings()->RenderTargetWidth;
	InOutSizeY = GetFrame()->GetSettings()->RenderTargetHeight;
}

void FGearVR::GetOrthoProjection(int32 RTWidth, int32 RTHeight, float OrthoDistance, FMatrix OrthoProjection[2]) const
{
	OrthoProjection[0] = OrthoProjection[1] = FMatrix::Identity;
	
	// note, this is not right way, this is hack. The proper orthoproj matrix should be used. @TODO!
	OrthoProjection[1] = FTranslationMatrix(FVector(OrthoProjection[1].M[0][3] * RTWidth * .25 + RTWidth * .5, 0 , 0));
}

bool FGearVR::NeedReAllocateViewportRenderTarget(const FViewport& Viewport)
{
	check(IsInGameThread());

	if (IsStereoEnabled())
	{
		const uint32 InSizeX = Viewport.GetSizeXY().X;
		const uint32 InSizeY = Viewport.GetSizeXY().Y;
		const FIntPoint RenderTargetSize = Viewport.GetRenderTargetTextureSizeXY();

		uint32 NewSizeX = InSizeX, NewSizeY = InSizeY;
		CalculateRenderTargetSize(Viewport, NewSizeX, NewSizeY);
		if (NewSizeX != RenderTargetSize.X || NewSizeY != RenderTargetSize.Y)
		{
			return true;
		}
	}

	return false;
}

void FGearVR::ShutdownRendering()
{
	check(IsInRenderingThread());
	
	if (OvrMobile_RT)
	{
		{
			FScopeLock lock(&OvrMobileLock);
			ovr_LeaveVrMode(OvrMobile_RT);
			OvrMobile_RT = nullptr;
		}

		check(GJavaVM);
		const jint DetachResult = GJavaVM->DetachCurrentThread();
		if (DetachResult == JNI_ERR)
		{
			FPlatformMisc::LowLevelOutputDebugString(TEXT("FJNIHelper failed to detach thread from Java VM!"));
		}
	}

	if (pGearVRBridge)
	{
		pGearVRBridge->Shutdown();
		pGearVRBridge = nullptr;
	}
}

void FGearVR::StartOVRGlobalMenu()
{
	check(IsInRenderingThread());

	if (OvrMobile_RT)
	{
		ovr_StartSystemActivity(OvrMobile_RT, PUI_GLOBAL_MENU, NULL);
	}
}

void FGearVR::UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& InViewport, SViewport* ViewportWidget)
{
	check(IsInGameThread());

	FRHIViewport* const ViewportRHI = InViewport.GetViewportRHI().GetReference();

	if (!IsStereoEnabled() || !pGearVRBridge)
	{
		if (!bUseSeparateRenderTarget || !pGearVRBridge)
		{
			ViewportRHI->SetCustomPresent(nullptr);
		}
		return;
	}

	check(pGearVRBridge);

	pGearVRBridge->UpdateViewport(InViewport, ViewportRHI);
}

void FGearVR::DrawDebug(UCanvas* Canvas)
{
#if !UE_BUILD_SHIPPING
	check(IsInGameThread());
	const auto frame = GetCurrentFrame();
	if (frame)
	{
		if (frame->Settings->Flags.bDrawTrackingCameraFrustum)
		{
			DrawDebugTrackingCameraFrustum(GWorld, Canvas->SceneView->ViewRotation, Canvas->SceneView->ViewLocation);
		}
		DrawSeaOfCubes(GWorld, Canvas->SceneView->ViewLocation);
	}

#endif // #if !UE_BUILD_SHIPPING
}

//////////////////////////////////////////////////////////////////////////
FGearVRCustomPresent::FGearVRCustomPresent(int MinimumVsyncs) :
	FRHICustomPresent(nullptr),
	bInitialized(false),
	MinimumVsyncs(MinimumVsyncs)
{
	Init();
}

void FGearVRCustomPresent::DicedBlit(uint32 SourceX, uint32 SourceY, uint32 DestX, uint32 DestY, uint32 Width, uint32 Height, uint32 NumXSteps, uint32 NumYSteps)
{
	check((NumXSteps > 0) && (NumYSteps > 0))
	uint32 StepX = Width / NumXSteps;
	uint32 StepY = Height / NumYSteps;

	uint32 MaxX = SourceX + Width;
	uint32 MaxY = SourceY + Height;
	
	for (; SourceY < MaxY; SourceY += StepY, DestY += StepY)
	{
		uint32 CurHeight = FMath::Min(StepY, MaxY - SourceY);

		for (uint32 CurSourceX = SourceX, CurDestX = DestX; CurSourceX < MaxX; CurSourceX += StepX, CurDestX += StepX)
		{
			uint32 CurWidth = FMath::Min(StepX, MaxX - CurSourceX);

			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, CurDestX, DestY, CurSourceX, SourceY, CurWidth, CurHeight);
			GL_CHECK_ERROR;
		}
	}
}

void FGearVRCustomPresent::SetRenderContext(FHMDViewExtension* InRenderContext)
{
	if (InRenderContext)
	{
		RenderContext = StaticCastSharedRef<FViewExtension>(InRenderContext->AsShared());
	}
	else
	{
		RenderContext.Reset();
	}
}

void FGearVRCustomPresent::BeginRendering(FHMDViewExtension& InRenderContext, const FTexture2DRHIRef& RT)
{
	check(IsInRenderingThread());

	SetRenderContext(&InRenderContext);

	check(IsValidRef(RT));
	const uint32 RTSizeX = RT->GetSizeX();
	const uint32 RTSizeY = RT->GetSizeY();
	RTTexId = *(GLuint*)RT->GetNativeResource();

	FGameFrame* CurrentFrame = GetRenderFrame();
	check(CurrentFrame);

	SwapParms.Images[0][0].TexCoordsFromTanAngles = CurrentFrame->TanAngleMatrix;
	//SwapParms.Images[0][0].TexId = RTTexId;
	//SwapParms.Images[0][0].Pose = Plugin->RenderParams.EyeRenderPose[0];
	// 
	SwapParms.Images[1][0].TexCoordsFromTanAngles = CurrentFrame->TanAngleMatrix;
	//SwapParms.Images[1][0].TexId = RTTexId;
	//SwapParms.Images[1][0].Pose = Plugin->RenderParams.EyeRenderPose[1];

#if 0	// split screen stereo
	for ( int i = 0 ; i < 2 ; i++ )
	{
		for ( int j = 0 ; j < 3 ; j++ )
		{
			SwapParms.Images[i][0].TexCoordsFromTanAngles.M[0][j] *= ((float)RenderTargetHeight/(float)RenderTargetWidth);
		}
	}
	SwapParms.Images[1][0].TexCoordsFromTanAngles.M[0][2] -= 1.0 - ((float)RenderTargetHeight/(float)RenderTargetWidth);
	SwapParms.WarpProgram = WP_MIDDLE_CLAMP;
#else
	SwapParms.WarpProgram = WP_SIMPLE;
#endif
}

void FGearVRCustomPresent::FinishRendering()
{
	check(IsInRenderingThread());

	// initialize our eye textures if they don't exist yet
	if (SwapChainTextures[0][0] == 0)
	{
		// Initialize buffers to black
		const uint NumBytes = GetFrameSetting()->RenderTargetWidth * GetFrameSetting()->RenderTargetHeight * 4;
		uint8* InitBuffer = (uint8*)FMemory::Malloc(NumBytes);
		check(InitBuffer);
		FMemory::Memzero(InitBuffer, NumBytes);

		CurrentSwapChainIndex = 0;
		glGenTextures(6, &SwapChainTextures[0][0]);

		for( int i = 0; i < 3; i++ ) 
		{
			for (int Eye = 0; Eye < 2; ++Eye)
			{
				glBindTexture(GL_TEXTURE_2D, SwapChainTextures[Eye][i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GetFrameSetting()->RenderTargetWidth/2, GetFrameSetting()->RenderTargetHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, InitBuffer);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				GL_CHECK_ERROR;
			}
		}

		FMemory::Free(InitBuffer);

		glBindTexture( GL_TEXTURE_2D, 0 );
	}

	if (RenderContext->bFrameBegun)
	{
 		// Finish the frame and let OVR do buffer swap (Present) and flush/sync.
		if (RenderContext->OvrMobile)
		{
			uint32 CopyWidth = GetFrameSetting()->RenderTargetWidth / 2;
			uint32 CopyHeight = GetFrameSetting()->RenderTargetHeight;
			uint32 CurStartX = 0;

			// blit two 1022x1022 eye buffers into the swapparms textures
			for (uint32 Eye = 0; Eye < 2; ++Eye)
			{
				GLuint TexId = SwapChainTextures[Eye][CurrentSwapChainIndex];
				glBindTexture(GL_TEXTURE_2D, TexId );
				GL_CHECK_ERROR;

				DicedBlit(CurStartX+1, 1, 1, 1, CopyWidth-2, CopyHeight-2, 1, 1);
				
				CurStartX += CopyWidth;
				SwapParms.Images[Eye][0].TexId = TexId;
			}

			glBindTexture(GL_TEXTURE_2D, 0 );

			ovr_WarpSwap(RenderContext->OvrMobile, &SwapParms);
			ovr_HandleDeviceStateChanges(RenderContext->OvrMobile);
			CurrentSwapChainIndex = (CurrentSwapChainIndex + 1) % 3;
		}
	}
	else
	{
		UE_LOG(LogHMD, Warning, TEXT("Skipping frame: FinishRendering called with no corresponding BeginRendering (was BackBuffer re-allocated?)"));
	}
	SetRenderContext(nullptr);
}

void FGearVRCustomPresent::Init()
{
	bInitialized = true;
	FMemory::Memzero(SwapChainTextures);
	CurrentSwapChainIndex = 0;
	RTTexId = 0;
	SwapParms = InitTimeWarpParms();
	SwapParms.MinimumVsyncs = MinimumVsyncs;
}

void FGearVRCustomPresent::Reset()
{
	check(IsInRenderingThread());

	if (SwapChainTextures[0][0] != 0)
	{
		glDeleteTextures(6, &SwapChainTextures[0][0]);
	}

	if (RenderContext.IsValid())
	{

		RenderContext->bFrameBegun = false;
		RenderContext.Reset();
	}
	bInitialized = false;
}

void FGearVRCustomPresent::OnBackBufferResize()
{
	// if we are in the middle of rendering: prevent from calling EndFrame
	if (RenderContext.IsValid())
	{
		RenderContext->bFrameBegun = false;
	}
}

void FGearVRCustomPresent::UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI)
{
	check(IsInGameThread());
	check(ViewportRHI);

	this->ViewportRHI = ViewportRHI;
	ViewportRHI->SetCustomPresent(this);
}



bool FGearVRCustomPresent::Present(int& SyncInterval)
{
	check(IsInRenderingThread());

	FinishRendering();

	return false; // indicates that we are presenting here, UE shouldn't do Present.
}

//////////////////////////////////////////////////////////////////////////
FViewExtension::FViewExtension(FHeadMountedDisplay* InDelegate)
	: FHMDViewExtension(InDelegate)
	, ShowFlags(ESFIM_All0)
	, bFrameBegun(false)
{
	auto GearVRHMD = static_cast<FGearVR*>(InDelegate);
	pPresentBridge = GearVRHMD->pGearVRBridge;
}

#endif //GEARVR_SUPPORTED_PLATFORMS

