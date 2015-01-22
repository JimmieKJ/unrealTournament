// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "GearVRPrivatePCH.h"
#include "GearVR.h"
#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "Android/AndroidJNI.h"
#include "RHIStaticStates.h"


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
#endif // #if !UE_BUILD_SHIPPING


//---------------------------------------------------
// GearVR Plugin Implementation
//---------------------------------------------------

class FGearVRPlugin : public IGearVRPlugin
{
	/** IHeadMountedDisplayModule implementation */
	virtual TSharedPtr< class IHeadMountedDisplay > CreateHeadMountedDisplay() override;

	// Pre-init the HMD module
	virtual void PreInit() override;

	FString GetModulePriorityKeyName() const
	{
		return FString(TEXT("GearVR"));
	}
};

IMPLEMENT_MODULE( FGearVRPlugin, GearVR )

TSharedPtr< class IHeadMountedDisplay > FGearVRPlugin::CreateHeadMountedDisplay()
{
#if GEARVR_SUPPORTED_PLATFORMS
	TSharedPtr< FGearVR > GearVR(new FGearVR());
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
	FGearVR::PreInit();
#endif//GEARVR_SUPPORTED_PLATFORMS
}


#if GEARVR_SUPPORTED_PLATFORMS


class ConditionalLocker
{
public:
	ConditionalLocker(bool condition, OVR::Lock* plock) :
		pLock((condition) ? plock : NULL)
	{
		OVR_ASSERT(!condition || pLock);
		if (pLock)
			pLock->DoLock();
	}
	~ConditionalLocker()
	{
		if (pLock)
			pLock->Unlock();
	}
private:
	OVR::Lock*	pLock;
};


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
	}
}

bool FGearVR::IsHMDEnabled() const
{
	return bHMDEnabled;
}

void FGearVR::EnableHMD(bool enable)
{
	bHMDEnabled = enable;
	if (!bHMDEnabled)
	{
		EnableStereo(false);
	}
}

EHMDDeviceType::Type FGearVR::GetHMDDeviceType() const
{
	return EHMDDeviceType::DT_NoPost;
}

bool FGearVR::GetHMDMonitorInfo(MonitorInfo& MonitorDesc)
{
	//@if (IsInitialized())
	//{
	//	MonitorDesc.MonitorName = HmdDesc.DisplayDeviceName;
	//	MonitorDesc.MonitorId	= HmdDesc.DisplayId;
	//	MonitorDesc.DesktopX	= HmdDesc.WindowsPos.x;
	//	MonitorDesc.DesktopY	= HmdDesc.WindowsPos.y;
	//	MonitorDesc.ResolutionX = HmdDesc.Resolution.w;
	//	MonitorDesc.ResolutionY = HmdDesc.Resolution.h;
	//	return true;
	//}
	//else
	{
		MonitorDesc.MonitorName = "";
		MonitorDesc.MonitorId = 0;
		MonitorDesc.DesktopX = MonitorDesc.DesktopY = 0;
		MonitorDesc.ResolutionX = RenderTargetWidth;
		MonitorDesc.ResolutionY = RenderTargetHeight;
	}
	return true;
}

bool FGearVR::DoesSupportPositionalTracking() const
{
	return false;
}

bool FGearVR::HasValidTrackingPosition() const
{
	return false;
}

void FGearVR::GetPositionalTrackingCameraProperties(FVector& OutOrigin, FRotator& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const
{
}

bool FGearVR::IsInLowPersistenceMode() const
{
	return true;
}

void FGearVR::EnableLowPersistenceMode(bool Enable)
{
}

float FGearVR::GetInterpupillaryDistance() const
{
	return InterpupillaryDistance;
}

void FGearVR::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
	InterpupillaryDistance = NewInterpupillaryDistance;

	UpdateStereoRenderingParams();
}

void FGearVR::GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const
{
	OutHFOVInDegrees = FMath::RadiansToDegrees(HFOVInRadians);
	OutVFOVInDegrees = FMath::RadiansToDegrees(VFOVInRadians);
}

void FGearVR::PoseToOrientationAndPosition(const ovrPosef& InPose, FQuat& OutOrientation, FVector& OutPosition) const
{
	OutOrientation = ToFQuat(InPose.Orientation);

	// correct position according to BaseOrientation and BaseOffset. Note, if VISION is disabled then BaseOffset is always a zero vector.
	OutPosition = BaseOrientation.Inverse().RotateVector(ToFVector_M2U(Vector3f(InPose.Position) - BaseOffset));

	// apply base orientation correction to OutOrientation
	OutOrientation = BaseOrientation.Inverse() * OutOrientation;
	OutOrientation.Normalize();
}

void FGearVR::GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	const ovrSensorState ss = ovrHmd_GetSensorState(OvrHmd, ovr_GetTimeInSeconds() + MotionPredictionInSeconds, true);
	const ovrPosef& pose = ss.Predicted.Pose;

	PoseToOrientationAndPosition(pose, CurrentOrientation, CurrentPosition);
}

void FGearVR::UpdatePlayerViewPoint(const FQuat& CurrentOrientation, const FVector& CurrentPosition, 
									const FVector& LastHmdPosition, const FQuat& DeltaControlOrientation, 
									const FQuat& BaseViewOrientation, const FVector& BaseViewPosition, 
									FRotator& ViewRotation, FVector& ViewLocation)
{
	const FQuat DeltaOrient = BaseViewOrientation.Inverse() * CurrentOrientation;
	ViewRotation = FRotator(ViewRotation.Quaternion() * DeltaOrient);

	// Apply delta between current HMD position and the LastHmdPosition to ViewLocation
	const FVector vHMDPositionDelta = DeltaControlOrientation.RotateVector(CurrentPosition - LastHmdPosition);
	ViewLocation += vHMDPositionDelta;
}

void FGearVR::ApplyHmdRotation(APlayerController* PC, FRotator& ViewRotation)
{
	ConditionalLocker lock(bUpdateOnRT, &UpdateOnRTLock);

	ViewRotation.Normalize();

	GetCurrentOrientationAndPosition(CurHmdOrientation, CurHmdPosition);
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

void FGearVR::UpdatePlayerCameraRotation(APlayerCameraManager* Camera, struct FMinimalViewInfo& POV)
{
	ConditionalLocker lock(bUpdateOnRT, &UpdateOnRTLock);

	GetCurrentOrientationAndPosition(CurHmdOrientation, CurHmdPosition);
	LastHmdOrientation = CurHmdOrientation;

	static const FName NAME_Fixed = FName(TEXT("Fixed"));
	static const FName NAME_ThirdPerson = FName(TEXT("ThirdPerson"));
	static const FName NAME_FreeCam = FName(TEXT("FreeCam"));
	static const FName NAME_FreeCam_Default = FName(TEXT("FreeCam_Default"));
	static const FName NAME_FirstPerson = FName(TEXT("FirstPerson"));

	DeltaControlRotation = POV.Rotation;
	DeltaControlOrientation = DeltaControlRotation.Quaternion();

	// Apply HMD orientation to camera rotation.
	POV.Rotation = FRotator(POV.Rotation.Quaternion() * CurHmdOrientation);
}

bool FGearVR::IsChromaAbCorrectionEnabled() const
{
	return bChromaAbCorrectionEnabled;
}

ISceneViewExtension* FGearVR::GetViewExtension()
{
	return this;
}

bool FGearVR::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
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
		else if (FParse::Command( &Cmd, TEXT("RESET") ))
		{
			bOverrideStereo = false;
			bOverrideIPD = false;
			bWorldToMetersOverride = false;
			NearClippingPlane = FarClippingPlane = 0.f;
			InterpupillaryDistance = OVR_DEFAULT_IPD;
			UpdateStereoRenderingParams();
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("SHOW")))
		{
			Ar.Logf(TEXT("stereo ipd=%.4f hfov=%.3f vfov=%.3f\n nearPlane=%.4f farPlane=%.4f"), GetInterpupillaryDistance(),
				FMath::RadiansToDegrees(HFOVInRadians), FMath::RadiansToDegrees(VFOVInRadians),
				(NearClippingPlane) ? NearClippingPlane : GNearClippingPlane, FarClippingPlane);
		}

		// normal configuration
		float val;
		if (FParse::Value( Cmd, TEXT("E="), val))
		{
			SetInterpupillaryDistance( val );
			bOverrideIPD = true;
		}
		if (FParse::Value(Cmd, TEXT("FCP="), val)) // far clipping plane override
		{
			FarClippingPlane = val;
		}
		if (FParse::Value(Cmd, TEXT("NCP="), val)) // near clipping plane override
		{
			NearClippingPlane = val;
		}
		if (FParse::Value(Cmd, TEXT("W2M="), val))
		{
			WorldToMetersScale = val;
			bWorldToMetersOverride = true;
		}

		// debug configuration
		if (bDevSettingsEnabled)
		{
			float fov;
			if (FParse::Value(Cmd, TEXT("HFOV="), fov))
			{
				HFOVInRadians = FMath::DegreesToRadians(fov);
				bOverrideStereo = true;
			}
			else if (FParse::Value(Cmd, TEXT("VFOV="), fov))
			{
				VFOVInRadians = FMath::DegreesToRadians(fov);
				bOverrideStereo = true;
			}
		}
		return true;
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
		else if (FParse::Command(&Cmd, TEXT("VSYNC")))
		{
			if (FParse::Command( &Cmd, TEXT("RESET") ))
			{
				if (bStereoEnabled)
				{
					bVSync = bSavedVSync;
					ApplySystemOverridesOnStereo();
				}
				bOverrideVSync = false;
				return true;
			}
			else
			{
				if (FParse::Command(&Cmd, TEXT("ON")) || FParse::Command(&Cmd, TEXT("1")))
				{
					bVSync = true;
					bOverrideVSync = true;
					ApplySystemOverridesOnStereo();
					return true;
				}
				else if (FParse::Command(&Cmd, TEXT("OFF")) || FParse::Command(&Cmd, TEXT("0")))
				{
					bVSync = false;
					bOverrideVSync = true;
					ApplySystemOverridesOnStereo();
					return true;
				}
				else if (FParse::Command(&Cmd, TEXT("TOGGLE")) || FParse::Command(&Cmd, TEXT("")))
				{
					bVSync = !bVSync;
					bOverrideVSync = true;
					ApplySystemOverridesOnStereo();
					Ar.Logf(TEXT("VSync is currently %s"), (bVSync) ? TEXT("ON") : TEXT("OFF"));
					return true;
				}
			}
			return false;
		}
		else if (FParse::Command(&Cmd, TEXT("SP")) || 
				 FParse::Command(&Cmd, TEXT("SCREENPERCENTAGE")))
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (CmdName.IsEmpty())
				return false;

			if (!FCString::Stricmp(*CmdName, TEXT("RESET")))
			{
				bOverrideScreenPercentage = false;
				ApplySystemOverridesOnStereo();
			}
			else
			{
				float sp = FCString::Atof(*CmdName);
				if (sp >= 30 && sp <= 300)
				{
					bOverrideScreenPercentage = true;
					ScreenPercentage = sp;
					ApplySystemOverridesOnStereo();
				}
				else
				{
					Ar.Logf(TEXT("Value is out of range [30..300]"));
				}
			}
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("UPDATEONRT"))) // update on renderthread
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (!CmdName.IsEmpty())
			{
				if (!FCString::Stricmp(*CmdName, TEXT("ON")))
				{
					bUpdateOnRT = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					bUpdateOnRT = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE")))
				{
					bUpdateOnRT = !bUpdateOnRT;
				}
				else
				{
					return false;
				}
			}
			else
			{
				bUpdateOnRT = !bUpdateOnRT;
			}
			Ar.Logf(TEXT("Update on render thread is currently %s"), (bUpdateOnRT) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
	}
	else if (FParse::Command(&Cmd, TEXT("HMDPOS")))
	{
		if (FParse::Command(&Cmd, TEXT("RESET")))
		{
			FString YawStr = FParse::Token(Cmd, 0);
			float yaw = 0.f;
			if (!YawStr.IsEmpty())
			{
				yaw = FCString::Atof(*YawStr);
			}
			ResetOrientationAndPosition(yaw);
			return true;
		}
	}
	else if (FParse::Command(&Cmd, TEXT("OCULUSDEV")))
	{
		if (FParse::Command(&Cmd, TEXT("ON")))
		{
			bDevSettingsEnabled = true;
		}
		else if (FParse::Command(&Cmd, TEXT("OFF")))
		{
			bDevSettingsEnabled = false;
		}
		UpdateStereoRenderingParams();
		return true;
	}
	if (FParse::Command(&Cmd, TEXT("MOTION")))
	{
		FString CmdName = FParse::Token(Cmd, 0);
		if (CmdName.IsEmpty())
			return false;

		if (!FCString::Stricmp(*CmdName, TEXT("ON")))
		{
			bHeadTrackingEnforced = false;
			return true;
		}
		else if (!FCString::Stricmp(*CmdName, TEXT("ENFORCE")))
		{
			bHeadTrackingEnforced = !bHeadTrackingEnforced;
			if (!bHeadTrackingEnforced)
			{
				CurHmdOrientation = FQuat::Identity;
				ResetControlRotation();
			}
			return true;
		}
		else if (!FCString::Stricmp(*CmdName, TEXT("RESET")))
		{
			bHeadTrackingEnforced = false;
			CurHmdOrientation = FQuat::Identity;
			ResetControlRotation();
			return true;
		}
		else if (!FCString::Stricmp(*CmdName, TEXT("SHOW")))
		{
			if (MotionPredictionInSeconds > 0)
			{
				Ar.Logf(TEXT("motion prediction=%.3f"),	MotionPredictionInSeconds);
			}
			else
			{
				Ar.Logf(TEXT("motion prediction OFF"));
			}
	
			return true;
		}

		FString Value = FParse::Token(Cmd, 0);
		if (Value.IsEmpty())
		{
			return false;
		}
		if (!FCString::Stricmp(*CmdName, TEXT("PRED")))
		{
			if (!FCString::Stricmp(*Value, TEXT("OFF")))
			{
				MotionPredictionInSeconds = 0.0;
			}
			else if (!FCString::Stricmp(*Value, TEXT("OFF")))
			{
				MotionPredictionInSeconds = DEFAULT_PREDICTION_IN_SECONDS;
			}
			else
			{
				MotionPredictionInSeconds = FCString::Atod(*Value);
			}
			return true;
		}
		return false;
	}
	else if (FParse::Command(&Cmd, TEXT("SETFINISHFRAME")))
	{
		static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));

		if (FParse::Command(&Cmd, TEXT("ON")))
		{
			bAllowFinishCurrentFrame = true;
			if (bStereoEnabled)
			{
				CFinishFrameVar->Set(bAllowFinishCurrentFrame);
			}
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("OFF")))
		{
			bAllowFinishCurrentFrame = false;
			if (bStereoEnabled)
			{
				CFinishFrameVar->Set(bAllowFinishCurrentFrame);
			}
			return true;
		}
		return false;
	}
	else if (FParse::Command(&Cmd, TEXT("UNCAPFPS")))
	{
		GEngine->bSmoothFrameRate = false;
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("OVRVERSION")))
	{
		static const char* Results = OVR_VERSION_STRING;
		Ar.Logf(TEXT("%s, LibOVR: %s, built %s, %s"), *GEngineVersion.ToString(), UTF8_TO_TCHAR(Results), 
			UTF8_TO_TCHAR(__DATE__), UTF8_TO_TCHAR(__TIME__));
		return true;
	}

	return false;
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
	OVR_UNUSED(enable);
	return false;
}

//---------------------------------------------------
// GearVR IStereoRendering Implementation
//---------------------------------------------------
bool FGearVR::IsStereoEnabled() const
{
	return true;
/*	return bStereoEnabled && bHMDEnabled;*/
}

bool FGearVR::EnableStereo(bool stereo)
{
	return true;
// 	bStereoEnabled = (IsHMDEnabled()) ? stereo : false;
// 	OnOculusStateChange(bStereoEnabled);
// 	return bStereoEnabled;
}

void FGearVR::ResetControlRotation() const
{
	// Switching back to non-stereo mode: reset player rotation and aim.
	// Should we go through all playercontrollers here?
	APlayerController* pc = GEngine->GetFirstLocalPlayerController(GWorld);
	if (pc)
	{
		// Reset Aim? @todo
		FRotator r = pc->GetControlRotation();
		r.Normalize();
		// Reset roll and pitch of the player
		r.Roll = 0;
		r.Pitch = 0;
		pc->SetControlRotation(r);
	}
}

void FGearVR::OnOculusStateChange(bool bIsEnabledNow)
{
	if (!bIsEnabledNow)
	{
		// Switching from stereo
		ResetControlRotation();
		RestoreSystemValues();
	}
	else
	{
		SaveSystemValues();
		ApplySystemOverridesOnStereo(bIsEnabledNow);

		UpdateStereoRenderingParams();
	}
}

void FGearVR::ApplySystemOverridesOnStereo(bool bForce)
{
	if (bStereoEnabled || bForce)
	{
		// Set the current VSync state
		if (bOverrideVSync)
		{
			static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
			CVSyncVar->Set(bVSync);
		}
		else
		{
			static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
			bVSync = CVSyncVar->GetInt() != 0;
		}

		static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
		CFinishFrameVar->Set(bAllowFinishCurrentFrame);
	}
}

void FGearVR::SaveSystemValues()
{
	static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	bSavedVSync = CVSyncVar->GetInt() != 0;

	static IConsoleVariable* CScrPercVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	SavedScrPerc = CScrPercVar->GetFloat();
}

void FGearVR::RestoreSystemValues()
{
	static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	CVSyncVar->Set(bSavedVSync);

	static IConsoleVariable* CScrPercVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	CScrPercVar->Set(SavedScrPerc);

	static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
	CFinishFrameVar->Set(false);
}

void FGearVR::UpdateScreenSettings(const FViewport*)
{
	// Set the current ScreenPercentage state
	static IConsoleVariable* CScrPercVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	float DesiredSceeenPercentage;
	if (bOverrideScreenPercentage)
	{
		DesiredSceeenPercentage = ScreenPercentage;
	}
	else
	{
		DesiredSceeenPercentage = IdealScreenPercentage;
	}
	if (FMath::RoundToInt(CScrPercVar->GetFloat()) != FMath::RoundToInt(DesiredSceeenPercentage))
	{
		CScrPercVar->Set(DesiredSceeenPercentage);
	}
}

void FGearVR::AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
    SizeX = SizeX / 2;
    if( StereoPass == eSSP_RIGHT_EYE )
    {
        X += SizeX;
    }
}

void FGearVR::CalculateStereoViewOffset(const EStereoscopicPass StereoPassType, const FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
	ConditionalLocker lock(bUpdateOnRT, &UpdateOnRTLock);
	if (bNeedUpdateStereoRenderingParams)
		UpdateStereoRenderingParams();

	if( StereoPassType != eSSP_FULL )
	{
		check(WorldToMeters != 0.f)

		const int idx = (StereoPassType == eSSP_LEFT_EYE) ? 0 : 1;
		float EyeMul = (StereoPassType == eSSP_LEFT_EYE) ? -1.0f : 1.0f;
		const float PassEyeOffset = InterpupillaryDistance * EyeMul * 0.5f * WorldToMeters;

		const FVector TotalOffset = FVector(0, PassEyeOffset, 0);

		ViewLocation += ViewRotation.Quaternion().RotateVector(TotalOffset);

		// The HMDPosition already has HMD orientation applied.
		// Apply rotational difference between HMD orientation and ViewRotation
		// to HMDPosition vector. 

		const FVector vHMDPosition = DeltaControlOrientation.RotateVector(CurHmdPosition);
		ViewLocation += vHMDPosition;
		LastHmdPosition = CurHmdPosition;
	}
	else if (bHeadTrackingEnforced)
	{
		const FVector vHMDPosition = DeltaControlOrientation.RotateVector(CurHmdPosition);
		ViewLocation += vHMDPosition;
		LastHmdPosition = CurHmdPosition;
	}
}

void FGearVR::ResetOrientationAndPosition(float yaw)
{
	const ovrSensorState ss = ovrHmd_GetSensorState(OvrHmd, ovr_GetTimeInSeconds(), true);
	const ovrPosef& pose = ss.Recorded.Pose;
	const OVR::Quatf orientation = OVR::Quatf(pose.Orientation);

	// Reset position
	BaseOffset = OVR::Vector3f(0, 0, 0);

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

	BaseOrientation = ViewRotation.Quaternion();
}

FMatrix FGearVR::GetStereoProjectionMatrix(enum EStereoscopicPass StereoPassType, const float FOV) const
{
	const float ProjectionCenterOffset = 0.0f;
	const float PassProjectionOffset = (StereoPassType == eSSP_LEFT_EYE) ? ProjectionCenterOffset : -ProjectionCenterOffset;

	const float HalfFov = HFOVInRadians / 2.0f;
	const float InWidth = RenderTargetWidth / 2.0f;
	const float InHeight = RenderTargetHeight;
	const float XS = 1.0f / tan(HalfFov);
	const float YS = InWidth / tan(HalfFov) / InHeight;

	const float InNearZ = GNearClippingPlane;
	return FMatrix(
		FPlane(XS,                      0.0f,								    0.0f,							0.0f),
		FPlane(0.0f,					YS,	                                    0.0f,							0.0f),
		FPlane(0.0f,	                0.0f,								    0.0f,							1.0f),
		FPlane(0.0f,					0.0f,								    InNearZ,						0.0f))

		* FTranslationMatrix(FVector(PassProjectionOffset,0,0));
}

void FGearVR::InitCanvasFromView(FSceneView* InView, UCanvas* Canvas)
{
	// This is used for placing small HUDs (with names)
	// over other players (for example, in Capture Flag).
	// HmdOrientation should be initialized by GetCurrentOrientation (or
	// user's own value).
	FSceneView HmdView(*InView);

	UpdatePlayerViewPoint(Canvas->HmdOrientation, FVector(0.f), FVector::ZeroVector, FQuat::Identity, HmdView.BaseHmdOrientation, HmdView.BaseHmdLocation, HmdView.ViewRotation, HmdView.ViewLocation);

	HmdView.UpdateViewMatrix();
	Canvas->ViewProjectionMatrix = HmdView.ViewProjectionMatrix;
}

void FGearVR::PushViewportCanvas(EStereoscopicPass StereoPass, FCanvas *InCanvas, UCanvas *InCanvasObject, FViewport *InViewport) const
{
	if (StereoPass != eSSP_FULL)
	{
		int32 SideSizeX = FMath::TruncToInt(InViewport->GetSizeXY().X * 0.5);

		// !AB: temporarily assuming all canvases are at Z = 1.0f and calculating
		// stereo disparity right here. Stereo disparity should be calculated for each
		// element separately, considering its actual Z-depth.
		const float Z = 1.0f;
		float Disparity = Z * HudOffset + Z * CanvasCenterOffset;
		if (StereoPass == eSSP_RIGHT_EYE)
			Disparity = -Disparity;

		if (InCanvasObject)
		{
			//InCanvasObject->Init();
			InCanvasObject->SizeX = SideSizeX;
			InCanvasObject->SizeY = InViewport->GetSizeXY().Y;
			InCanvasObject->SetView(NULL);
			InCanvasObject->Update();
		}

		float ScaleFactor = 1.0f;
		FScaleMatrix m(ScaleFactor);

		InCanvas->PushAbsoluteTransform(FTranslationMatrix(
			FVector(((StereoPass == eSSP_RIGHT_EYE) ? SideSizeX : 0) + Disparity, 0, 0))*m);
	}
	else
	{
		FMatrix m;
		m.SetIdentity();
		InCanvas->PushAbsoluteTransform(m);
	}
}

void FGearVR::PushViewCanvas(EStereoscopicPass StereoPass, FCanvas *InCanvas, UCanvas *InCanvasObject, FSceneView *InView) const
{
	if (StereoPass != eSSP_FULL)
	{
		if (InCanvasObject)
		{
			//InCanvasObject->Init();
			InCanvasObject->SizeX = InView->ViewRect.Width();
			InCanvasObject->SizeY = InView->ViewRect.Height();
			InCanvasObject->SetView(InView);
			InCanvasObject->Update();
		}

		InCanvas->PushAbsoluteTransform(FTranslationMatrix(FVector(InView->ViewRect.Min.X, InView->ViewRect.Min.Y, 0)));
	}
	else
	{
		FMatrix m;
		m.SetIdentity();
		InCanvas->PushAbsoluteTransform(m);
	}
}


//---------------------------------------------------
// GearVR ISceneViewExtension Implementation
//---------------------------------------------------

void FGearVR::ModifyShowFlags(FEngineShowFlags& ShowFlags)
{
	ShowFlags.MotionBlur = 0;
	ShowFlags.HMDDistortion = false;
	ShowFlags.ScreenPercentage = true;
	ShowFlags.StereoRendering = IsStereoEnabled();
}

void FGearVR::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	InView.BaseHmdOrientation = LastHmdOrientation;
	InView.BaseHmdLocation = FVector(0.f);
	if (!bWorldToMetersOverride)
	{
		WorldToMetersScale = InView.WorldToMetersScale;
	}

	InViewFamily.bUseSeparateRenderTarget = ShouldUseSeparateRenderTarget();

	// check and save texture size. 
	if (InView.StereoPass == eSSP_LEFT_EYE)
	{
		if (EyeViewportSize != InView.ViewRect.Size())
		{
			EyeViewportSize = InView.ViewRect.Size();
			bNeedUpdateStereoRenderingParams = true;
		}
	}
}

bool FGearVR::IsHeadTrackingAllowed() const
{
	return bHeadTrackingEnforced || GEngine->IsStereoscopic3D();
}

//---------------------------------------------------
// GearVR Specific
//---------------------------------------------------

FGearVR::FGearVR()
	:
	InitStatus(0)
	, bStereoEnabled(false)
	, bHMDEnabled(true)
	, bNeedUpdateStereoRenderingParams(true)
    , bOverrideStereo(false)
	, bOverrideIPD(false)
	, bOverrideDistortion(false)
	, bDevSettingsEnabled(false)
	, bOverrideFOV(false)
	, bOverrideVSync(true)
	, bVSync(true)
	, bSavedVSync(false)
	, SavedScrPerc(100.f)
	, bOverrideScreenPercentage(false)
	, ScreenPercentage(100.f)
	, bAllowFinishCurrentFrame(false)
	, InterpupillaryDistance(OVR_DEFAULT_IPD)
	, WorldToMetersScale(100.f)
	, bWorldToMetersOverride(false)
	, UserDistanceToScreenModifier(0.f)
	, HFOVInRadians(FMath::DegreesToRadians(90.f))
	, VFOVInRadians(FMath::DegreesToRadians(90.f))
	, RenderTargetWidth(2048)
	, RenderTargetHeight(1024)
	, MotionPredictionInSeconds(DEFAULT_PREDICTION_IN_SECONDS)
	, bChromaAbCorrectionEnabled(true)
	, bOverride2D(false)
	, HudOffset(0.f)
	, CanvasCenterOffset(0.f)
	, bUpdateOnRT(true)
	, bHeadTrackingEnforced(false)
	, NearClippingPlane(0)
	, FarClippingPlane(0)
	, CurHmdOrientation(FQuat::Identity)
	, DeltaControlRotation(FRotator::ZeroRotator)
	, DeltaControlOrientation(FQuat::Identity)
	, CurHmdPosition(FVector::ZeroVector)
	, LastHmdOrientation(FQuat::Identity)
	, LastHmdPosition(FVector::ZeroVector)
	, BaseOffset(0, 0, 0)
	, BaseOrientation(FQuat::Identity)
	, OvrInited_RenderThread(0)
	, EyeViewportSize(0, 0)
	, RenderParams_RenderThread(getThis())
	, bHmdPosTracking(false)
{
	OvrMobile = nullptr;
	Startup();
}

FGearVR::~FGearVR()
{
	Shutdown();
}

bool FGearVR::IsInitialized() const
{
	return (InitStatus & eInitialized) != 0;
}

void FGearVR::Startup()
{
	// grab the clock settings out of the ini
	const TCHAR* GearVRSettings = TEXT("GearVR.Settings");
	int CpuLevel = 2;
	int GpuLevel = 2;
	GConfig->GetInt(GearVRSettings, TEXT("CpuLevel"), CpuLevel, GEngineIni);
	GConfig->GetInt(GearVRSettings, TEXT("GpuLevel"), GpuLevel, GEngineIni);

	UE_LOG(LogHMD, Log, TEXT("GearVR starting with CPU: %d GPU: %d"), CpuLevel, GpuLevel);

	FMemory::MemZero(VrModeParms);
	VrModeParms.AsynchronousTimeWarp = true;
	VrModeParms.DistortionFileName = NULL;
	VrModeParms.EnableImageServer = false;
	VrModeParms.GameThreadTid = gettid();
	VrModeParms.CpuLevel = CpuLevel;
	VrModeParms.GpuLevel = GpuLevel;
	VrModeParms.ActivityObject = FJavaWrapper::GameActivityThis;

	FPlatformMisc::MemoryBarrier();

	if (!IsRunningGame() || (InitStatus & eStartupExecuted) != 0)
	{
		// do not initialize plugin for server or if it was already initialized
		return;
	}
	InitStatus |= eStartupExecuted;

	// register our application lifetime delegates
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FGearVR::ApplicationPauseDelegate);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FGearVR::ApplicationResumeDelegate);

	InitStatus |= eInitialized;

	UpdateHmdRenderInfo();
	UpdateStereoRenderingParams();

	// Uncap fps to enable FPS higher than 62
	GEngine->bSmoothFrameRate = false;

	pGearVRBridge = new FGearVRBridge(this, RenderTargetWidth, RenderTargetHeight, HFOVInRadians);

	LoadFromIni();
	SaveSystemValues();
}

void FGearVR::Shutdown()
{
	if (!(InitStatus & eStartupExecuted))
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

	InitStatus = 0;
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
		pGearVRBridge = new FGearVRBridge(this, RenderTargetWidth, RenderTargetHeight, HFOVInRadians);
	}
}

void FGearVR::UpdateHmdRenderInfo()
{
#if 0 //@todo gearvr
	// Assuming we've successfully grabbed the device, read the configuration data from it, which we'll use for projection
	ovrHmd_GetDesc(OvrHmd, &HmdDesc);

	UE_LOG(LogHMD, Warning, TEXT("HMD %s, Monitor %s, res = %d x %d, windowPos = {%d, %d}"), ANSI_TO_TCHAR(HmdDesc.ProductName), 
		ANSI_TO_TCHAR(HmdDesc.DisplayDeviceName), HmdDesc.Resolution.w, HmdDesc.Resolution.h, HmdDesc.WindowsPos.x, HmdDesc.WindowsPos.y); 

	// Calc FOV
	if (!bOverrideFOV)
	{
		// Calc FOV, symmetrical, for each eye. 
		//@EyeFov[0] = SymmetricalFOV(HmdDesc.DefaultEyeFov[0]);
		//EyeFov[1] = SymmetricalFOV(HmdDesc.DefaultEyeFov[1]);

		//// Calc FOV in radians
		//@VFOVInRadians = FMath::Max(GetVerticalFovRadians(EyeFov[0]), GetVerticalFovRadians(EyeFov[1]));
		//HFOVInRadians = FMath::Max(GetHorizontalFovRadians(EyeFov[0]), GetHorizontalFovRadians(EyeFov[1]));
	}

	const Sizei recommenedTex0Size = ovrHmd_GetFovTextureSize(OvrHmd, ovrEye_Left, EyeFov[0], 1.0f);
	const Sizei recommenedTex1Size = ovrHmd_GetFovTextureSize(OvrHmd, ovrEye_Right, EyeFov[1], 1.0f);

	Sizei idealRenderTargetSize;
	idealRenderTargetSize.w = recommenedTex0Size.w + recommenedTex1Size.w;
	idealRenderTargetSize.h = FMath::Max(recommenedTex0Size.h, recommenedTex1Size.h);

	IdealScreenPercentage = FMath::Max(float(idealRenderTargetSize.w) / float(HmdDesc.Resolution.w) * 100.f,
									   float(idealRenderTargetSize.h) / float(HmdDesc.Resolution.h) * 100.f);

	// Override eye distance by the value from HMDInfo (stored in Profile).
	if (!bOverrideIPD)
	{
		InterpupillaryDistance = ovrHmd_GetFloat(OvrHmd, OVR_KEY_IPD, OVR_DEFAULT_IPD);
	}

	// Default texture size (per eye) is equal to half of W x H resolution. Will be overridden in SetupView.
	EyeViewportSize = FIntPoint(HmdDesc.Resolution.w / 2, HmdDesc.Resolution.h);

	bNeedUpdateStereoRenderingParams = true;
#endif
}

void FGearVR::UpdateStereoRenderingParams()
{
	// If we've manually overridden stereo rendering params for debugging, don't mess with them
	if (bOverrideStereo || !IsStereoEnabled())
	{
		return;
	}

	if (IsInitialized())
	{
		Lock::Locker lock(&StereoParamsLock);

		// 2D elements offset
		if (!bOverride2D)
		{
#if 0 //@todo: gearvr
			float ScreenSizeInMeters[2]; // 0 - width, 1 - height
			float LensSeparationInMeters;
			LensSeparationInMeters = ovrHmd_GetFloat(Hmd, "LensSeparation", 0);
			ovrHmd_GetFloatArray(Hmd, "ScreenSize", ScreenSizeInMeters, 2);

			// Recenter projection (meters)
			const float LeftProjCenterM = ScreenSizeInMeters[0] * 0.25f;
			const float LensRecenterM = LeftProjCenterM - LensSeparationInMeters * 0.5f;

			// Recenter projection (normalized)
			const float LensRecenter = 4.0f * LensRecenterM / ScreenSizeInMeters[0];

			HudOffset = 0.25f * InterpupillaryDistance * (HmdDesc.Resolution.w / ScreenSizeInMeters[0]) / 15.0f;
			CanvasCenterOffset = (0.25f * LensRecenter) * HmdDesc.Resolution.w;
#endif
		}
	}
	else
	{
		CanvasCenterOffset = 0.f;
	}

	bNeedUpdateStereoRenderingParams = false;
}

void FGearVR::LoadFromIni()
{
	const TCHAR* GearVRSettings = TEXT("GearVR.Settings");
	bool v;
	float f;
	if (GConfig->GetBool(GearVRSettings, TEXT("bChromaAbCorrectionEnabled"), v, GEngineIni))
	{
		bChromaAbCorrectionEnabled = v;
	}
	if (GConfig->GetBool(GearVRSettings, TEXT("bDevSettingsEnabled"), v, GEngineIni))
	{
		bDevSettingsEnabled = v;
	}
	if (GConfig->GetFloat(GearVRSettings, TEXT("MotionPrediction"), f, GEngineIni))
	{
		MotionPredictionInSeconds = f;
	}
	if (GConfig->GetBool(GearVRSettings, TEXT("bOverrideIPD"), v, GEngineIni))
	{
		bOverrideIPD = v;
		if (bOverrideIPD)
		{
			if (GConfig->GetFloat(GearVRSettings, TEXT("IPD"), f, GEngineIni))
			{
				SetInterpupillaryDistance(f);
			}
		}
	}
	if (GConfig->GetBool(GearVRSettings, TEXT("bOverrideStereo"), v, GEngineIni))
	{
		bOverrideStereo = v;
		if (bOverrideStereo)
		{
			if (GConfig->GetFloat(GearVRSettings, TEXT("HFOV"), f, GEngineIni))
			{
				HFOVInRadians = f;
			}
			if (GConfig->GetFloat(GearVRSettings, TEXT("VFOV"), f, GEngineIni))
			{
				VFOVInRadians = f;
			}
		}
	}
	if (GConfig->GetBool(GearVRSettings, TEXT("bOverrideVSync"), v, GEngineIni))
	{
		bOverrideVSync = v;
		if (GConfig->GetBool(GearVRSettings, TEXT("bVSync"), v, GEngineIni))
		{
			bVSync = v;
		}
	}
	if (GConfig->GetBool(GearVRSettings, TEXT("bOverrideScreenPercentage"), v, GEngineIni))
	{
		bOverrideScreenPercentage = v;
		if (GConfig->GetFloat(GearVRSettings, TEXT("ScreenPercentage"), f, GEngineIni))
		{
			ScreenPercentage = f;
		}
	}
	if (GConfig->GetBool(GearVRSettings, TEXT("bAllowFinishCurrentFrame"), v, GEngineIni))
	{
		bAllowFinishCurrentFrame = v;
	}
	if (GConfig->GetBool(GearVRSettings, TEXT("bUpdateOnRT"), v, GEngineIni))
	{
		bUpdateOnRT = v;
	}
	if (GConfig->GetFloat(GearVRSettings, TEXT("FarClippingPlane"), f, GEngineIni))
	{
		FarClippingPlane = f;
	}
	if (GConfig->GetFloat(GearVRSettings, TEXT("NearClippingPlane"), f, GEngineIni))
	{
		NearClippingPlane = f;
	}
}

void FGearVR::DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FSceneView& View, const FIntPoint& TextureSize)
{
	// no distortion mesh needed on GearVR
}

void FGearVR::GetEyeRenderParams_RenderThread(EStereoscopicPass StereoPass, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const
{
	// only used for postprocess distortion, not needed on GearVR
	EyeToSrcUVOffsetValue = FVector2D::ZeroVector;
	EyeToSrcUVScaleValue = FVector2D(1.0f, 1.0f);
}

void FGearVR::PreRenderView_RenderThread(FSceneView& View)
{
	check(IsInRenderingThread());

	if (!RenderParams_RenderThread.ShowFlags.Rendering)
		return;

	const ovrEyeType eyeIdx = (View.StereoPass == eSSP_LEFT_EYE) ? ovrEye_Left : ovrEye_Right;

	FQuat	CurrentHmdOrientation = RenderParams_RenderThread.CurHmdOrientation;
	FVector	CurrentHmdPosition = RenderParams_RenderThread.CurHmdPosition;

//  	// Get new predicted pose to corresponding eye.
//  	ovrPosef eyeRenderPose;//@ = ovrHmd_GetEyePose(Hmd, eyeIdx);
//  	PoseToOrientationAndPosition(eyeRenderPose, CurrentHmdOrientation, CurrentHmdPosition);
//  	RenderParams_RenderThread.EyeRenderPose[eyeIdx] = eyeRenderPose;

	if (bUpdateOnRT)
	{
		// Apply updated pose to corresponding View at recalc matrices.
		UpdatePlayerViewPoint(CurrentHmdOrientation, CurrentHmdPosition, 
			RenderParams_RenderThread.LastHmdPosition, RenderParams_RenderThread.DeltaControlOrientation,
			View.BaseHmdOrientation, View.BaseHmdLocation,
			View.ViewRotation, View.ViewLocation);
		View.UpdateViewMatrix();
	}
}

void FGearVR::PreRenderViewFamily_RenderThread(FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());

	if (pGearVRBridge->bFirstTime)
	{
		// enter vr mode
		OvrMobile = ovr_EnterVrMode(VrModeParms, &HmdInfo);
		pGearVRBridge->bFirstTime = false;
	}

	RenderParams_RenderThread.ShowFlags = ViewFamily.EngineShowFlags;

	RenderParams_RenderThread.bFrameBegun = true;

	// get latest orientation/position and cache it
	if (bUpdateOnRT)
	{
		Lock::Locker lock(&UpdateOnRTLock);

		const ovrSensorState ss = ovrHmd_GetSensorState(OvrHmd, ovr_GetTimeInSeconds(), true);
		const ovrPosef& pose = ss.Predicted.Pose;

		PoseToOrientationAndPosition(pose, RenderParams_RenderThread.CurHmdOrientation, RenderParams_RenderThread.CurHmdPosition);

		RenderParams_RenderThread.EyeRenderPose[0] = ss.Predicted;
		RenderParams_RenderThread.EyeRenderPose[1] = ss.Predicted;

		check(pGearVRBridge);
		pGearVRBridge->SwapParms.Images[0][0].Pose = ss.Predicted;
		pGearVRBridge->SwapParms.Images[1][0].Pose = ss.Predicted;

		RenderParams_RenderThread.LastHmdPosition = LastHmdPosition;
		RenderParams_RenderThread.DeltaControlOrientation = DeltaControlOrientation;
	}

	RenderParams_RenderThread.bFrameBegun = true;
}

void FGearVR::FinishRenderingFrame_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	if (RenderParams_RenderThread.bFrameBegun)
	{
		check(IsInRenderingThread());

		RenderParams_RenderThread.bFrameBegun = false;
	}
}



FGearVR::FRenderParams::FRenderParams(FGearVR* plugin)
	: 
	  CurHmdOrientation(FQuat::Identity)
	, CurHmdPosition(FVector::ZeroVector)
	, bFrameBegun(false)
	, ShowFlags(ESFIM_All0)
{
}

void FGearVR::CalculateRenderTargetSize(uint32& InOutSizeX, uint32& InOutSizeY) const
{
	check(IsInGameThread());

	InOutSizeX = RenderTargetWidth;
	InOutSizeY = RenderTargetHeight;
}

bool FGearVR::NeedReAllocateViewportRenderTarget(const FViewport& Viewport) const
{
	check(IsInGameThread());

	if (!IsStereoEnabled())
	{
		return false;
	}

	const uint32 InSizeX = Viewport.GetSizeXY().X;
	const uint32 InSizeY = Viewport.GetSizeXY().Y;
	FIntPoint RenderTargetSize;
	RenderTargetSize.X = Viewport.GetRenderTargetTexture()->GetSizeX();
	RenderTargetSize.Y = Viewport.GetRenderTargetTexture()->GetSizeY();

	uint32 NewSizeX = InSizeX, NewSizeY = InSizeY;
	CalculateRenderTargetSize(NewSizeX, NewSizeY);
	if (NewSizeX != RenderTargetSize.X || NewSizeY != RenderTargetSize.Y)
	{
		return true;
	}

	return false;
}

void FGearVR::ShutdownRendering()
{
	check(IsInRenderingThread());
	
	if (OvrMobile)
	{
		ovr_LeaveVrMode(OvrMobile);
		OvrMobile = nullptr;

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

void FGearVR::UpdateViewport(bool bUseSeparateRenderTarget, const FViewport& InViewport, SViewport* ViewportWidget)
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

	check(pGearVRBridge);

	const FTexture2DRHIRef& RT = InViewport.GetRenderTargetTexture();
	check(IsValidRef(RT));
	const FIntPoint NewEyeRTSize = FIntPoint((RT->GetSizeX() + 1) / 2, RT->GetSizeY());

	if (EyeViewportSize != NewEyeRTSize)
	{
		EyeViewportSize.X = NewEyeRTSize.X;
		EyeViewportSize.Y = NewEyeRTSize.Y;
		bNeedUpdateStereoRenderingParams = true;
	}

	if (bNeedUpdateStereoRenderingParams)
	{
		UpdateStereoRenderingParams();
	}

	pGearVRBridge->UpdateViewport(InViewport, ViewportRHI);
}


FGearVR::FGearVRBridge::FGearVRBridge(FGearVR* plugin, uint32 RenderTargetWidth, uint32 RenderTargetHeight, float FOV) :
	FRHICustomPresent(nullptr),
	Plugin(plugin), 
	bInitialized(false),
	RenderTargetWidth(RenderTargetWidth),
	RenderTargetHeight(RenderTargetHeight),
	FOV(FOV)
{
	Init();
}

void FGearVR::FGearVRBridge::BeginRendering()
{
	if (bInitialized)
	{
	}
}

void FGearVR::FGearVRBridge::DicedBlit(uint32 SourceX, uint32 SourceY, uint32 DestX, uint32 DestY, uint32 Width, uint32 Height, uint32 NumXSteps, uint32 NumYSteps)
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
		}
	}
}

void FGearVR::FGearVRBridge::FinishRendering()
{
	check(IsInRenderingThread());

	// initialize our eye textures if they don't exist yet
	if (SwapChainTextures[0][0] == 0)
	{
		// Initialize buffers to black
		const uint NumBytes = RenderTargetWidth * RenderTargetHeight * 4;
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
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RenderTargetWidth/2, RenderTargetHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, InitBuffer);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}
		}

		FMemory::Free(InitBuffer);

		glBindTexture( GL_TEXTURE_2D, 0 );
	}

	if (Plugin->RenderParams_RenderThread.bFrameBegun)
	{
 		// Finish the frame and let OVR do buffer swap (Present) and flush/sync.
		if (Plugin->OvrMobile)
		{
			uint32 CopyWidth = RenderTargetWidth / 2;
			uint32 CopyHeight = RenderTargetHeight;
			uint32 CurStartX = 0;

			// blit two 1022x1022 eye buffers into the swapparms textures
			for (uint32 Eye = 0; Eye < 2; ++Eye)
			{
				GLuint TexId = SwapChainTextures[Eye][CurrentSwapChainIndex];
				glBindTexture(GL_TEXTURE_2D, TexId );

				DicedBlit(CurStartX+1, 1, 1, 1, CopyWidth-2, CopyHeight-2, 1, 1);
				
				CurStartX += CopyWidth;
				SwapParms.Images[Eye][0].TexId = TexId;
			}

			glBindTexture(GL_TEXTURE_2D, 0 );

			ovr_WarpSwap(Plugin->OvrMobile, &SwapParms);
			CurrentSwapChainIndex = (CurrentSwapChainIndex + 1) % 3;
		}
	}
	else
	{
		UE_LOG(LogHMD, Warning, TEXT("Skipping frame: FinishRendering called with no corresponding BeginRendering (was BackBuffer re-allocated?)"));
	}
}

void FGearVR::FGearVRBridge::Init()
{
	bInitialized = true;
	bFirstTime = true;

	for (uint32 Eye = 0; Eye < 2; ++Eye)
	{
		for (int i = 0; i < 3; ++i)
		{
			SwapChainTextures[Eye][i] = 0;
		}
	}
}

void FGearVR::FGearVRBridge::Reset()
{
	check(IsInRenderingThread());

	if (SwapChainTextures[0][0] != 0)
	{
		glDeleteTextures(6, &SwapChainTextures[0][0]);
	}

	Plugin->RenderParams_RenderThread.bFrameBegun = false;
	bInitialized = false;
}

void FGearVR::FGearVRBridge::OnBackBufferResize()
{
	// if we are in the middle of rendering: prevent from calling EndFrame
	Plugin->RenderParams_RenderThread.bFrameBegun = false;
}

void FGearVR::FGearVRBridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI)
{
	check(IsInGameThread());
	check(ViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));
	const uint32 RTSizeX = RT->GetSizeX();
	const uint32 RTSizeY = RT->GetSizeY();
	GLuint RTTexId = *(GLuint*)RT->GetNativeResource();

	FMatrix ProjMat = Plugin->GetStereoProjectionMatrix(eSSP_LEFT_EYE, 90.0f);
	const Matrix4f proj = Plugin->ToMatrix4f(ProjMat);
 //	const Matrix4f proj = Matrix4f::PerspectiveRH( FOV, 1.0f, 1.0f, 100.0f);
 	SwapParms.Images[0][0].TexCoordsFromTanAngles = TanAngleMatrixFromProjection( proj );
// 	SwapParms.Images[0][0].TexId = RTTexId;
// 	SwapParms.Images[0][0].Pose = Plugin->RenderParams_RenderThread.EyeRenderPose[0];
// 
 	SwapParms.Images[1][0].TexCoordsFromTanAngles = TanAngleMatrixFromProjection( proj );
// 	SwapParms.Images[1][0].TexId = RTTexId;
// 	SwapParms.Images[1][0].Pose = Plugin->RenderParams_RenderThread.EyeRenderPose[1];

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

	this->ViewportRHI = ViewportRHI;
	ViewportRHI->SetCustomPresent(this);
}

bool FGearVR::FGearVRBridge::Present(int SyncInterval)
{
	check(IsInRenderingThread());

	FinishRendering();

	return false; // indicates that we are presenting here, UE shouldn't do Present.
}

#endif //GEARVR_SUPPORTED_PLATFORMS
