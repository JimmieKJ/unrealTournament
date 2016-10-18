// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GearVRPrivatePCH.h"
#include "GearVR.h"
#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

#if GEARVR_SUPPORTED_PLATFORMS

#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include <android_native_app_glue.h>

#endif

#include "RHIStaticStates.h"
#include "SceneViewport.h"
#include "GearVRSplash.h"


//---------------------------------------------------
// GearVR Plugin Implementation
//---------------------------------------------------

#if GEARVR_SUPPORTED_PLATFORMS
extern bool AndroidThunkCpp_IsGearVRApplication();

static TAutoConsoleVariable<int32> CVarGearVREnableMSAA(TEXT("gearvr.EnableMSAA"), 1, TEXT("Enable MSAA when rendering on GearVR"));

static TAutoConsoleVariable<int32> CVarGearVREnableQueueAhead(TEXT("gearvr.EnableQueueAhead"), 1, TEXT("Enable full-frame queue ahead for rendering on GearVR"));

static TAutoConsoleVariable<int32> CVarGearVRBackButton(TEXT("gearvr.HandleBackButton"), 1, TEXT("GearVR plugin will handle the 'back' button"));

static const ovrQuatf QuatIdentity = {0,0,0,1};

#endif

class FGearVRPlugin : public IGearVRPlugin
{
	/** IHeadMountedDisplayModule implementation */
	virtual TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > CreateHeadMountedDisplay() override;

	// Pre-init the HMD module
	virtual bool PreInit() override;

	FString GetModulePriorityKeyName() const
	{
		return FString(TEXT("GearVR"));
	}

	virtual void StartOVRGlobalMenu() const ;

	virtual void StartOVRQuitMenu() const ;

	virtual void SetCPUAndGPULevels(int32 CPULevel, int32 GPULevel) const ;

	virtual bool IsPowerLevelStateMinimum() const;

	virtual bool IsPowerLevelStateThrottled() const;
	
	virtual float GetTemperatureInCelsius() const;

	virtual float GetBatteryLevel() const;

	virtual bool AreHeadPhonesPluggedIn() const;

	virtual void SetLoadingIconTexture(FTextureRHIRef InTexture);

	virtual void SetLoadingIconMode(bool bActiveLoadingIcon);

	virtual void RenderLoadingIcon_RenderThread();

	virtual bool IsInLoadingIconMode() const;
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

bool FGearVRPlugin::PreInit()
{
#if GEARVR_SUPPORTED_PLATFORMS
	if (AndroidThunkCpp_IsGearVRApplication())
	{
		UE_LOG(LogHMD, Log, TEXT("GearVR: it is packaged for GearVR!"));
		return true;
	}
	UE_LOG(LogHMD, Log, TEXT("GearVR: not packaged for GearVR"));
	// don't do anything if we aren't packaged for GearVR
#endif//GEARVR_SUPPORTED_PLATFORMS
	return false;
}


#if GEARVR_SUPPORTED_PLATFORMS
FSettings::FSettings()
	: RenderTargetSize(OVR_DEFAULT_EYE_RENDER_TARGET_WIDTH * 2, OVR_DEFAULT_EYE_RENDER_TARGET_HEIGHT)
{
	CpuLevel = 2;
	GpuLevel = 3;
	HFOVInRadians = FMath::DegreesToRadians(90.f);
	VFOVInRadians = FMath::DegreesToRadians(90.f);
	IdealScreenPercentage = ScreenPercentage = SavedScrPerc = 100.f;

	HeadModelParms = vrapi_DefaultHeadModelParms();
	InterpupillaryDistance = HeadModelParms.InterpupillaryDistance;

	Flags.bStereoEnabled = false; Flags.bHMDEnabled = true;
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
	HeadPose.Pose.Orientation = QuatIdentity;
	EyeRenderPose[0].Orientation = EyeRenderPose[1].Orientation = QuatIdentity;
	CurEyeRenderPose[0].Orientation = CurEyeRenderPose[1].Orientation = QuatIdentity;
	GameThreadId = 0;
}

TSharedPtr<FHMDGameFrame, ESPMode::ThreadSafe> FGameFrame::Clone() const
{
	TSharedPtr<FGameFrame, ESPMode::ThreadSafe> NewFrame = MakeShareable(new FGameFrame(*this));
	return NewFrame;
}


//---------------------------------------------------
// GearVR IHeadMountedDisplay Implementation
//---------------------------------------------------

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

bool FGearVR::OnStartGameFrame( FWorldContext& WorldContext )
{
	// Temp fix to a bug in ovr_DeviceIsDocked() that can't return
	// actual state of docking. We are switching to stereo at the start
	// (missing the first frame to let it render at least once; otherwise
	// a blurry image may appear on Note4 with Adreno 420).
	if (GFrameNumber > 2 && !Settings->Flags.bStereoEnforced)
	{
		EnableStereo(true);
	}

#if 0 // temporarily out of order. Until ovr_DeviceIsDocked returns the actual state.
	if (ovr_DeviceIsDocked() != Settings->IsStereoEnabled())
	{
		if (!Settings->IsStereoEnabled() || !Settings->Flags.bStereoEnforced)
		{
			UE_LOG(LogHMD, Log, TEXT("Device is docked/undocked, changing stereo mode to %s"), (ovr_DeviceIsDocked()) ? TEXT("ON") : TEXT("OFF"));
			EnableStereo(ovr_DeviceIsDocked());
		}
	}
#endif // if 0

	bool rv = FHeadMountedDisplay::OnStartGameFrame(WorldContext);
	if (!rv)
	{
		return false;
	}

	FGameFrame* CurrentFrame = GetFrame();

	// need to make a copy of settings here, since settings could change.
	CurrentFrame->Settings = Settings->Clone();
	FSettings* CurrentSettings = CurrentFrame->GetSettings();

	if (pGearVRBridge && pGearVRBridge->IsSubmitFrameLocked())
	{
		CurrentSettings->Flags.bPauseRendering = true;
	}
	else
	{
		CurrentSettings->Flags.bPauseRendering = false;
	}

	if (OCFlags.bResumed && CurrentSettings->IsStereoEnabled() && pGearVRBridge)
	{
		if (!HasValidOvrMobile())
		{
			// re-enter VR mode if necessary
			EnterVRMode();
		}
	}
	CurrentFrame->GameThreadId = gettid();
	
	HandleBackButtonAction();

	rv = GetEyePoses(*CurrentFrame, CurrentFrame->CurEyeRenderPose, CurrentFrame->CurSensorState);

#if !UE_BUILD_SHIPPING
	{ // used for debugging, do not remove
		FQuat CurHmdOrientation;
		FVector CurHmdPosition;
		GetCurrentPose(CurHmdOrientation, CurHmdPosition, false, false);
		//UE_LOG(LogHMD, Log, TEXT("BFPOSE: Pos %.3f %.3f %.3f, fr: %d"), CurHmdPosition.X, CurHmdPosition.Y, CurHmdPosition.Y,(int)CurrentFrame->FrameNumber);
		//UE_LOG(LogHMD, Log, TEXT("BFPOSE: Yaw %.3f Pitch %.3f Roll %.3f, fr: %d"), CurHmdOrientation.Rotator().Yaw, CurHmdOrientation.Rotator().Pitch, CurHmdOrientation.Rotator().Roll, (int)CurrentFrame->FrameNumber);
	}
#endif
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
	if (!GetSettings()->IsStereoEnabled())
	{
		return false;
	}
	MonitorDesc.MonitorName = "";
	MonitorDesc.MonitorId = 0;
	MonitorDesc.DesktopX = MonitorDesc.DesktopY = 0;
	MonitorDesc.ResolutionX = GetSettings()->RenderTargetSize.X;
	MonitorDesc.ResolutionY = GetSettings()->RenderTargetSize.Y;
	return true;
}

bool FGearVR::IsHMDConnected()
{
	// consider HMD connected all the time if GearVR enabled
	return AndroidThunkCpp_IsGearVRApplication();
}

bool FGearVR::IsInLowPersistenceMode() const
{
	return true;
}

bool FGearVR::GetEyePoses(const FGameFrame& InFrame, ovrPosef outEyePoses[2], ovrTracking& outTracking)
{
	FOvrMobileSynced OvrMobile = GetMobileSynced();

	if (!OvrMobile.IsValid())
	{
		FMemory::Memzero(outTracking);
		outTracking.HeadPose.Pose.Orientation = QuatIdentity;
		const OVR::Vector3f HmdToEyeViewOffset0 = OVR::Vector3f(-InFrame.GetSettings()->HeadModelParms.InterpupillaryDistance * 0.5f, 0, 0); // -X <=, +X => (OVR coord sys)
		const OVR::Vector3f HmdToEyeViewOffset1 = OVR::Vector3f(InFrame.GetSettings()->HeadModelParms.InterpupillaryDistance * 0.5f, 0, 0);  // -X <=, +X => (OVR coord sys)
		const OVR::Vector3f transl0 = HmdToEyeViewOffset0;
		const OVR::Vector3f transl1 = HmdToEyeViewOffset1;
		outEyePoses[0].Orientation = outEyePoses[1].Orientation = outTracking.HeadPose.Pose.Orientation;
		outEyePoses[0].Position = transl0;
		outEyePoses[1].Position = transl1;
		return false;
	}

	double predictedTime = 0.0;
	const double now = vrapi_GetTimeInSeconds();
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
		predictedTime = vrapi_GetPredictedDisplayTime(*OvrMobile, InFrame.FrameNumber);
		//UE_LOG(LogHMD, Log, TEXT("RT Frame %d, predicted time: %.6f"), InFrame.FrameNumber, (float)(predictedTime - now));
	}
	outTracking = vrapi_GetPredictedTracking(*OvrMobile, predictedTime);

	outTracking = vrapi_ApplyHeadModel(&InFrame.GetSettings()->HeadModelParms, &outTracking);
	const OVR::Posef hmdPose = (OVR::Posef)outTracking.HeadPose.Pose;
	const OVR::Vector3f HmdToEyeViewOffset0 = OVR::Vector3f(-InFrame.GetSettings()->HeadModelParms.InterpupillaryDistance * 0.5f, 0, 0); // -X <=, +X => (OVR coord sys)
	const OVR::Vector3f HmdToEyeViewOffset1 = OVR::Vector3f(InFrame.GetSettings()->HeadModelParms.InterpupillaryDistance * 0.5f, 0, 0);  // -X <=, +X => (OVR coord sys)
	const OVR::Vector3f transl0 = hmdPose.Rotation.Rotate(HmdToEyeViewOffset0);
	const OVR::Vector3f transl1 = hmdPose.Rotation.Rotate(HmdToEyeViewOffset1);

	outEyePoses[0].Orientation = outEyePoses[1].Orientation = outTracking.HeadPose.Pose.Orientation;
	outEyePoses[0].Position = OVR::Vector3f(outTracking.HeadPose.Pose.Position) + transl0;
	outEyePoses[1].Position = OVR::Vector3f(outTracking.HeadPose.Pose.Position) + transl1;

	//UE_LOG(LogHMD, Log, TEXT("LEFTEYE: Pos %.3f %.3f %.3f"), ToFVector(outEyePoses[0].Position).X, ToFVector(outEyePoses[0].Position).Y, ToFVector(outEyePoses[0].Position).Z);
	//UE_LOG(LogHMD, Log, TEXT("RIGHEYE: Pos %.3f %.3f %.3f"), ToFVector(outEyePoses[1].Position).X, ToFVector(outEyePoses[1].Position).Y, ToFVector(outEyePoses[1].Position).Z);
	return true;
}

void FGameFrame::PoseToOrientationAndPosition(const ovrPosef& InPose, FQuat& OutOrientation, FVector& OutPosition) const
{
	OutOrientation = ToFQuat(InPose.Orientation);

	float WorldToMetersScale = GetWorldToMetersScale();
	check(WorldToMetersScale >= 0);
	// correct position according to BaseOrientation and BaseOffset. 
	const FVector Pos = (ToFVector_M2U(OVR::Vector3f(InPose.Position), WorldToMetersScale) - (Settings->BaseOffset * WorldToMetersScale)) * CameraScale3D;
	OutPosition = Settings->BaseOrientation.Inverse().RotateVector(Pos);

	// apply base orientation correction to OutOrientation
	OutOrientation = Settings->BaseOrientation.Inverse() * OutOrientation;
	OutOrientation.Normalize();
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
		frame->HeadPose = frame->CurSensorState.HeadPose;
	}

	frame->PoseToOrientationAndPosition(frame->CurSensorState.HeadPose.Pose, CurrentHmdOrientation, CurrentHmdPosition);
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
	const ovrHeadModelParms headModelParms = vrapi_DefaultHeadModelParms();
	Settings->InterpupillaryDistance = headModelParms.InterpupillaryDistance;
}

bool FGearVR::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (FHeadMountedDisplay::Exec(InWorld, Cmd, Ar))
	{
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("HMD")))
	{
		if (FParse::Command(&Cmd, TEXT("QAHEAD"))) // pixel density
		{
			FString CmdName = FParse::Token(Cmd, 0);

			if (!FCString::Stricmp(*CmdName, TEXT("ON")))
			{
				pGearVRBridge->bExtraLatencyMode = true;
			}
			else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
			{
				pGearVRBridge->bExtraLatencyMode = false;
			}
			else
			{
				pGearVRBridge->bExtraLatencyMode = !pGearVRBridge->bExtraLatencyMode;
			}
			Ar.Log(TEXT("Restart stereo for change to make effect"));
			return true;
		}
	}
	else if (FParse::Command(&Cmd, TEXT("OVRGLOBALMENU")))
	{
		// fire off the global menu from the render thread
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(OVRGlobalMenu,
			FGearVR*, Plugin, this,
			{
				Plugin->StartOVRGlobalMenu();
			});
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("OVRQUITMENU")))
	{
		// fire off the global menu from the render thread
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(OVRQuitMenu,
			FGearVR*, Plugin, this,
			{
				Plugin->StartOVRQuitMenu();
			});
		return true;
	}
#if !UE_BUILD_SHIPPING
	else if (FParse::Command(&Cmd, TEXT("OVRLD")))
	{
		SetLoadingIconMode(!IsInLoadingIconMode());
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("OVRLDI")))
	{
		if (!IsInLoadingIconMode())
		{
			const TCHAR* iconPath = TEXT("/Game/Tuscany_OculusCube.Tuscany_OculusCube");
			//const TCHAR* iconPath = TEXT("/Game/Loading/LoadingIconTexture.LoadingIconTexture");
			UE_LOG(LogHMD, Log, TEXT("Loading texture for loading icon %s..."), iconPath);
			UTexture2D* LoadingTexture = LoadObject<UTexture2D>(NULL, iconPath, NULL, LOAD_None, NULL);
			UE_LOG(LogHMD, Log, TEXT("...EEE"));
			if (LoadingTexture != nullptr)
			{
				LoadingTexture->AddToRoot();
				ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				SetRenderLoadingTex,
				FGearVR*, pGearVR, this,
				UTexture2D*, LoadingTexture, LoadingTexture,
				{
					UE_LOG(LogHMD, Log, TEXT("...Success. Loading icon format %d"), int(LoadingTexture->Resource->TextureRHI->GetFormat()));
					pGearVR->SetLoadingIconTexture(LoadingTexture->Resource->TextureRHI);
				});
				FlushRenderingCommands();
			}
			else
			{
				UE_LOG(LogHMD, Warning, TEXT("Can't load texture %s for loading icon"), iconPath);
			}
			return true;
		}
		else
		{
			SetLoadingIconTexture(nullptr);
		}
	}
	else if (FParse::Command(&Cmd, TEXT("TESTL"))) 
	{
		static uint32 LID1 = ~0u, LID2 = ~0u, LID3 = ~0u;
		IStereoLayers* StereoL = this;

		FString ValueStr = FParse::Token(Cmd, 0);
		int t = FCString::Atoi(*ValueStr);
		int maxt = 0;

		if (!FCString::Stricmp(*ValueStr, TEXT("OFF")))
		{
			t = -1;
		}
		if (!FCString::Stricmp(*ValueStr, TEXT("ALL")))
		{
			t = 0;
			maxt = 3;
		}

		do 
		{
		switch(t)
		{
			case 0:
			{
				if (LID1 != ~0u) break;
				const TCHAR* iconPath = TEXT("/Game/Tuscany_LoadScreen.Tuscany_LoadScreen");
				UE_LOG(LogHMD, Log, TEXT("LID1: Loading texture for loading icon %s..."), iconPath);
				TAssetPtr<UTexture2D> LoadingTexture = LoadObject<UTexture2D>(NULL, iconPath, NULL, LOAD_None, NULL);
				UE_LOG(LogHMD, Log, TEXT("...EEE"));

				if (LoadingTexture != nullptr)
				{
					UE_LOG(LogHMD, Log, TEXT("...Success. "));
					LoadingTexture->UpdateResource();
					//BeginUpdateResourceRHI(LoadingTexture->Resource);
					FlushRenderingCommands();

					IStereoLayers::FLayerDesc LayerDesc;
					LayerDesc.Texture = LoadingTexture->Resource->TextureRHI;
					LayerDesc.Priority = 10;
					LayerDesc.Transform = FTransform(FVector(400, 30, 130));
					LayerDesc.QuadSize = FVector2D(200, 200);
					LayerDesc.Type = IStereoLayers::ELayerType::WorldLocked;
					LID1 = StereoL->CreateLayer(LayerDesc);
				}
			}
			break;
			case 1:
			{
				if (LID2 != ~0u) break;
				//const TCHAR* iconPath = TEXT("/Game/alpha.alpha");
				const TCHAR* iconPath = TEXT("/Game/Tuscany_OculusCube.Tuscany_OculusCube");
				UE_LOG(LogHMD, Log, TEXT("LID2: Loading texture for loading icon %s..."), iconPath);
				TAssetPtr<UTexture2D> LoadingTexture = LoadObject<UTexture2D>(NULL, iconPath, NULL, LOAD_None, NULL);
				if (LoadingTexture != nullptr)
				{
					UE_LOG(LogHMD, Log, TEXT("...Success. "));
					LoadingTexture->UpdateResource();
					FlushRenderingCommands();

					IStereoLayers::FLayerDesc LayerDesc;
					LayerDesc.Texture = LoadingTexture->Resource->TextureRHI;
					LayerDesc.Priority = 10;
					LayerDesc.Transform = FTransform(FRotator(0, 30, 0), FVector(300, 0, 0));
					LayerDesc.QuadSize = FVector2D(200, 200);
					LayerDesc.Type = IStereoLayers::ELayerType::FaceLocked;
					LID2 = StereoL->CreateLayer(LayerDesc);
				}
			}
			break;
			case 2:
			{
				if (LID3 != ~0u) break;
				//const TCHAR* iconPath = TEXT("/Game/alpha.alpha");
				const TCHAR* iconPath = TEXT("/Game/Tuscany_OculusCube.Tuscany_OculusCube");
				UE_LOG(LogHMD, Log, TEXT("LID3: Loading texture for loading icon %s..."), iconPath);
				TAssetPtr<UTexture2D> LoadingTexture = LoadObject<UTexture2D>(NULL, iconPath, NULL, LOAD_None, NULL);
				if (LoadingTexture != nullptr)
				{
					UE_LOG(LogHMD, Log, TEXT("...Success. "));
					LoadingTexture->UpdateResource();
					FlushRenderingCommands();

					IStereoLayers::FLayerDesc LayerDesc;
					LayerDesc.Texture = LoadingTexture->Resource->TextureRHI;
					LayerDesc.Priority = 10;
					LayerDesc.Transform = FTransform(FRotator(0, 30, 0), FVector(300, 100, 0));
					LayerDesc.QuadSize = FVector2D(200, 200);
					LayerDesc.Type = IStereoLayers::ELayerType::TrackerLocked;
					LID3 = StereoL->CreateLayer(LayerDesc);
				}
			}
			break;
			case 3: 
			{
				IStereoLayers::FLayerDesc LayerDesc;
				StereoL->GetLayerDesc(LID2, LayerDesc);
				FTransform tr(FRotator(0, -30, 0), FVector(100, 0, 0));

				LayerDesc.Transform = tr;
				LayerDesc.QuadSize = FVector2D(200, 200);
				StereoL->SetLayerDesc(LID2, LayerDesc);
			} 
			break;
			case -1: 
			{
				UE_LOG(LogHMD, Log, TEXT("Destroy layers %d %d %d"), LID1, LID2, LID3);
				StereoL->DestroyLayer(LID1);
				StereoL->DestroyLayer(LID2);
				StereoL->DestroyLayer(LID3);
			}
		}
		} while (++t < maxt);
	}

#endif
	return false;
}

FString FGearVR::GetVersionString() const
{
	FString VerStr = ANSI_TO_TCHAR(vrapi_GetVersionString());
	FString s = FString::Printf(TEXT("GearVR - %s, VrLib: %s, built %s, %s"), *FEngineVersion::Current().ToString(), *VerStr,
		UTF8_TO_TCHAR(__DATE__), UTF8_TO_TCHAR(__TIME__));
	return s;
}

void FGearVR::GetRawSensorData(SensorData& OutData)
{
	FMemory::Memset(OutData, 0);
	const auto frame = GetFrame();
	if (!frame)
	{
		return;
	}
	OutData.AngularAcceleration = ToFVector(frame->HeadPose.AngularAcceleration);
	OutData.LinearAcceleration = ToFVector(frame->HeadPose.LinearAcceleration);
	OutData.AngularVelocity = ToFVector(frame->HeadPose.AngularVelocity);
	OutData.LinearVelocity = ToFVector(frame->HeadPose.LinearVelocity);
	OutData.TimeInSeconds = frame->HeadPose.TimeInSeconds;
}

bool FGearVR::IsPositionalTrackingEnabled() const
{
	return false;
}

bool FGearVR::EnablePositionalTracking(bool enable)
{
	return false;
}

static class FSceneViewport* FindSceneViewport()
{
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	return GameEngine->SceneViewport.Get();
}

bool FGearVR::EnableStereo(bool bStereo)
{
	Settings->Flags.bStereoEnforced = false;
	if (bStereo)
	{
		Flags.bNeedEnableStereo = true;
		Flags.bNeedDisableStereo = false; 
	}
	else
	{
		Flags.bNeedEnableStereo = false;
		Flags.bNeedDisableStereo = true;
	}
	return Settings->Flags.bStereoEnabled;
}

bool FGearVR::DoEnableStereo(bool bStereo)
{
	check(IsInGameThread());

	FSceneViewport* SceneVP = FindSceneViewport();
	if (bStereo && (!SceneVP || !SceneVP->IsStereoRenderingAllowed()))
	{
		return false;
	}

	// Uncap fps to enable FPS higher than 62
	GEngine->bForceDisableFrameRateSmoothing = bStereo;

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

	Settings->Flags.bStereoEnabled = stereoToBeEnabled;

	if (!stereoToBeEnabled)
	{
		LeaveVRMode();
	}
	return Settings->Flags.bStereoEnabled;
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
				UE_LOG(LogHMD, Log, TEXT("Orientation wasn't applied to a camera in frame %d"), int(CurrentFrameNumber.GetValue()));
			}

			FVector CurEyePosition;
			FQuat CurEyeOrient;
			frame->PoseToOrientationAndPosition(frame->EyeRenderPose[idx], CurEyeOrient, CurEyePosition);
			frame->PlayerLocation = ViewLocation;

			FVector HeadPosition = FVector::ZeroVector;
			// If we use PlayerController->bFollowHmd then we must apply full EyePosition (HeadPosition == 0).
			// Otherwise, we will apply only a difference between EyePosition and HeadPosition, since
			// HeadPosition is supposedly already applied.
			if (!frame->Flags.bPlayerControllerFollowsHmd)
			{
				FQuat HeadOrient;
				frame->PoseToOrientationAndPosition(frame->HeadPose.Pose, HeadOrient, HeadPosition);
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

	FOvrMobileSynced OvrMobile = GetMobileSynced();

	auto frame = GetFrame();
	if (!frame || !OvrMobile.IsValid())
	{
		OCFlags.NeedResetOrientationAndPosition = true;
		ResetToYaw = yaw;
		return;
	}

	Settings->BaseOffset = FVector::ZeroVector;
	if (yaw != 0.0f)
	{
		Settings->BaseOrientation = FRotator(0, -yaw, 0).Quaternion();
	}
	else
	{
		Settings->BaseOrientation = FQuat::Identity;
	}
	if (OvrMobile.IsValid())
	{
		vrapi_RecenterPose(*OvrMobile);
	}
	OCFlags.NeedResetOrientationAndPosition = false;
}

void FGearVR::RebaseObjectOrientationAndPosition(FVector& OutPosition, FQuat& OutOrientation) const
{
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
	const float InWidth = FrameSettings->RenderTargetSize.X / 2.0f;
	const float InHeight = FrameSettings->RenderTargetSize.Y;
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
	frame->TanAngleMatrix = ovrMatrix4f_TanAngleMatrixFromProjection(&tanAngleMatrix);
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
	InViewFamily.EngineShowFlags.ScreenPercentage = false;
	InViewFamily.EngineShowFlags.StereoRendering = IsStereoEnabled();

	auto frame = GetFrame();
	check(frame);
	if (frame->Settings->Flags.bPauseRendering)
	{
		InViewFamily.EngineShowFlags.Rendering = 0;
	}
}

void FGearVR::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
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

FGearVR::FGearVR()
	: DeltaControlRotation(FRotator::ZeroRotator)
{
	OCFlags.Raw = 0;
	DeltaControlRotation = FRotator::ZeroRotator;
	ResetToYaw = 0.f;

	Settings = MakeShareable(new FSettings);

	BackButtonState = BACK_BUTTON_STATE_NONE;
	BackButtonDown = false;
	BackButtonDownStartTime = 0.0;

	Startup();
}

FGearVR::~FGearVR()
{
	Shutdown();
}

void FGearVR::Startup()
{
	CurrentFrameNumber.Set(1);
//	Flags.bNeedEnableStereo = true;

	// grab the clock settings out of the ini
	const TCHAR* GearVRSettings = TEXT("GearVR.Settings");
	int CpuLevel = 2;
	int GpuLevel = 3;
	int MinimumVsyncs = 1;
	float HeadModelScale = 1.0f;
	GConfig->GetInt(GearVRSettings, TEXT("CpuLevel"), CpuLevel, GEngineIni);
	GConfig->GetInt(GearVRSettings, TEXT("GpuLevel"), GpuLevel, GEngineIni);
	GConfig->GetInt(GearVRSettings, TEXT("MinimumVsyncs"), MinimumVsyncs, GEngineIni);
	GConfig->GetFloat(GearVRSettings, TEXT("HeadModelScale"), HeadModelScale, GEngineIni);

	UE_LOG(LogHMD, Log, TEXT("GearVR starting with CPU: %d GPU: %d MinimumVsyncs: %d"), CpuLevel, GpuLevel, MinimumVsyncs);

	JavaGT.Vm = GJavaVM;
	JavaGT.Env = FAndroidApplication::GetJavaEnv();
	extern struct android_app* GNativeAndroidApp;
	JavaGT.ActivityObject = GNativeAndroidApp->activity->clazz;

	SystemActivities_Init(&JavaGT);

	const ovrInitParms initParms = vrapi_DefaultInitParms(&JavaGT);
	int32_t initResult = vrapi_Initialize(&initParms);
	if (initResult != VRAPI_INITIALIZE_SUCCESS)
	{
		char const * msg = initResult == VRAPI_INITIALIZE_PERMISSIONS_ERROR ? 
			"Thread priority security exception. Make sure the APK is signed." :
			"VrApi initialization error.";
		SystemActivities_DisplayError(&JavaGT, SYSTEM_ACTIVITIES_FATAL_ERROR_OSIG, __FILE__, msg);
		return;
	}

	ovrHeadModelParms& HeadModel = GetSettings()->HeadModelParms;
	HeadModel.InterpupillaryDistance *= HeadModelScale;
	HeadModel.EyeHeight *= HeadModelScale;
	HeadModel.HeadModelDepth *= HeadModelScale;
	HeadModel.HeadModelHeight *= HeadModelScale;

	GetSettings()->MinimumVsyncs = MinimumVsyncs;
	GetSettings()->CpuLevel = CpuLevel;
	GetSettings()->GpuLevel = GpuLevel;

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

	LoadFromIni();
	UpdateHmdRenderInfo();
	UpdateStereoRenderingParams();

#if !OVR_DEBUG_DRAW
	pGearVRBridge = new FCustomPresent(GNativeAndroidApp->activity->clazz, MinimumVsyncs);
#endif

	SaveSystemValues();

	if(CVarGearVREnableMSAA.GetValueOnAnyThread())
	{
		static IConsoleVariable* CVarMobileOnChipMSAA = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MobileOnChipMSAA"));
		if (CVarMobileOnChipMSAA)
		{
			UE_LOG(LogHMD, Log, TEXT("Enabling r.MobileOnChipMSAA, previous value %d"), CVarMobileOnChipMSAA->GetInt());
			CVarMobileOnChipMSAA->Set(1);
		}
	}
	pGearVRBridge->bExtraLatencyMode = CVarGearVREnableQueueAhead.GetValueOnAnyThread() != 0;

	Splash = MakeShareable(new FGearVRSplash(this));
	Splash->Startup();

	UE_LOG(LogHMD, Log, TEXT("GearVR has started"));
}

void FGearVR::Shutdown()
{
	if (!(Settings->Flags.InitStatus & FSettings::eStartupExecuted))
	{
		return;
	}

	if (Splash.IsValid())
	{
		Splash->Shutdown();
		Splash = nullptr;
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(ShutdownRen,
	FGearVR*, Plugin, this,
	{
		Plugin->ShutdownRendering();
		if (Plugin->pGearVRBridge)
		{
			Plugin->pGearVRBridge->Shutdown();
			Plugin->pGearVRBridge = nullptr;
		}

	});

	// Wait for all resources to be released
	FlushRenderingCommands();

	Settings->Flags.InitStatus = 0;

	vrapi_Shutdown();

	SystemActivities_Shutdown(&JavaGT);

	UE_LOG(LogHMD, Log, TEXT("GearVR shutdown."));
}

void FGearVR::ApplicationPauseDelegate()
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("+++++++ GEARVR APP PAUSE ++++++, tid = %d"), gettid());
	OCFlags.bResumed = false;

	LeaveVRMode();
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("------- GEARVR APP PAUSE ------, tid = %d"), gettid());
}

void FGearVR::ApplicationResumeDelegate()
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("+++++++ GEARVR APP RESUME ++++++, tid = %d"), gettid());
	OCFlags.bResumed = true;
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("------- GEARVR APP RESUME ------, tid = %d"), gettid());
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
		const int SuggestedEyeResolutionWidth  = vrapi_GetSystemPropertyInt(&JavaGT, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH);
		const int SuggestedEyeResolutionHeight = vrapi_GetSystemPropertyInt(&JavaGT, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT);

		CurrentSettings->RenderTargetSize.X = SuggestedEyeResolutionWidth * 2 * CurrentSettings->ScreenPercentage / 100;
		CurrentSettings->RenderTargetSize.Y = SuggestedEyeResolutionHeight * CurrentSettings->ScreenPercentage / 100;

		const float SuggestedEyeFovDegreesX = vrapi_GetSystemPropertyFloat(&JavaGT, VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_X);
		const float SuggestedEyeFovDegreesY = vrapi_GetSystemPropertyFloat(&JavaGT, VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_Y);
		CurrentSettings->HFOVInRadians = FMath::DegreesToRadians(SuggestedEyeFovDegreesX);
		CurrentSettings->VFOVInRadians = FMath::DegreesToRadians(SuggestedEyeFovDegreesY);

		const int32 RTSizeX = CurrentSettings->RenderTargetSize.X;
		const int32 RTSizeY = CurrentSettings->RenderTargetSize.Y;
		CurrentSettings->EyeRenderViewport[0] = FIntRect(1, 1, RTSizeX / 2 - 1, RTSizeY - 1);
		CurrentSettings->EyeRenderViewport[1] = FIntRect(RTSizeX / 2 + 1, 1, RTSizeX-1, RTSizeY-1);
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

void FGearVR::GetOrthoProjection(int32 RTWidth, int32 RTHeight, float OrthoDistance, FMatrix OrthoProjection[2]) const
{
	OrthoProjection[0] = OrthoProjection[1] = FMatrix::Identity;
	
	// note, this is not right way, this is hack. The proper orthoproj matrix should be used. @TODO!
	OrthoProjection[1] = FTranslationMatrix(FVector(OrthoProjection[1].M[0][3] * RTWidth * .25 + RTWidth * .5, 0 , 0));
}

void FGearVR::StartSystemActivity_RenderThread(const char * commandString)
{
	UE_LOG(LogHMD, Log, TEXT("StartSystemActivity( %s )"), ANSI_TO_TCHAR(commandString) );
	check(IsInRenderingThread());

	if (SystemActivities_StartSystemActivity(&pGearVRBridge->JavaRT, commandString, NULL))
	{
		// Push black images to the screen to eliminate any frames of lost head tracking.
		if (pGearVRBridge->OvrMobile)
		{
			const ovrFrameParms blackFrameParms = vrapi_DefaultFrameParms(&pGearVRBridge->JavaRT, VRAPI_FRAME_INIT_BLACK_FINAL, vrapi_GetTimeInSeconds(), NULL );
			vrapi_SubmitFrame(pGearVRBridge->OvrMobile, &blackFrameParms);
		}
		return;
	}

	UE_LOG(LogHMD, Log, TEXT( "*************************************************************************" ));
	UE_LOG(LogHMD, Log, TEXT( "A fatal dependency error occured. Oculus SystemActivities failed to start."));
	UE_LOG(LogHMD, Log, TEXT( "*************************************************************************" ));
	SystemActivities_ReturnToHome(&pGearVRBridge->JavaRT);	
}


void FGearVR::StartOVRGlobalMenu()
{
	check(IsInRenderingThread());

	if (pGearVRBridge)
	{
		StartSystemActivity_RenderThread(PUI_GLOBAL_MENU);
	}
}

void FGearVR::StartOVRQuitMenu()
{
	check(IsInRenderingThread());

	if (pGearVRBridge)
	{
		StartSystemActivity_RenderThread(PUI_CONFIRM_QUIT);
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
	if (frame && Canvas && Canvas->SceneView)
	{
		if (frame->Settings->Flags.bDrawSensorFrustum)
		{
			DrawDebugTrackingCameraFrustum(GWorld, Canvas->SceneView->ViewRotation, Canvas->SceneView->ViewLocation);
		}

		if(Canvas && Canvas->SceneView)
		{
			DrawSeaOfCubes(GWorld, Canvas->SceneView->ViewLocation);
		}
	}

#endif // #if !UE_BUILD_SHIPPING
}

float FGearVR::GetBatteryLevel() const
{
	return FAndroidMisc::GetBatteryState().Level;
}

float FGearVR::GetTemperatureInCelsius() const
{
	return FAndroidMisc::GetBatteryState().Temperature;
}

bool FGearVR::AreHeadPhonesPluggedIn() const
{
	return FAndroidMisc::AreHeadPhonesPluggedIn();
}

bool FGearVR::IsPowerLevelStateThrottled() const
{
	check(IsInGameThread());
	if (!IsInitialized())
	{
		return false;
	}

	return vrapi_GetSystemStatusInt(&JavaGT, VRAPI_SYS_STATUS_THROTTLED) != VRAPI_FALSE;
}

bool FGearVR::IsPowerLevelStateMinimum() const
{
	check(IsInGameThread());
	if (!IsInitialized())
	{
		return false;
	}

	return vrapi_GetSystemStatusInt(&JavaGT, VRAPI_SYS_STATUS_THROTTLED2) != VRAPI_FALSE;
}

void FGearVR::SetCPUAndGPULevels(int32 CPULevel, int32 GPULevel)
{
	check(IsInGameThread());
	UE_LOG(LogHMD, Log, TEXT("SetCPUAndGPULevels: Adjusting levels to CPU=%d - GPU=%d"), CPULevel, GPULevel);

	FSettings* CurrentSettings = GetSettings();
	CurrentSettings->CpuLevel = CPULevel;
	CurrentSettings->GpuLevel = GPULevel;
}

bool FGearVR::HasValidOvrMobile() const
{
	return pGearVRBridge->OvrMobile != nullptr;
}

void FGearVR::PushBlackFinal()
{
	check(IsInGameThread());

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(OVRPushBlackFinal,
	FCustomPresent*, pGearVRBridge, pGearVRBridge,
	FGameFrame*, Frame, GetGameFrame(),
	{
		pGearVRBridge->PushBlackFinal(Frame);
	});
	FlushRenderingCommands(); // wait till complete
}

void FGearVR::HandleBackButtonAction()
{
	check(IsInGameThread());
	if (BackButtonState == BACK_BUTTON_STATE_PENDING_DOUBLE_TAP)
	{
		UE_LOG(LogHMD, Log, TEXT("back button double tap") );
		BackButtonState = BACK_BUTTON_STATE_SKIP_UP;
	}
	else if (BackButtonState == BACK_BUTTON_STATE_PENDING_SHORT_PRESS && !BackButtonDown)
	{
		if ( ( vrapi_GetTimeInSeconds() - BackButtonDownStartTime ) > BUTTON_DOUBLE_TAP_TIME_IN_SECONDS )
		{
			UE_LOG(LogHMD, Log, TEXT("back button short press"));

			PushBlackFinal();

			UE_LOG(LogHMD, Log, TEXT("        SystemActivities_StartSystemActivity( %s )"), ANSI_TO_TCHAR(PUI_CONFIRM_QUIT));
			SystemActivities_StartSystemActivity( &JavaGT, PUI_CONFIRM_QUIT, NULL );
			BackButtonState = BACK_BUTTON_STATE_NONE;
		}
	}
	else if (BackButtonState == BACK_BUTTON_STATE_NONE && BackButtonDown)
	{
		if ( ( vrapi_GetTimeInSeconds() - BackButtonDownStartTime ) > BACK_BUTTON_LONG_PRESS_TIME_IN_SECONDS )
		{
			UE_LOG(LogHMD, Log, TEXT("back button long press"));
			UE_LOG(LogHMD, Log, TEXT("        ovrApp_PushBlackFinal()"));
			PushBlackFinal();
			UE_LOG(LogHMD, Log, TEXT("        SystemActivities_StartSystemActivity( %s )"), ANSI_TO_TCHAR(PUI_GLOBAL_MENU));
			SystemActivities_StartSystemActivity( &JavaGT, PUI_GLOBAL_MENU, NULL );
			BackButtonState = BACK_BUTTON_STATE_SKIP_UP;
		}
	}
}

bool FGearVR::HandleInputKey(UPlayerInput* pPlayerInput,
	const FKey& Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
	UE_LOG(LogHMD, Log, TEXT("KEY %s evt, evttype %d, gamepad %d"), *Key.ToString(), (int)EventType, int(bGamepad));
	if (CVarGearVRBackButton.GetValueOnAnyThread())
	{
		if (Key == EKeys::Android_Back && bGamepad)
		{
			if (EventType == IE_Pressed || EventType == IE_DoubleClick)
			{
				if (!BackButtonDown)
				{
					if ((vrapi_GetTimeInSeconds() - BackButtonDownStartTime ) < BUTTON_DOUBLE_TAP_TIME_IN_SECONDS)
					{
						BackButtonState = BACK_BUTTON_STATE_PENDING_DOUBLE_TAP;
					}
					BackButtonDownStartTime = vrapi_GetTimeInSeconds();
				}
				BackButtonDown = true;
			}
			else if (EventType == IE_Released)
			{
				if (BackButtonState == BACK_BUTTON_STATE_NONE)
				{
					if ( ( vrapi_GetTimeInSeconds() - BackButtonDownStartTime ) < BUTTON_SHORT_PRESS_TIME_IN_SECONDS )
					{
						BackButtonState = BACK_BUTTON_STATE_PENDING_SHORT_PRESS;
					}
				}
				else if (BackButtonState == BACK_BUTTON_STATE_SKIP_UP)
				{
					BackButtonState = BACK_BUTTON_STATE_NONE;
				}
				BackButtonDown = false;
			}
			return true;
		}
	}
	return false;
}

void FGearVR::OnBeginPlay(FWorldContext& InWorldContext)
{
	UE_LOG(LogHMD, Log, TEXT("FGearVR::OnBeginPlay"));
	CreateAndInitNewGameFrame(InWorldContext.World()->GetWorldSettings());
	check(Frame.IsValid());
	Frame->Flags.bOutOfFrame = true;
	Frame->Settings->Flags.bHeadTrackingEnforced = true; // to make sure HMD data is available

	// if we need to enable stereo then populate the frame with initial tracking data.
	// once this is done GetOrientationAndPosition will be able to return actual HMD / MC data (when requested
	// from BeginPlay event, for example).
	auto CurrentFrame = GetGameFrame();
	if (CurrentFrame && CurrentFrame->Flags.bOutOfFrame)
	{
		EnterVRMode();
		bool rv = GetEyePoses(*CurrentFrame, CurrentFrame->CurEyeRenderPose, CurrentFrame->CurSensorState);
		if (!rv)
		{
			UE_LOG(LogHMD, Log, TEXT("FGearVR::OnBeginPlay: Can't get Eye Poses, OvrMobile is %s"), HasValidOvrMobile() ? TEXT("OK") : TEXT("NULL"));
		}
		LeaveVRMode();
	}
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

//////////////////////////////////////////////////////////////////////////

#endif //GEARVR_SUPPORTED_PLATFORMS

//////////////////////////////////////////////////////////////////////////
void FGearVRPlugin::StartOVRGlobalMenu() const 
{
#if GEARVR_SUPPORTED_PLATFORMS
	check(IsInGameThread());
	IHeadMountedDisplay* HMD = GEngine->HMDDevice.Get();
	if (HMD && HMD->GetHMDDeviceType() == EHMDDeviceType::DT_GearVR)
	{
		FGearVR* OculusHMD = static_cast<FGearVR*>(HMD);

		OculusHMD->StartOVRGlobalMenu();
	}
#endif //GEARVR_SUPPORTED_PLATFORMS
}

void FGearVRPlugin::StartOVRQuitMenu() const 
{
#if GEARVR_SUPPORTED_PLATFORMS
	check(IsInGameThread());
	IHeadMountedDisplay* HMD = GEngine->HMDDevice.Get();
	if (HMD && HMD->GetHMDDeviceType() == EHMDDeviceType::DT_GearVR)
	{
		FGearVR* OculusHMD = static_cast<FGearVR*>(HMD);

		OculusHMD->StartOVRQuitMenu();
	}
#endif //GEARVR_SUPPORTED_PLATFORMS
}

void FGearVRPlugin::SetCPUAndGPULevels(int32 CPULevel, int32 GPULevel) const
{
#if GEARVR_SUPPORTED_PLATFORMS
	check(IsInGameThread());
	IHeadMountedDisplay* HMD = GEngine->HMDDevice.Get();
	if (HMD && HMD->GetHMDDeviceType() == EHMDDeviceType::DT_GearVR)
	{
		FGearVR* OculusHMD = static_cast<FGearVR*>(HMD);

		OculusHMD->SetCPUAndGPULevels(CPULevel, GPULevel);
	}
#endif //GEARVR_SUPPORTED_PLATFORMS
}

bool FGearVRPlugin::IsPowerLevelStateMinimum() const
{
#if GEARVR_SUPPORTED_PLATFORMS
	check(IsInGameThread());
	IHeadMountedDisplay* HMD = GEngine->HMDDevice.Get();
	if (HMD && HMD->GetHMDDeviceType() == EHMDDeviceType::DT_GearVR)
	{
		FGearVR* OculusHMD = static_cast<FGearVR*>(HMD);

		return OculusHMD->IsPowerLevelStateMinimum();
	}
#endif //GEARVR_SUPPORTED_PLATFORMS
	return false;
}

bool FGearVRPlugin::IsPowerLevelStateThrottled() const
{
#if GEARVR_SUPPORTED_PLATFORMS
	check(IsInGameThread());
	IHeadMountedDisplay* HMD = GEngine->HMDDevice.Get();
	if (HMD && HMD->GetHMDDeviceType() == EHMDDeviceType::DT_GearVR)
	{
		FGearVR* OculusHMD = static_cast<FGearVR*>(HMD);

		return OculusHMD->IsPowerLevelStateThrottled();
	}
#endif //GEARVR_SUPPORTED_PLATFORMS
	return false;
}

float FGearVRPlugin::GetTemperatureInCelsius() const
{
#if GEARVR_SUPPORTED_PLATFORMS
	check(IsInGameThread());
	IHeadMountedDisplay* HMD = GEngine->HMDDevice.Get();
	if (HMD && HMD->GetHMDDeviceType() == EHMDDeviceType::DT_GearVR)
	{
		FGearVR* OculusHMD = static_cast<FGearVR*>(HMD);

		return OculusHMD->GetTemperatureInCelsius();
	}
#endif //GEARVR_SUPPORTED_PLATFORMS
	return 0.f;
}

float FGearVRPlugin::GetBatteryLevel() const
{
#if GEARVR_SUPPORTED_PLATFORMS
	check(IsInGameThread());
	IHeadMountedDisplay* HMD = GEngine->HMDDevice.Get();
	if (HMD && HMD->GetHMDDeviceType() == EHMDDeviceType::DT_GearVR)
	{
		FGearVR* OculusHMD = static_cast<FGearVR*>(HMD);

		return OculusHMD->GetBatteryLevel();
	}
#endif //GEARVR_SUPPORTED_PLATFORMS
	return 0.f;
}

bool FGearVRPlugin::AreHeadPhonesPluggedIn() const
{
#if GEARVR_SUPPORTED_PLATFORMS
	check(IsInGameThread());
	IHeadMountedDisplay* HMD = GEngine->HMDDevice.Get();
	if (HMD && HMD->GetHMDDeviceType() == EHMDDeviceType::DT_GearVR)
	{
		FGearVR* OculusHMD = static_cast<FGearVR*>(HMD);

		return OculusHMD->AreHeadPhonesPluggedIn();
	}
#endif //GEARVR_SUPPORTED_PLATFORMS
	return false;
}

void FGearVRPlugin::SetLoadingIconTexture(FTextureRHIRef InTexture)
{
#if GEARVR_SUPPORTED_PLATFORMS
	check(IsInGameThread());
	IHeadMountedDisplay* HMD = GEngine->HMDDevice.Get();
	if (HMD && HMD->GetHMDDeviceType() == EHMDDeviceType::DT_GearVR)
	{
		FGearVR* OculusHMD = static_cast<FGearVR*>(HMD);

		OculusHMD->SetLoadingIconTexture(InTexture);
	}
#endif //GEARVR_SUPPORTED_PLATFORMS
}

void FGearVRPlugin::SetLoadingIconMode(bool bActiveLoadingIcon)
{
#if GEARVR_SUPPORTED_PLATFORMS
	check(IsInGameThread());
	IHeadMountedDisplay* HMD = GEngine->HMDDevice.Get();
	if (HMD && HMD->GetHMDDeviceType() == EHMDDeviceType::DT_GearVR)
	{
		FGearVR* OculusHMD = static_cast<FGearVR*>(HMD);

		OculusHMD->SetLoadingIconMode(bActiveLoadingIcon);
	}
#endif //GEARVR_SUPPORTED_PLATFORMS
}

void FGearVRPlugin::RenderLoadingIcon_RenderThread()
{
#if GEARVR_SUPPORTED_PLATFORMS
	check(IsInRenderingThread());
	IHeadMountedDisplay* HMD = GEngine->HMDDevice.Get();
	if (HMD && HMD->GetHMDDeviceType() == EHMDDeviceType::DT_GearVR)
	{
		FGearVR* OculusHMD = static_cast<FGearVR*>(HMD);

		OculusHMD->RenderLoadingIcon_RenderThread();
	}
#endif //GEARVR_SUPPORTED_PLATFORMS
}

bool FGearVRPlugin::IsInLoadingIconMode() const
{
#if GEARVR_SUPPORTED_PLATFORMS
	check(IsInGameThread());
	IHeadMountedDisplay* HMD = GEngine->HMDDevice.Get();
	if (HMD && HMD->GetHMDDeviceType() == EHMDDeviceType::DT_GearVR)
	{
		FGearVR* OculusHMD = static_cast<FGearVR*>(HMD);

		return OculusHMD->IsInLoadingIconMode();
	}
#endif //GEARVR_SUPPORTED_PLATFORMS
	return false;
}

#if GEARVR_SUPPORTED_PLATFORMS

#include <HeadMountedDisplayCommon.cpp>
#include <AsyncLoadingSplash.cpp>
#include <OculusStressTests.cpp>

#endif //GEARVR_SUPPORTED_PLATFORMS

