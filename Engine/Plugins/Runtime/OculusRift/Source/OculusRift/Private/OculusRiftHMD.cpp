// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OculusRiftPrivate.h"
#include "OculusRiftHMD.h"
#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "SceneViewport.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#endif

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
// Oculus Rift Plugin Implementation
//---------------------------------------------------

class FOculusRiftPlugin : public IOculusRiftPlugin
{
	/** IHeadMountedDisplayModule implementation */
	virtual TSharedPtr< class IHeadMountedDisplay > CreateHeadMountedDisplay() override;

	// Pre-init the HMD module (optional).
	virtual void PreInit() override;

	FString GetModulePriorityKeyName() const
	{
		return FString(TEXT("OculusRift"));
	}
};

IMPLEMENT_MODULE( FOculusRiftPlugin, OculusRift )

TSharedPtr< class IHeadMountedDisplay > FOculusRiftPlugin::CreateHeadMountedDisplay()
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	TSharedPtr< FOculusRiftHMD > OculusRiftHMD( new FOculusRiftHMD() );
	if( OculusRiftHMD->IsInitialized() )
	{
		return OculusRiftHMD;
	}
#endif//OCULUS_RIFT_SUPPORTED_PLATFORMS
	return NULL;
}

void FOculusRiftPlugin::PreInit()
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	FOculusRiftHMD::PreInit();
#endif//OCULUS_RIFT_SUPPORTED_PLATFORMS
}

//---------------------------------------------------
// Oculus Rift IHeadMountedDisplay Implementation
//---------------------------------------------------

#if OCULUS_RIFT_SUPPORTED_PLATFORMS

#if !UE_BUILD_SHIPPING
//////////////////////////////////////////////////////////////////////////
class OculusLog : public OVR::Log
{
public:
	OculusLog()
	{
		SetLoggingMask(OVR::LogMask_Debug | OVR::LogMask_Regular);
	}

	// This virtual function receives all the messages,
	// developers should override this function in order to do custom logging
	virtual void    LogMessageVarg(LogMessageType messageType, const char* fmt, va_list argList)
	{
		if ((messageType & GetLoggingMask()) == 0)
			return;

		ANSICHAR buf[1024];
		int32 len = FCStringAnsi::GetVarArgs(buf, sizeof(buf), sizeof(buf) / sizeof(ANSICHAR), fmt, argList);
		if (len > 0 && buf[len - 1] == '\n') // truncate the trailing new-line char, since Logf adds its own
			buf[len - 1] = '\0';
		TCHAR* tbuf = ANSI_TO_TCHAR(buf);
		GLog->Logf(TEXT("OCULUS: %s"), tbuf);
	}
};

#endif

//////////////////////////////////////////////////////////////////////////
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

void FOculusRiftHMD::PreInit()
{
	ovr_Initialize();
}

bool FOculusRiftHMD::IsHMDConnected()
{
	if (IsHMDEnabled())
	{
		InitDevice();
		return Hmd != nullptr;
	}
	return false;
}

bool FOculusRiftHMD::IsHMDEnabled() const
{
	return Flags.bHMDEnabled;
}

void FOculusRiftHMD::EnableHMD(bool enable)
{
	Flags.bHMDEnabled = enable;
	if (!Flags.bHMDEnabled)
	{
		EnableStereo(false);
	}
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
		MonitorDesc.WindowSizeX = MirrorWindowSize.X;
		MonitorDesc.WindowSizeY = MirrorWindowSize.Y;
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
	InitDevice();	
	return ((Hmd && (Hmd->HmdCaps & ovrHmdCap_ExtendDesktop) != 0) || !Hmd) ? true : false;
}

bool FOculusRiftHMD::DoesSupportPositionalTracking() const
{
#ifdef OVR_VISION_ENABLED
	 return Flags.bHmdPosTracking && (SupportedTrackingCaps & ovrTrackingCap_Position) != 0;
#else
	return false;
#endif //OVR_VISION_ENABLED
}

bool FOculusRiftHMD::HasValidTrackingPosition() const
{
#ifdef OVR_VISION_ENABLED
	return Flags.bHmdPosTracking && Flags.bHaveVisionTracking;
#else
	return false;
#endif //OVR_VISION_ENABLED
}

#define TRACKER_FOCAL_DISTANCE			1.00f // meters (focal point to origin for position)

void FOculusRiftHMD::GetPositionalTrackingCameraProperties(FVector& OutOrigin, FRotator& OutOrientation, float& OutHFOV, float& OutVFOV, float& OutCameraDistance, float& OutNearPlane, float& OutFarPlane) const
{
	OutOrigin = FVector::ZeroVector;
	OutOrientation = FRotator::ZeroRotator;
	OutHFOV = OutVFOV = OutCameraDistance = OutNearPlane = OutFarPlane = 0;

	if (!Hmd)
	{
		return;
	}

	OutCameraDistance = TRACKER_FOCAL_DISTANCE * WorldToMetersScale;
	OutHFOV = FMath::RadiansToDegrees(Hmd->CameraFrustumHFovInRadians);
	OutVFOV = FMath::RadiansToDegrees(Hmd->CameraFrustumVFovInRadians);
	OutNearPlane = Hmd->CameraFrustumNearZInMeters * WorldToMetersScale;
	OutFarPlane = Hmd->CameraFrustumFarZInMeters * WorldToMetersScale;

	// Read the camera pose
	const ovrTrackingState ss = ovrHmd_GetTrackingState(Hmd, ovr_GetTimeInSeconds());
	if (!(ss.StatusFlags & ovrStatus_CameraPoseTracked))
	{
		return;
	}
	const ovrPosef& cameraPose = ss.CameraPose;

	FQuat Orient;
	FVector Pos;
	PoseToOrientationAndPosition(cameraPose, Orient, Pos);
	OutOrientation = (DeltaControlOrientation * Orient).Rotator();
	OutOrigin = DeltaControlOrientation.RotateVector(Pos) + PositionOffset;
}

bool FOculusRiftHMD::IsInLowPersistenceMode() const
{
	return Flags.bLowPersistenceMode && (SupportedHmdCaps & ovrHmdCap_LowPersistence) != 0;
}

void FOculusRiftHMD::EnableLowPersistenceMode(bool Enable)
{
	Flags.bLowPersistenceMode = Enable;
	UpdateHmdCaps();
}

float FOculusRiftHMD::GetInterpupillaryDistance() const
{
	return InterpupillaryDistance;
}

void FOculusRiftHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
	InterpupillaryDistance = NewInterpupillaryDistance;

	UpdateStereoRenderingParams();
}

void FOculusRiftHMD::GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const
{
	OutHFOVInDegrees = FMath::RadiansToDegrees(HFOVInRadians);
	OutVFOVInDegrees = FMath::RadiansToDegrees(VFOVInRadians);
}

void FOculusRiftHMD::PoseToOrientationAndPosition(const ovrPosef& InPose, FQuat& OutOrientation, FVector& OutPosition) const
{
	OutOrientation = ToFQuat(InPose.Orientation);

	// correct position according to BaseOrientation and BaseOffset. 
	const FVector Pos = ToFVector_M2U(Vector3f(InPose.Position)) - ToFVector_M2U(BaseOffset);
	OutPosition = BaseOrientation.Inverse().RotateVector(Pos);

	// apply base orientation correction to OutOrientation
	OutOrientation = BaseOrientation.Inverse() * OutOrientation;
	OutOrientation.Normalize();
}

void FOculusRiftHMD::GetCurrentOrientationAndPosition(FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	// only supposed to be used from the game thread
	checkf(IsInGameThread());
	GetCurrentPose(CurrentOrientation, CurrentPosition);
	LastHmdOrientation = CurrentOrientation;
}

void FOculusRiftHMD::GetCurrentPose(FQuat& CurrentHmdOrientation, FVector& CurrentHmdPosition)
{
	check(IsInGameThread());
	check(Hmd);

	if (Flags.bNeedUpdateStereoRenderingParams)
		UpdateStereoRenderingParams();

	// Save eye poses
	ovrTrackingState ts;
	ovrVector3f hmdToEyeViewOffset[2] = { EyeRenderDesc[0].HmdToEyeViewOffset, EyeRenderDesc[1].HmdToEyeViewOffset };
	ovrHmd_GetEyePoses(Hmd, GFrameNumber, hmdToEyeViewOffset, EyeRenderPose, &ts);

	const ovrPosef& ThePose = ts.HeadPose.ThePose;
	PoseToOrientationAndPosition(ThePose, CurrentHmdOrientation, CurrentHmdPosition);
	//UE_LOG(LogHMD, Log, TEXT("P: %.3f %.3f %.3f"), CurrentPosition.X, CurrentPosition.Y, CurrentPosition.Y);

	LastFrameNumber = GFrameNumber;

#ifdef OVR_VISION_ENABLED
	if (Flags.bHmdPosTracking)
	{
#if !UE_BUILD_SHIPPING
		bool hadVisionTracking = Flags.bHaveVisionTracking;
		Flags.bHaveVisionTracking = (ts.StatusFlags & ovrStatus_PositionTracked) != 0;
		if (Flags.bHaveVisionTracking && !hadVisionTracking)
			UE_LOG(LogHMD, Warning, TEXT("Vision Tracking Acquired"));
		if (!Flags.bHaveVisionTracking && hadVisionTracking)
			UE_LOG(LogHMD, Warning, TEXT("Lost Vision Tracking"));
#endif // #if !UE_BUILD_SHIPPING
	}
#endif // OVR_VISION_ENABLED
}

void FOculusRiftHMD::ApplyHmdRotation(APlayerController* PC, FRotator& ViewRotation)
{
#if !UE_BUILD_SHIPPING
	if (Flags.bDoNotUpdateOnGT)
		return;
#endif

	ViewRotation.Normalize();

	FQuat CurHmdOrientation;
	FVector CurHmdPosition;
	GetCurrentPose(CurHmdOrientation, CurHmdPosition);
	LastHmdOrientation = CurHmdOrientation;

	const FRotator DeltaRot = ViewRotation - PC->GetControlRotation();
	DeltaControlRotation = (DeltaControlRotation + DeltaRot).GetNormalized();

	// Pitch from other sources is never good, because there is an absolute up and down that must be respected to avoid motion sickness.
	// Same with roll.
	DeltaControlRotation.Pitch = 0;
	DeltaControlRotation.Roll = 0;
	{
		ConditionalLocker lock(Flags.bUpdateOnRT, &StereoParamsLock);
		DeltaControlOrientation = DeltaControlRotation.Quaternion();
	}

	ViewRotation = FRotator(DeltaControlOrientation * CurHmdOrientation);

#if !UE_BUILD_SHIPPING
	if (Flags.bDrawTrackingCameraFrustum && PC->GetPawnOrSpectator())
	{
		DrawDebugTrackingCameraFrustum(PC->GetWorld(), PC->GetPawnOrSpectator()->GetPawnViewLocation());
	}
#endif
}

void FOculusRiftHMD::UpdatePlayerCameraRotation(APlayerCameraManager* Camera, struct FMinimalViewInfo& POV)
{
#if !UE_BUILD_SHIPPING
	if (Flags.bDoNotUpdateOnGT)
		return;
#endif
	FQuat	CurHmdOrientation;
	FVector CurHmdPosition;
	GetCurrentPose(CurHmdOrientation, CurHmdPosition);
	LastHmdOrientation = CurHmdOrientation;

	static const FName NAME_Fixed = FName(TEXT("Fixed"));
	static const FName NAME_ThirdPerson = FName(TEXT("ThirdPerson"));
	static const FName NAME_FreeCam = FName(TEXT("FreeCam"));
	static const FName NAME_FreeCam_Default = FName(TEXT("FreeCam_Default"));
	static const FName NAME_FirstPerson = FName(TEXT("FirstPerson"));

	DeltaControlRotation = POV.Rotation;
	{
		ConditionalLocker lock(Flags.bUpdateOnRT, &StereoParamsLock);
		DeltaControlOrientation = DeltaControlRotation.Quaternion();
	}

	// Apply HMD orientation to camera rotation.
	POV.Rotation = FRotator(POV.Rotation.Quaternion() * CurHmdOrientation);

#if !UE_BUILD_SHIPPING
	if (Flags.bDrawTrackingCameraFrustum)
	{
		DrawDebugTrackingCameraFrustum(Camera->GetWorld(), POV.Location);
	}
#endif
}

#if !UE_BUILD_SHIPPING
void FOculusRiftHMD::DrawDebugTrackingCameraFrustum(UWorld* World, const FVector& ViewLocation)
{
	const FColor c = (HasValidTrackingPosition() ? FColor::Green : FColor::Red);
	FVector origin;
	FRotator rotation;
	float hfovDeg, vfovDeg, nearPlane, farPlane, cameraDist;
	GetPositionalTrackingCameraProperties(origin, rotation, hfovDeg, vfovDeg, cameraDist, nearPlane, farPlane);

	// Level line
	//DrawDebugLine(World, ViewLocation, FVector(ViewLocation.X + 1000, ViewLocation.Y, ViewLocation.Z), FColor::Blue);

	const float hfov = FMath::DegreesToRadians(hfovDeg * 0.5f);
	const float vfov = FMath::DegreesToRadians(vfovDeg * 0.5f);
	FVector coneTop(0, 0, 0);
	FVector coneBase1(-farPlane, farPlane * FMath::Tan(hfov), farPlane * FMath::Tan(vfov));
	FVector coneBase2(-farPlane,-farPlane * FMath::Tan(hfov), farPlane * FMath::Tan(vfov));
	FVector coneBase3(-farPlane,-farPlane * FMath::Tan(hfov),-farPlane * FMath::Tan(vfov));
	FVector coneBase4(-farPlane, farPlane * FMath::Tan(hfov),-farPlane * FMath::Tan(vfov));
	FMatrix m(FMatrix::Identity);
	m *= FRotationMatrix(rotation);
	m *= FTranslationMatrix(origin);
	m *= FTranslationMatrix(ViewLocation); // to location of pawn
	coneTop = m.TransformPosition(coneTop);
	coneBase1 = m.TransformPosition(coneBase1);
	coneBase2 = m.TransformPosition(coneBase2);
	coneBase3 = m.TransformPosition(coneBase3);
	coneBase4 = m.TransformPosition(coneBase4);

	// draw a point at the camera pos
	DrawDebugPoint(World, coneTop, 5, c);

	// draw main pyramid, from top to base
	DrawDebugLine(World, coneTop, coneBase1, c);
	DrawDebugLine(World, coneTop, coneBase2, c);
	DrawDebugLine(World, coneTop, coneBase3, c);
	DrawDebugLine(World, coneTop, coneBase4, c);
											  
	// draw base (far plane)				  
	DrawDebugLine(World, coneBase1, coneBase2, c);
	DrawDebugLine(World, coneBase2, coneBase3, c);
	DrawDebugLine(World, coneBase3, coneBase4, c);
	DrawDebugLine(World, coneBase4, coneBase1, c);

	// draw near plane
	FVector coneNear1(-nearPlane, nearPlane * FMath::Tan(hfov), nearPlane * FMath::Tan(vfov));
	FVector coneNear2(-nearPlane,-nearPlane * FMath::Tan(hfov), nearPlane * FMath::Tan(vfov));
	FVector coneNear3(-nearPlane,-nearPlane * FMath::Tan(hfov),-nearPlane * FMath::Tan(vfov));
	FVector coneNear4(-nearPlane, nearPlane * FMath::Tan(hfov),-nearPlane * FMath::Tan(vfov));
	coneNear1 = m.TransformPosition(coneNear1);
	coneNear2 = m.TransformPosition(coneNear2);
	coneNear3 = m.TransformPosition(coneNear3);
	coneNear4 = m.TransformPosition(coneNear4);
	DrawDebugLine(World, coneNear1, coneNear2, c);
	DrawDebugLine(World, coneNear2, coneNear3, c);
	DrawDebugLine(World, coneNear3, coneNear4, c);
	DrawDebugLine(World, coneNear4, coneNear1, c);

	// center line
	FVector centerLine(-cameraDist, 0, 0);
	centerLine = m.TransformPosition(centerLine);
	DrawDebugLine(World, coneTop, centerLine, FColor::Yellow);
	DrawDebugPoint(World, centerLine, 5, FColor::Yellow);
}
#endif // #if !UE_BUILD_SHIPPING

bool FOculusRiftHMD::IsChromaAbCorrectionEnabled() const
{
	return Flags.bChromaAbCorrectionEnabled;
}

ISceneViewExtension* FOculusRiftHMD::GetViewExtension()
{
	return this;
}

bool FOculusRiftHMD::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (FParse::Command( &Cmd, TEXT("STEREO") ))
	{
		if (FParse::Command(&Cmd, TEXT("OFF")))
		{
			DoEnableStereo(false, true);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("TOGGLE")))
		{
			EnableStereo(!IsStereoEnabled());
			return true;
		}
		else if (FParse::Command( &Cmd, TEXT("RESET") ))
		{
			Flags.bNeedUpdateStereoRenderingParams = true;
			Flags.bOverrideStereo = false;
			Flags.bOverrideIPD = false;
			Flags.bWorldToMetersOverride = false;
			NearClippingPlane = FarClippingPlane = 0.f;
			Flags.bClippingPlanesOverride = true; // forces zeros to be written to ini file to use default values next run
			InterpupillaryDistance = ovrHmd_GetFloat(Hmd, OVR_KEY_IPD, OVR_DEFAULT_IPD);
			UpdateStereoRenderingParams();
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("SHOW")))
		{
			Ar.Logf(TEXT("stereo ipd=%.4f hfov=%.3f vfov=%.3f\n nearPlane=%.4f farPlane=%.4f"), GetInterpupillaryDistance(),
				FMath::RadiansToDegrees(HFOVInRadians), FMath::RadiansToDegrees(VFOVInRadians),
				(NearClippingPlane) ? NearClippingPlane : GNearClippingPlane, FarClippingPlane);
		}
		else
		{
			bool on, hmd = false;
			on = FParse::Command(&Cmd, TEXT("ON"));
			if (!on)
			{
				hmd = FParse::Command(&Cmd, TEXT("HMD"));
			}
			if (on || hmd)
			{
				if (!IsHMDEnabled())
				{
					Ar.Logf(TEXT("HMD is disabled. Use 'hmd enable' to re-enable it."));
				}
				DoEnableStereo(true, hmd);
				if (!Flags.bStereoEnabled)
				{
					Ar.Logf(TEXT("Stereo can't be enabled (is HMD connected?)"));
				}
				return true;
			}
		}

		// normal configuration
		float val;
		if (FParse::Value( Cmd, TEXT("E="), val))
		{
			SetInterpupillaryDistance( val );
			Flags.bOverrideIPD = true;
			Flags.bNeedUpdateStereoRenderingParams = true;
		}
		if (FParse::Value(Cmd, TEXT("FCP="), val)) // far clipping plane override
		{
			FarClippingPlane = val;
			Flags.bClippingPlanesOverride = true;
		}
		if (FParse::Value(Cmd, TEXT("NCP="), val)) // near clipping plane override
		{
			NearClippingPlane = val;
			Flags.bClippingPlanesOverride = true;
		}
		if (FParse::Value(Cmd, TEXT("W2M="), val))
		{
			WorldToMetersScale = val;
			Flags.bWorldToMetersOverride = true;
		}

		// debug configuration
		if (Flags.bDevSettingsEnabled)
		{
			float fov;
			if (FParse::Value(Cmd, TEXT("HFOV="), fov))
			{
				HFOVInRadians = FMath::DegreesToRadians(fov);
				Flags.bOverrideStereo = true;
			}
			else if (FParse::Value(Cmd, TEXT("VFOV="), fov))
			{
				VFOVInRadians = FMath::DegreesToRadians(fov);
				Flags.bOverrideStereo = true;
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
#ifdef OVR_SDK_RENDERING
		else if (FParse::Command(&Cmd, TEXT("AFR")))
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (CmdName.IsEmpty())
			{
				AlternateFrameRateDivider = 1;
			}
			else
			{
				AlternateFrameRateDivider = FCString::Atof(*CmdName);
			}

			return true;
		}
#endif // #ifdef OVR_SDK_RENDERING
		else if (FParse::Command(&Cmd, TEXT("VSYNC")))
		{
			if (FParse::Command( &Cmd, TEXT("RESET") ))
			{
				if (Flags.bStereoEnabled)
				{
					Flags.bVSync = Flags.bSavedVSync;
					ApplySystemOverridesOnStereo();
				}
				Flags.bOverrideVSync = false;
				return true;
			}
			else
			{
				if (FParse::Command(&Cmd, TEXT("ON")) || FParse::Command(&Cmd, TEXT("1")))
				{
					Flags.bVSync = true;
					Flags.bOverrideVSync = true;
					ApplySystemOverridesOnStereo();
					return true;
				}
				else if (FParse::Command(&Cmd, TEXT("OFF")) || FParse::Command(&Cmd, TEXT("0")))
				{
					Flags.bVSync = false;
					Flags.bOverrideVSync = true;
					ApplySystemOverridesOnStereo();
					return true;
				}
				else if (FParse::Command(&Cmd, TEXT("TOGGLE")) || FParse::Command(&Cmd, TEXT("")))
				{
					Flags.bVSync = !Flags.bVSync;
					Flags.bOverrideVSync = true;
					ApplySystemOverridesOnStereo();
					Ar.Logf(TEXT("VSync is currently %s"), (Flags.bVSync) ? TEXT("ON") : TEXT("OFF"));
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
				Flags.bOverrideScreenPercentage = false;
				ApplySystemOverridesOnStereo();
			}
			else
			{
				float sp = FCString::Atof(*CmdName);
				if (sp >= 30 && sp <= 300)
				{
					Flags.bOverrideScreenPercentage = true;
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
		else if (FParse::Command(&Cmd, TEXT("LP"))) // low persistence mode
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (!CmdName.IsEmpty())
			{
				if (!FCString::Stricmp(*CmdName, TEXT("ON")))
				{
					Flags.bLowPersistenceMode = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Flags.bLowPersistenceMode = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE")))
				{
					Flags.bLowPersistenceMode = !Flags.bLowPersistenceMode;
				}
				else
				{
					return false;
				}
			}
			else
			{
				Flags.bLowPersistenceMode = !Flags.bLowPersistenceMode;
			}
			UpdateHmdCaps();
			Ar.Logf(TEXT("Low Persistence is currently %s"), (Flags.bLowPersistenceMode) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("MIRROR"))) // to mirror or not to mirror?...
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (!CmdName.IsEmpty())
			{
				if (!FCString::Stricmp(*CmdName, TEXT("ON")))
				{
					Flags.bMirrorToWindow = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Flags.bMirrorToWindow = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE"))) 
				{
					Flags.bMirrorToWindow = !Flags.bMirrorToWindow;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("RESET")))
				{
					Flags.bMirrorToWindow = true;
					MirrorWindowSize.X = MirrorWindowSize.Y = 0;
				}
				else
				{
					int32 X = FCString::Atoi(*CmdName);
					const TCHAR* CmdTemp = FCString::Strchr(*CmdName, 'x') ? FCString::Strchr(*CmdName, 'x') + 1 : FCString::Strchr(*CmdName, 'X') ? FCString::Strchr(*CmdName, 'X') + 1 : TEXT("");
					int32 Y = FCString::Atoi(CmdTemp);

					MirrorWindowSize.X = X;
					MirrorWindowSize.Y = Y; 
				}
			}
			else
			{
				Flags.bMirrorToWindow = !Flags.bMirrorToWindow;
			}
			UpdateHmdCaps();
			Ar.Logf(TEXT("Mirroring is currently %s"), (Flags.bMirrorToWindow) ? TEXT("ON") : TEXT("OFF"));
			if (Flags.bMirrorToWindow && (MirrorWindowSize.X != 0 || MirrorWindowSize.Y != 0))
			{
				Ar.Logf(TEXT("Mirror window size is %d x %d"), MirrorWindowSize.X, MirrorWindowSize.Y);
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
					Flags.bUpdateOnRT = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Flags.bUpdateOnRT = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE")))
				{
					Flags.bUpdateOnRT = !Flags.bUpdateOnRT;
				}
				else
				{
					return false;
				}
			}
			else
			{
				Flags.bUpdateOnRT = !Flags.bUpdateOnRT;
			}
			Ar.Logf(TEXT("Update on render thread is currently %s"), (Flags.bUpdateOnRT) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("OVERDRIVE"))) // 2 frame raise overdrive
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (!CmdName.IsEmpty())
			{
				if (!FCString::Stricmp(*CmdName, TEXT("ON")))
				{
					Flags.bOverdrive = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Flags.bOverdrive = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE")))
				{
					Flags.bOverdrive = !Flags.bOverdrive;
				}
				else
				{
					return false;
				}
			}
			else
			{
				Flags.bOverdrive = !Flags.bOverdrive;
			}
			UpdateDistortionCaps();
			Ar.Logf(TEXT("Overdrive is currently %s"), (Flags.bOverdrive) ? TEXT("ON") : TEXT("OFF"));
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
					Flags.bTimeWarp = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Flags.bTimeWarp = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE")))
				{
					Flags.bTimeWarp = !Flags.bTimeWarp;
				}
				else
				{
					return false;
				}
			}
			else
			{
				Flags.bTimeWarp = !Flags.bTimeWarp;
			}
			UpdateDistortionCaps();
			Ar.Logf(TEXT("TimeWarp is currently %s"), (Flags.bTimeWarp) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
#endif // #ifdef OVR_SDK_RENDERING
#if !UE_BUILD_SHIPPING
		else if (FParse::Command(&Cmd, TEXT("UPDATEONGT"))) // update on game thread
		{
			FString CmdName = FParse::Token(Cmd, 0);
			if (!CmdName.IsEmpty())
			{
				if (!FCString::Stricmp(*CmdName, TEXT("ON")))
				{
					Flags.bDoNotUpdateOnGT = false;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("OFF")))
				{
					Flags.bDoNotUpdateOnGT = true;
				}
				else if (!FCString::Stricmp(*CmdName, TEXT("TOGGLE")))
				{
					Flags.bDoNotUpdateOnGT = !Flags.bDoNotUpdateOnGT;
				}
				else
				{
					return false;
				}
			}
			else
			{
				Flags.bDoNotUpdateOnGT = !Flags.bDoNotUpdateOnGT;
			}
			Ar.Logf(TEXT("Update on game thread is currently %s"), (!Flags.bDoNotUpdateOnGT) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("STATS"))) // status / statistics
		{
			Flags.bShowStats = !Flags.bShowStats;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("GRID"))) // grid
		{
			Flags.bDrawGrid = !Flags.bDrawGrid;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("PROFILE"))) // profile
		{
			Flags.bProfiling = !Flags.bProfiling;
			UpdateDistortionCaps();
			Ar.Logf(TEXT("Profiling mode is currently %s"), (Flags.bProfiling) ? TEXT("ON") : TEXT("OFF"));
			return true;
		}
#endif //UE_BUILD_SHIPPING
	}
	else if (FParse::Command(&Cmd, TEXT("HMDMAG")))
	{
		if (FParse::Command(&Cmd, TEXT("ON")))
		{
			Flags.bYawDriftCorrectionEnabled = true;
			UpdateHmdCaps();
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("OFF")))
		{
			Flags.bYawDriftCorrectionEnabled = false;
			UpdateHmdCaps();
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("SHOW")))
		{
			Ar.Logf(TEXT("mag %s"), Flags.bYawDriftCorrectionEnabled ? 
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
            Flags.bHmdDistortion = true;
            return true;
        }
        else if (FParse::Command( &Cmd, TEXT("OFF") ))
        {
            Flags.bHmdDistortion = false;
            return true;
        }
#endif //OVR_SDK_RENDERING
		if (FParse::Command(&Cmd, TEXT("CHA")))
		{
			Flags.bChromaAbCorrectionEnabled = true;
			UpdateDistortionCaps();
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("NOCHA")))
		{
			Flags.bChromaAbCorrectionEnabled = false;
			UpdateDistortionCaps();
			return true;
		}
		else if (FParse::Command( &Cmd, TEXT("HQ") ))
		{
			// High quality distortion
			if (FParse::Command( &Cmd, TEXT("ON") ))
			{
				Flags.bHQDistortion = true;
			}
			else if (FParse::Command(&Cmd, TEXT("OFF")))
			{
				Flags.bHQDistortion = false;
			}
			else
			{
				Flags.bHQDistortion = !Flags.bHQDistortion;
			}
			Ar.Logf(TEXT("High quality distortion is currently %s"), (Flags.bHQDistortion) ? TEXT("ON") : TEXT("OFF"));
			UpdateDistortionCaps();
			return true;
		}

		if (FParse::Command(&Cmd, TEXT("SHOW")))
		{
			Ar.Logf(TEXT("hmdwarp %s sc=%f %s"), (Flags.bHmdDistortion ? TEXT("on") : TEXT("off"))
				, IdealScreenPercentage / 100.f
				, (Flags.bChromaAbCorrectionEnabled ? TEXT("cha") : TEXT("nocha")));
		}
		return true;
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
#ifdef OVR_VISION_ENABLED
		else if (FParse::Command(&Cmd, TEXT("ON")) || FParse::Command(&Cmd, TEXT("ENABLE")))
		{
			Flags.bHmdPosTracking = true;
			UpdateHmdCaps();
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("OFF")) || FParse::Command(&Cmd, TEXT("DISABLE")))
		{
			Flags.bHmdPosTracking = false;
			Flags.bHaveVisionTracking = false;
			UpdateHmdCaps();
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("TOGGLE")))
		{
			Flags.bHmdPosTracking = !Flags.bHmdPosTracking;
			Flags.bHaveVisionTracking = false;
			UpdateHmdCaps();
			return true;
		}
#if !UE_BUILD_SHIPPING
		else if (FParse::Command(&Cmd, TEXT("SHOWCAMERA")))
		{
			if (FParse::Command(&Cmd, TEXT("OFF")))
			{
				Flags.bDrawTrackingCameraFrustum = false;
				return true;
			}
			if (FParse::Command(&Cmd, TEXT("ON")))
			{
				Flags.bDrawTrackingCameraFrustum = true;
				return true;
			}
			else 
			{
				Flags.bDrawTrackingCameraFrustum = !Flags.bDrawTrackingCameraFrustum;
				return true;
			}
		}
#endif // #if !UE_BUILD_SHIPPING
		else if (FParse::Command(&Cmd, TEXT("SHOW")))
		{
			Ar.Logf(TEXT("hmdpos is %s, vision='%s'"), 
				(Flags.bHmdPosTracking ? TEXT("enabled") : TEXT("disabled")),
				(Flags.bHaveVisionTracking ? TEXT("active") : TEXT("lost")));
			return true;
		}
#endif // #ifdef OVR_VISION_ENABLED
	}
	else if (FParse::Command(&Cmd, TEXT("OCULUSDEV")))
	{
		if (FParse::Command(&Cmd, TEXT("ON")))
		{
			Flags.bDevSettingsEnabled = true;
		}
		else if (FParse::Command(&Cmd, TEXT("OFF")))
		{
			Flags.bDevSettingsEnabled = false;
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
			Flags.bHeadTrackingEnforced = false;
			return true;
		}
		else if (!FCString::Stricmp(*CmdName, TEXT("ENFORCE")))
		{
			Flags.bHeadTrackingEnforced = !Flags.bHeadTrackingEnforced;
			if (!Flags.bHeadTrackingEnforced)
			{
				ResetControlRotation();
			}
			return true;
		}
		else if (!FCString::Stricmp(*CmdName, TEXT("RESET")))
		{
			Flags.bHeadTrackingEnforced = false;
			ResetControlRotation();
			return true;
		}
		return false;
	}
#ifndef OVR_SDK_RENDERING
	else if (FParse::Command(&Cmd, TEXT("SETFINISHFRAME")))
	{
		static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));

		if (FParse::Command(&Cmd, TEXT("ON")))
		{
			Flags.bAllowFinishCurrentFrame = true;
			if (Flags.bStereoEnabled)
			{
				CFinishFrameVar->Set(Flags.bAllowFinishCurrentFrame != 0);
			}
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("OFF")))
		{
			Flags.bAllowFinishCurrentFrame = false;
			if (Flags.bStereoEnabled)
			{
				CFinishFrameVar->Set(Flags.bAllowFinishCurrentFrame != 0);
			}
			return true;
		}
		return false;
	}
#endif
	else if (FParse::Command(&Cmd, TEXT("UNCAPFPS")))
	{
		GEngine->bSmoothFrameRate = false;
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("OVRVERSION")))
	{
		Ar.Logf(*GetVersionString());
		return true;
	}

	return false;
}

void FOculusRiftHMD::OnScreenModeChange(EWindowMode::Type WindowMode)
{
	EnableStereo(WindowMode != EWindowMode::Windowed);
	UpdateStereoRenderingParams();
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

		IHeadMountedDisplay::MonitorInfo MonitorInfo;
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

		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ChromaAbCorrectionEnabled"), Flags.bChromaAbCorrectionEnabled));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("MagEnabled"), Flags.bYawDriftCorrectionEnabled));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DevSettingsEnabled"), Flags.bDevSettingsEnabled));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("OverrideInterpupillaryDistance"), Flags.bOverrideIPD));
		if (Flags.bOverrideIPD)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("InterpupillaryDistance"), GetInterpupillaryDistance()));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("OverrideStereo"), Flags.bOverrideStereo));
		if (Flags.bOverrideStereo)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("HFOV"), HFOVInRadians));
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("VFOV"), VFOVInRadians));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("OverrideVSync"), Flags.bOverrideVSync));
		if (Flags.bOverrideVSync)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("VSync"), Flags.bVSync));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("OverrideScreenPercentage"), Flags.bOverrideScreenPercentage));
		if (Flags.bOverrideScreenPercentage)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ScreenPercentage"), ScreenPercentage));
		}
		if (Flags.bWorldToMetersOverride)
		{
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("WorldToMetersScale"), WorldToMetersScale));
		}
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("InterpupillaryDistance"), InterpupillaryDistance));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("TimeWarp"), Flags.bTimeWarp));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("AllowFinishCurrentFrame"), Flags.bAllowFinishCurrentFrame));
#ifdef OVR_VISION_ENABLED
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("HmdPosTracking"), Flags.bHmdPosTracking));
#endif
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("LowPersistenceMode"), Flags.bLowPersistenceMode));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("UpdateOnRT"), Flags.bUpdateOnRT));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Overdrive"), Flags.bOverdrive));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("MirrorToWindow"), Flags.bMirrorToWindow));

		FString OutStr(TEXT("Editor.VR.DeviceInitialised"));
		FEngineAnalytics::GetProvider().RecordEvent(OutStr, EventAttributes);
	}
}

bool FOculusRiftHMD::IsPositionalTrackingEnabled() const
{
#ifdef OVR_VISION_ENABLED
	return Flags.bHmdPosTracking;
#else
	return false;
#endif // #ifdef OVR_VISION_ENABLED
}

bool FOculusRiftHMD::EnablePositionalTracking(bool enable)
{
#ifdef OVR_VISION_ENABLED
	Flags.bHmdPosTracking = enable;
	return IsPositionalTrackingEnabled();
#else
	OVR_UNUSED(enable);
	return false;
#endif
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
bool FOculusRiftHMD::IsStereoEnabled() const
{
	return Flags.bStereoEnabled && Flags.bHMDEnabled /* && !GIsEditor*/;
}

bool FOculusRiftHMD::EnableStereo(bool bStereo)
{
	return DoEnableStereo(bStereo, true);
}

bool FOculusRiftHMD::DoEnableStereo(bool bStereo, bool bApplyToHmd)
{
	FSceneViewport* SceneVP = FindSceneViewport();
	if (bStereo && (!SceneVP || !SceneVP->IsStereoRenderingAllowed()))
	{
		return false;
	}

	bool stereoEnabled = (IsHMDEnabled()) ? bStereo : false;

	if ((Flags.bStereoEnabled && stereoEnabled) || (!Flags.bStereoEnabled && !stereoEnabled))
	{
		// already in the desired mode
		return Flags.bStereoEnabled;
	}

	bool wasFullscreenAllowed = IsFullscreenAllowed();
	if (OnOculusStateChange(stereoEnabled))
	{
		Flags.bStereoEnabled = stereoEnabled;

		if (SceneVP)
		{
			if (!IsFullscreenAllowed() && stereoEnabled)
			{
				if (Hmd)
				{
					// keep window size, but set viewport size to Rift resolution
					SceneVP->SetViewportSize(Hmd->Resolution.w, Hmd->Resolution.h);
				}
			}
			else if ((!wasFullscreenAllowed && !stereoEnabled))
			{
				// restoring original viewport size (to be equal to window size).
				TSharedPtr<SWindow> Window = SceneVP->FindWindow();
				if (Window.IsValid())
				{
					FVector2D size = Window->GetSizeInScreen();
					SceneVP->SetViewportSize(size.X, size.Y);
					Window->SetViewportSizeDrivenByWindow(true);
				}
			}

			if (SceneVP)
			{
				TSharedPtr<SWindow> Window = SceneVP->FindWindow();
				if (Window.IsValid())
				{
					FVector2D size = Window->GetSizeInScreen();

					if (bApplyToHmd && IsFullscreenAllowed())
					{
						SceneVP->SetViewportSize(size.X, size.Y);
						Window->SetViewportSizeDrivenByWindow(true);

						if (stereoEnabled)
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
						FSystemResolution::RequestResolutionChange(size.X, size.Y, (stereoEnabled) ? EWindowMode::WindowedMirror : EWindowMode::Windowed);
					}
				}
			}
		}
	}
	return Flags.bStereoEnabled;
}

void FOculusRiftHMD::ResetControlRotation() const
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

bool FOculusRiftHMD::OnOculusStateChange(bool bIsEnabledNow)
{
	Flags.bHmdDistortion = bIsEnabledNow;
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
			ApplySystemOverridesOnStereo(bIsEnabledNow);

			UpdateStereoRenderingParams();
			return true;
		}
	}
	return false;
}

void FOculusRiftHMD::ApplySystemOverridesOnStereo(bool bForce)
{
	if (Flags.bStereoEnabled || bForce)
	{
		// Set the current VSync state
		if (Flags.bOverrideVSync)
		{
			static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
			CVSyncVar->Set(Flags.bVSync != 0);
		}
		else
		{
			static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
			Flags.bVSync = CVSyncVar->GetInt() != 0;
		}
		UpdateHmdCaps();

#ifndef OVR_SDK_RENDERING
		static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
		CFinishFrameVar->Set(Flags.bAllowFinishCurrentFrame != 0);
#endif
	}
}

void FOculusRiftHMD::SaveSystemValues()
{
	static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	Flags.bSavedVSync = CVSyncVar->GetInt() != 0;

	static IConsoleVariable* CScrPercVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	SavedScrPerc = CScrPercVar->GetFloat();
}

void FOculusRiftHMD::RestoreSystemValues()
{
	static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	CVSyncVar->Set(Flags.bSavedVSync != 0);

	static IConsoleVariable* CScrPercVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	CScrPercVar->Set(SavedScrPerc);

	static IConsoleVariable* CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
	CFinishFrameVar->Set(false);
}

void FOculusRiftHMD::UpdateScreenSettings(const FViewport*)
{
	if (Flags.bScreenPercentageEnabled)
	{
		// Set the current ScreenPercentage state
		static IConsoleVariable* CScrPercVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
		float DesiredSceeenPercentage;
		if (Flags.bOverrideScreenPercentage)
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
}

void FOculusRiftHMD::AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
    SizeX = SizeX / 2;
    if( StereoPass == eSSP_RIGHT_EYE )
    {
        X += SizeX;
    }
}

void FOculusRiftHMD::CalculateStereoViewOffset(const EStereoscopicPass StereoPassType, const FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
	check(WorldToMeters != 0.f);
	check(Hmd);

	const int idx = (StereoPassType == eSSP_LEFT_EYE) ? 0 : 1;

	if (IsInGameThread())
	{
		// This method is called from GetProjectionData on a game thread.
		// The modified ViewLocation is used ONLY for ViewMatrix composition, it is not
		// stored modified in the ViewInfo. ViewInfo.ViewLocation remains unmodified.

		if( StereoPassType != eSSP_FULL || Flags.bHeadTrackingEnforced )
		{
			if (Flags.bNeedUpdateStereoRenderingParams)
				UpdateStereoRenderingParams();

			// Check if we already have updated the pose for this frame. If not, read it here
			if (GFrameNumber != LastFrameNumber)
			{
				// Usually, the pose is updated either from ApplyHmdOrientation or from UpdateCameraOrientation methods.
				// However, in Editor those methods are not called.
				FQuat	CurrentHmdOrientation;
				FVector CurrentHmdPosition;
				GetCurrentPose(CurrentHmdOrientation, CurrentHmdPosition);
			}

			FVector CurEyePosition;
			FQuat CurEyeOrient;
			PoseToOrientationAndPosition(EyeRenderPose[idx], CurEyeOrient, CurEyePosition);

			// The HMDPosition already has HMD orientation applied.
			// Apply rotational difference between HMD orientation and ViewRotation
			// to HMDPosition vector. 
			const FVector vEyePosition = DeltaControlOrientation.RotateVector(CurEyePosition) + PositionOffset;
			ViewLocation += vEyePosition;
		}
	}
	else if (IsInRenderingThread())
	{
		// This method is called from UpdateViewMatrix on a render thread.
		// The modified ViewLocation is used ONLY for ViewMatrix composition, it is not
		// stored modified in the ViewInfo. ViewInfo.ViewLocation remains unmodified.

		if (StereoPassType != eSSP_FULL)
		{
			FVector CurEyePosition;
			FQuat CurEyeOrient;
			PoseToOrientationAndPosition(RenderParams_RenderThread.EyeRenderPose[idx], CurEyeOrient, CurEyePosition);

			// The HMDPosition already has HMD orientation applied.
			// Apply rotational difference between HMD orientation and ViewRotation
			// to HMDPosition vector. 
			const FVector vEyePosition = RenderParams_RenderThread.DeltaControlOrientation.RotateVector(CurEyePosition) + PositionOffset;
			ViewLocation += vEyePosition;
		}
	}
	else
	{
		check(0); // huh? neither game nor rendering thread?
	}
}

void FOculusRiftHMD::ResetOrientationAndPosition(float yaw)
{
	ResetOrientation(yaw);
	ResetPosition();
}

void FOculusRiftHMD::ResetOrientation(float yaw)
{
	const ovrTrackingState ss = ovrHmd_GetTrackingState(Hmd, ovr_GetTimeInSeconds());
	const ovrPosef& pose = ss.HeadPose.ThePose;
	const OVR::Quatf orientation = OVR::Quatf(pose.Orientation);

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

void FOculusRiftHMD::ResetPosition()
{
	// Reset position
#ifdef OVR_VISION_ENABLED
	const ovrTrackingState ss = ovrHmd_GetTrackingState(Hmd, ovr_GetTimeInSeconds());
	const ovrPosef& pose = ss.HeadPose.ThePose;
	const OVR::Quatf orientation = OVR::Quatf(pose.Orientation);

	BaseOffset = pose.Position;
#else
	BaseOffset = OVR::Vector3f(0, 0, 0);
#endif // #ifdef OVR_VISION_ENABLED
}

void FOculusRiftHMD::SetClippingPlanes(float NCP, float FCP)
{
	NearClippingPlane = NCP;
	FarClippingPlane = FCP;
	Flags.bClippingPlanesOverride = false; // prevents from saving in .ini file
}

void FOculusRiftHMD::SetBaseRotation(const FRotator& BaseRot)
{
	SetBaseOrientation(BaseRot.Quaternion());
}

FRotator FOculusRiftHMD::GetBaseRotation() const
{
	return GetBaseOrientation().Rotator();
}

void FOculusRiftHMD::SetBaseOrientation(const FQuat& BaseOrient)
{
	BaseOrientation = BaseOrient;
}

FQuat FOculusRiftHMD::GetBaseOrientation() const
{
	return BaseOrientation;
}

void FOculusRiftHMD::SetPositionOffset(const FVector& PosOff)
{
	PositionOffset = PosOff;
}

FVector FOculusRiftHMD::GetPositionOffset() const
{
	return PositionOffset;
}

FMatrix FOculusRiftHMD::GetStereoProjectionMatrix(enum EStereoscopicPass StereoPassType, const float FOV) const
{
	// Stereo params must be recalculated already, see CalculateStereoViewOffset.
	check(!Flags.bNeedUpdateStereoRenderingParams);
	check(IsStereoEnabled());

	const int idx = (StereoPassType == eSSP_LEFT_EYE) ? 0 : 1;

	FMatrix proj = ToFMatrix(EyeProjectionMatrices[idx]);

	// correct far and near planes for reversed-Z projection matrix
	const float InNearZ = (NearClippingPlane) ? NearClippingPlane : GNearClippingPlane;
	const float InFarZ = (FarClippingPlane) ? FarClippingPlane : GNearClippingPlane;
	proj.M[3][3] = 0.0f;
	proj.M[2][3] = 1.0f;

	proj.M[2][2] = (InNearZ == InFarZ) ? 0.0f    : InNearZ / (InNearZ - InFarZ);
	proj.M[3][2] = (InNearZ == InFarZ) ? InNearZ : -InFarZ * InNearZ / (InNearZ - InFarZ);

	return proj;
}

void FOculusRiftHMD::InitCanvasFromView(FSceneView* InView, UCanvas* Canvas)
{
	// This is used for placing small HUDs (with names)
	// over other players (for example, in Capture Flag).
	// HmdOrientation should be initialized by GetCurrentOrientation (or
	// user's own value).
	FSceneView HmdView(*InView);

	const FQuat DeltaOrient = HmdView.BaseHmdOrientation.Inverse() * Canvas->HmdOrientation;
	HmdView.ViewRotation = FRotator(HmdView.ViewRotation.Quaternion() * DeltaOrient);

	HmdView.UpdateViewMatrix();
	Canvas->ViewProjectionMatrix = HmdView.ViewProjectionMatrix;
}

void FOculusRiftHMD::PushViewportCanvas(EStereoscopicPass StereoPass, FCanvas *InCanvas, UCanvas *InCanvasObject, FViewport *InViewport) const
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

void FOculusRiftHMD::PushViewCanvas(EStereoscopicPass StereoPass, FCanvas *InCanvas, UCanvas *InCanvasObject, FSceneView *InView) const
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
// Oculus Rift ISceneViewExtension Implementation
//---------------------------------------------------

void FOculusRiftHMD::ModifyShowFlags(FEngineShowFlags& ShowFlags)
{
	ShowFlags.MotionBlur = 0;
#ifndef OVR_SDK_RENDERING
	ShowFlags.HMDDistortion = Flags.bHmdDistortion;
#else
	ShowFlags.HMDDistortion = false;
#endif
	ShowFlags.StereoRendering = IsStereoEnabled();
}

void FOculusRiftHMD::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	InView.BaseHmdOrientation = LastHmdOrientation;
	InView.BaseHmdLocation = FVector(0.f);
	if (!Flags.bWorldToMetersOverride)
	{
		WorldToMetersScale = InView.WorldToMetersScale;
	}
	Flags.bScreenPercentageEnabled = InViewFamily.EngineShowFlags.ScreenPercentage;

#ifndef OVR_SDK_RENDERING
	InViewFamily.bUseSeparateRenderTarget = false;

	// check and save texture size. 
	if (InView.StereoPass == eSSP_LEFT_EYE)
	{
		if (EyeViewportSize != InView.ViewRect.Size())
		{
			EyeViewportSize = InView.ViewRect.Size();
			Flags.bNeedUpdateStereoRenderingParams = true;
		}
	}
#else
	InViewFamily.bUseSeparateRenderTarget = ShouldUseSeparateRenderTarget();
#endif
}

bool FOculusRiftHMD::IsHeadTrackingAllowed() const
{
#if WITH_EDITOR
	UEditorEngine* EdEngine = Cast<UEditorEngine>(GEngine);
#endif//WITH_EDITOR
	return Hmd && 
#if WITH_EDITOR
		(!EdEngine || EdEngine->bUseVRPreviewForPlayWorld || GetDefault<ULevelEditorPlaySettings>()->ViewportGetsHMDControl) &&
#endif//WITH_EDITOR
		(Flags.bHeadTrackingEnforced || GEngine->IsStereoscopic3D());
}

//---------------------------------------------------
// Oculus Rift Specific
//---------------------------------------------------

FOculusRiftHMD::FOculusRiftHMD()
	:
	SavedScrPerc(100.f)
	, ScreenPercentage(100.f)
	, InterpupillaryDistance(OVR_DEFAULT_IPD)
	, WorldToMetersScale(100.f)
	, UserDistanceToScreenModifier(0.f)
	, HFOVInRadians(FMath::DegreesToRadians(90.f))
	, VFOVInRadians(FMath::DegreesToRadians(90.f))
	, HudOffset(0.f)
	, CanvasCenterOffset(0.f)
	, MirrorWindowSize(0, 0)
	, NearClippingPlane(0)
	, FarClippingPlane(0)
	, DeltaControlRotation(FRotator::ZeroRotator)
	, DeltaControlOrientation(FQuat::Identity)
	, LastHmdOrientation(FQuat::Identity)
	, LastFrameNumber(0)
	, BaseOffset(0, 0, 0)
	, BaseOrientation(FQuat::Identity)
	, PositionOffset(0,0,0)
	, Hmd(nullptr)
	, TrackingCaps(0)
	, DistortionCaps(0)
	, HmdCaps(0)
	, EyeViewportSize(0, 0)
	, RenderParams_RenderThread(getThis())
{
	Flags.Raw = 0;

	Flags.bHMDEnabled = true;
	Flags.bNeedUpdateStereoRenderingParams = true;
	Flags.bOverrideVSync = true;
	Flags.bVSync = true;
	Flags.bAllowFinishCurrentFrame = true;
	Flags.bHmdDistortion = true;
	Flags.bChromaAbCorrectionEnabled = true;
	Flags.bYawDriftCorrectionEnabled = true;
	Flags.bLowPersistenceMode = true; // on by default (DK2+ only)
	Flags.bUpdateOnRT = true;
	Flags.bOverdrive = true;
	Flags.bMirrorToWindow = true;
	Flags.bTimeWarp = true;
#ifdef OVR_VISION_ENABLED
	Flags.bHmdPosTracking = true;
#endif
#ifndef OVR_SDK_RENDERING
	Flags.bTimeWarp = false;
#else
	AlternateFrameRateDivider = 1;
#endif
	if (GIsEditor)
	{
		Flags.bOverrideScreenPercentage = true;
		ScreenPercentage = 100;
	}
	SupportedTrackingCaps = SupportedDistortionCaps = SupportedHmdCaps = 0;
	OSWindowHandle = nullptr;
	Startup();
}

FOculusRiftHMD::~FOculusRiftHMD()
{
	Shutdown();
}

bool FOculusRiftHMD::IsInitialized() const
{
	return (Flags.InitStatus & eInitialized) != 0;
}

void FOculusRiftHMD::Startup()
{
	if ((!IsRunningGame() && !GIsEditor) || (Flags.InitStatus & eStartupExecuted) != 0)
	{
		// do not initialize plugin for server or if it was already initialized
		return;
	}
	Flags.InitStatus |= eStartupExecuted;

	// Initializes LibOVR. This LogMask_All enables maximum logging.
	// Custom allocator can also be specified here.
	// Actually, most likely, the ovr_Initialize is already called from PreInit.
	int8 bWasInitialized = ovr_Initialize();

#if !UE_BUILD_SHIPPING
	// Should be changed to CAPI when available.
	static OculusLog OcLog;
	OVR::Log::SetGlobalLog(&OcLog);
#endif //#if !UE_BUILD_SHIPPING

	if (GIsEditor)
	{
		Flags.bHeadTrackingEnforced = true;
		//AlternateFrameRateDivider = 2;
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
		Flags.InitStatus = 0;
		return;
	}

	// Uncap fps to enable FPS higher than 62
	GEngine->bSmoothFrameRate = false;

	SaveSystemValues();

#ifdef OVR_SDK_RENDERING
#if defined(OVR_D3D_VERSION) && (OVR_D3D_VERSION == 11)
	if (IsPCPlatform(GMaxRHIShaderPlatform) && !IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		pD3D11Bridge = new D3D11Bridge(this);
	}
#endif
#if defined(OVR_GL)
	if (IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		pOGLBridge = new OGLBridge(this);
	}
#endif
#endif // #ifdef OVR_SDK_RENDERING

	if (bForced || Hmd)
	{
		Flags.InitStatus |= eInitialized;

		UE_LOG(LogHMD, Log, TEXT("Oculus plugin initialized. Version: %s"), *GetVersionString());
	}
}

void FOculusRiftHMD::Shutdown()
{
	if (!(Flags.InitStatus & eInitialized))
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
	
#ifndef OVR_SDK_RENDERING
	for (unsigned i = 0; i < sizeof(pDistortionMesh) / sizeof(pDistortionMesh[0]); ++i)
	{
		pDistortionMesh[i] = NULL;
	}
#endif
	{
		OVR::Lock::Locker lock(&StereoParamsLock);
		RenderParams_RenderThread.Clear();
	}
	ovr_Shutdown();
	Flags.InitStatus = 0;
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

	Hmd = ovrHmd_Create(0);
	if (Hmd)
	{
		SupportedDistortionCaps = Hmd->DistortionCaps;
		SupportedHmdCaps = Hmd->HmdCaps;
		SupportedTrackingCaps = Hmd->TrackingCaps;

#ifndef OVR_SDK_RENDERING
		SupportedDistortionCaps &= ~ovrDistortionCap_Overdrive;
#endif
#ifndef OVR_VISION_ENABLED
		SupportedTrackingCaps &= ~ovrTrackingCap_Position;
#endif

		DistortionCaps = SupportedDistortionCaps & (ovrDistortionCap_Chromatic | ovrDistortionCap_TimeWarp | ovrDistortionCap_Vignette | ovrDistortionCap_Overdrive);
		TrackingCaps = SupportedTrackingCaps & (ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position);
		HmdCaps = SupportedHmdCaps & (ovrHmdCap_DynamicPrediction | ovrHmdCap_LowPersistence);
		HmdCaps |= (Flags.bVSync ? 0 : ovrHmdCap_NoVSync);

		if (!(SupportedDistortionCaps & ovrDistortionCap_TimeWarp))
		{
			Flags.bTimeWarp = false;
		}

		Flags.bHmdPosTracking = (SupportedTrackingCaps & ovrTrackingCap_Position) != 0;

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
		// Wait for all resources to be released
		FlushRenderingCommands();
#endif 

		ovrHmd_Destroy(Hmd);
		Hmd = nullptr;
	}
}

void FOculusRiftHMD::UpdateDistortionCaps()
{
	if (IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		DistortionCaps &= ~ovrDistortionCap_SRGB;
		DistortionCaps |= ovrDistortionCap_FlipInput;
	}
	(Flags.bTimeWarp) ? DistortionCaps |= ovrDistortionCap_TimeWarp : DistortionCaps &= ~ovrDistortionCap_TimeWarp;
	(Flags.bOverdrive) ? DistortionCaps |= ovrDistortionCap_Overdrive : DistortionCaps &= ~ovrDistortionCap_Overdrive;
	(Flags.bHQDistortion) ? DistortionCaps |= ovrDistortionCap_HqDistortion : DistortionCaps &= ~ovrDistortionCap_HqDistortion;
	(Flags.bChromaAbCorrectionEnabled) ? DistortionCaps |= ovrDistortionCap_Chromatic : DistortionCaps &= ~ovrDistortionCap_Chromatic;
#if !UE_BUILD_SHIPPING
	(Flags.bProfiling) ? DistortionCaps |= ovrDistortionCap_ProfileNoTimewarpSpinWaits : DistortionCaps &= ~ovrDistortionCap_ProfileNoTimewarpSpinWaits;
#endif // #if !UE_BUILD_SHIPPING

#ifdef OVR_SDK_RENDERING 
	if (GetActiveRHIBridgeImpl())
	{
		GetActiveRHIBridgeImpl()->SetNeedReinitRendererAPI();
	}
#endif // OVR_SDK_RENDERING
}

void FOculusRiftHMD::UpdateHmdCaps()
{
	if (Hmd)
	{
		TrackingCaps = ovrTrackingCap_Orientation;
		if (Flags.bYawDriftCorrectionEnabled)
		{
			TrackingCaps |= ovrTrackingCap_MagYawCorrection;
		}
		else
		{
			TrackingCaps &= ~ovrTrackingCap_MagYawCorrection;
		}
		if (Flags.bHmdPosTracking)
		{
			TrackingCaps |= ovrTrackingCap_Position;
		}
		else
		{
			TrackingCaps &= ~ovrTrackingCap_Position;
		}

		if (Flags.bLowPersistenceMode)
		{
			HmdCaps |= ovrHmdCap_LowPersistence;
		}
		else
		{
			HmdCaps &= ~ovrHmdCap_LowPersistence;
		}

		if (Flags.bVSync)
		{
			HmdCaps &= ~ovrHmdCap_NoVSync;
		}
		else
		{
			HmdCaps |= ovrHmdCap_NoVSync;
		}

		if (Flags.bMirrorToWindow)
		{
			HmdCaps &= ~ovrHmdCap_NoMirrorToWindow;
		}
		else
		{
			HmdCaps |= ovrHmdCap_NoMirrorToWindow;
		}
		ovrHmd_SetEnabledCaps(Hmd, HmdCaps);

		ovrHmd_ConfigureTracking(Hmd, TrackingCaps, 0);
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

	// Calc FOV
	if (!Flags.bOverrideFOV)
	{
		// Calc FOV, symmetrical, for each eye. 
		EyeFov[0] = Hmd->DefaultEyeFov[0];
		EyeFov[1] = Hmd->DefaultEyeFov[1];

		// Calc FOV in radians
		VFOVInRadians = FMath::Max(GetVerticalFovRadians(EyeFov[0]), GetVerticalFovRadians(EyeFov[1]));
		HFOVInRadians = FMath::Max(GetHorizontalFovRadians(EyeFov[0]), GetHorizontalFovRadians(EyeFov[1]));
	}

	const Sizei recommenedTex0Size = ovrHmd_GetFovTextureSize(Hmd, ovrEye_Left, EyeFov[0], 1.0f);
	const Sizei recommenedTex1Size = ovrHmd_GetFovTextureSize(Hmd, ovrEye_Right, EyeFov[1], 1.0f);

	Sizei idealRenderTargetSize;
	idealRenderTargetSize.w = recommenedTex0Size.w + recommenedTex1Size.w;
	idealRenderTargetSize.h = FMath::Max(recommenedTex0Size.h, recommenedTex1Size.h);

	IdealScreenPercentage = FMath::Max(float(idealRenderTargetSize.w) / float(Hmd->Resolution.w) * 100.f,
									   float(idealRenderTargetSize.h) / float(Hmd->Resolution.h) * 100.f);

	// Override eye distance by the value from HMDInfo (stored in Profile).
	if (!Flags.bOverrideIPD)
	{
		InterpupillaryDistance = ovrHmd_GetFloat(Hmd, OVR_KEY_IPD, OVR_DEFAULT_IPD);
	}

	// Default texture size (per eye) is equal to half of W x H resolution. Will be overridden in SetupView.
	EyeViewportSize = FIntPoint(Hmd->Resolution.w / 2, Hmd->Resolution.h);

	Flags.bNeedUpdateStereoRenderingParams = true;
}

void FOculusRiftHMD::UpdateStereoRenderingParams()
{
	// If we've manually overridden stereo rendering params for debugging, don't mess with them
	if (Flags.bOverrideStereo || (!IsStereoEnabled() && !Flags.bHeadTrackingEnforced))
	{
		return;
	}
	if (IsInitialized() && Hmd)
	{
		Lock::Locker lock(&StereoParamsLock);

		TextureSize = Sizei(EyeViewportSize.X * 2, EyeViewportSize.Y);

		EyeRenderViewport[0].Pos = Vector2i(0, 0);
		EyeRenderViewport[0].Size = Sizei(EyeViewportSize.X, EyeViewportSize.Y);
		EyeRenderViewport[1].Pos = Vector2i(EyeViewportSize.X, 0);
		EyeRenderViewport[1].Size = EyeRenderViewport[0].Size;

		//!AB: note, for Direct Rendering EyeRenderDesc is calculated twice, once
		// here and another time in BeginRendering_RenderThread. I need to have EyeRenderDesc
		// on a game thread for ViewAdjust (for StereoViewOffset calculation).
		EyeRenderDesc[0] = ovrHmd_GetRenderDesc(Hmd, ovrEye_Left, EyeFov[0]);
		EyeRenderDesc[1] = ovrHmd_GetRenderDesc(Hmd, ovrEye_Right, EyeFov[1]);
		if (Flags.bOverrideIPD)
		{
			EyeRenderDesc[0].HmdToEyeViewOffset.x = InterpupillaryDistance * 0.5f;
			EyeRenderDesc[1].HmdToEyeViewOffset.x = -InterpupillaryDistance * 0.5f;
		}

		const bool bRightHanded = false;
		// Far and Near clipping planes will be modified in GetStereoProjectionMatrix()
		EyeProjectionMatrices[0] = ovrMatrix4f_Projection(EyeFov[0], 0.01f, 10000.0f, bRightHanded);
		EyeProjectionMatrices[1] = ovrMatrix4f_Projection(EyeFov[1], 0.01f, 10000.0f, bRightHanded);

		// 2D elements offset
		if (!Flags.bOverride2D)
		{
			float ScreenSizeInMeters[2]; // 0 - width, 1 - height
			float LensSeparationInMeters;
			LensSeparationInMeters = ovrHmd_GetFloat(Hmd, "LensSeparation", 0);
			ovrHmd_GetFloatArray(Hmd, "ScreenSize", ScreenSizeInMeters, 2);

			// Recenter projection (meters)
			const float LeftProjCenterM = ScreenSizeInMeters[0] * 0.25f;
			const float LensRecenterM = LeftProjCenterM - LensSeparationInMeters * 0.5f;

			// Recenter projection (normalized)
			const float LensRecenter = 4.0f * LensRecenterM / ScreenSizeInMeters[0];

			HudOffset = 0.25f * InterpupillaryDistance * (Hmd->Resolution.w / ScreenSizeInMeters[0]) / 15.0f;
			CanvasCenterOffset = (0.25f * LensRecenter) * Hmd->Resolution.w;
		}

		PrecalculatePostProcess_NoLock();
#ifdef OVR_SDK_RENDERING 
		GetActiveRHIBridgeImpl()->SetNeedReinitRendererAPI();
#endif // OVR_SDK_RENDERING
		Flags.bNeedUpdateStereoRenderingParams = false;
	}
	else
	{
		CanvasCenterOffset = 0.f;
	}
}

void FOculusRiftHMD::LoadFromIni()
{
	const TCHAR* OculusSettings = TEXT("Oculus.Settings");
	bool v;
	float f;
	if (GConfig->GetBool(OculusSettings, TEXT("bChromaAbCorrectionEnabled"), v, GEngineIni))
	{
		Flags.bChromaAbCorrectionEnabled = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bYawDriftCorrectionEnabled"), v, GEngineIni))
	{
		Flags.bYawDriftCorrectionEnabled = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bDevSettingsEnabled"), v, GEngineIni))
	{
		Flags.bDevSettingsEnabled = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bOverrideIPD"), v, GEngineIni))
	{
		Flags.bOverrideIPD = v;
		if (Flags.bOverrideIPD)
		{
			if (GConfig->GetFloat(OculusSettings, TEXT("IPD"), f, GEngineIni))
			{
				SetInterpupillaryDistance(f);
			}
		}
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bOverrideStereo"), v, GEngineIni))
	{
		Flags.bOverrideStereo = v;
		if (Flags.bOverrideStereo)
		{
			if (GConfig->GetFloat(OculusSettings, TEXT("HFOV"), f, GEngineIni))
			{
				HFOVInRadians = f;
			}
			if (GConfig->GetFloat(OculusSettings, TEXT("VFOV"), f, GEngineIni))
			{
				VFOVInRadians = f;
			}
		}
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bOverrideVSync"), v, GEngineIni))
	{
		Flags.bOverrideVSync = v;
		if (GConfig->GetBool(OculusSettings, TEXT("bVSync"), v, GEngineIni))
		{
			Flags.bVSync = v;
		}
	}
	if (!GIsEditor)
	{
		if (GConfig->GetBool(OculusSettings, TEXT("bOverrideScreenPercentage"), v, GEngineIni))
		{
			Flags.bOverrideScreenPercentage = v;
			if (GConfig->GetFloat(OculusSettings, TEXT("ScreenPercentage"), f, GEngineIni))
			{
				ScreenPercentage = f;
			}
		}
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bAllowFinishCurrentFrame"), v, GEngineIni))
	{
		Flags.bAllowFinishCurrentFrame = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bLowPersistenceMode"), v, GEngineIni))
	{
		Flags.bLowPersistenceMode = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bUpdateOnRT"), v, GEngineIni))
	{
		Flags.bUpdateOnRT = v;
	}
	if (GConfig->GetFloat(OculusSettings, TEXT("FarClippingPlane"), f, GEngineIni))
	{
		FarClippingPlane = f;
	}
	if (GConfig->GetFloat(OculusSettings, TEXT("NearClippingPlane"), f, GEngineIni))
	{
		NearClippingPlane = f;
	}
}

void FOculusRiftHMD::SaveToIni()
{
	const TCHAR* OculusSettings = TEXT("Oculus.Settings");
	GConfig->SetBool(OculusSettings, TEXT("bChromaAbCorrectionEnabled"), Flags.bChromaAbCorrectionEnabled, GEngineIni);
	GConfig->SetBool(OculusSettings, TEXT("bYawDriftCorrectionEnabled"), Flags.bYawDriftCorrectionEnabled, GEngineIni);
	GConfig->SetBool(OculusSettings, TEXT("bDevSettingsEnabled"), Flags.bDevSettingsEnabled, GEngineIni);

	GConfig->SetBool(OculusSettings, TEXT("bOverrideIPD"), Flags.bOverrideIPD, GEngineIni);
	if (Flags.bOverrideIPD)
	{
		GConfig->SetFloat(OculusSettings, TEXT("IPD"), GetInterpupillaryDistance(), GEngineIni);
	}
	GConfig->SetBool(OculusSettings, TEXT("bOverrideStereo"), Flags.bOverrideStereo, GEngineIni);
	if (Flags.bOverrideStereo)
	{
		GConfig->SetFloat(OculusSettings, TEXT("HFOV"), HFOVInRadians, GEngineIni);
		GConfig->SetFloat(OculusSettings, TEXT("VFOV"), VFOVInRadians, GEngineIni);
	}

	GConfig->SetBool(OculusSettings, TEXT("bOverrideVSync"), Flags.bOverrideVSync, GEngineIni);
	if (Flags.bOverrideVSync)
	{
		GConfig->SetBool(OculusSettings, TEXT("VSync"), Flags.bVSync, GEngineIni);
	}

	if (!GIsEditor)
	{
		GConfig->SetBool(OculusSettings, TEXT("bOverrideScreenPercentage"), Flags.bOverrideScreenPercentage, GEngineIni);
		if (Flags.bOverrideScreenPercentage)
		{
			// Save the current ScreenPercentage state
			GConfig->SetFloat(OculusSettings, TEXT("ScreenPercentage"), ScreenPercentage, GEngineIni);
		}
	}
	GConfig->SetBool(OculusSettings, TEXT("bAllowFinishCurrentFrame"), Flags.bAllowFinishCurrentFrame, GEngineIni);

	GConfig->SetBool(OculusSettings, TEXT("bLowPersistenceMode"), Flags.bLowPersistenceMode, GEngineIni);

	GConfig->SetBool(OculusSettings, TEXT("bUpdateOnRT"), Flags.bUpdateOnRT, GEngineIni);

	if (Flags.bClippingPlanesOverride)
	{
		GConfig->SetFloat(OculusSettings, TEXT("FarClippingPlane"), FarClippingPlane, GEngineIni);
		GConfig->SetFloat(OculusSettings, TEXT("NearClippingPlane"), NearClippingPlane, GEngineIni);
	}
}

bool FOculusRiftHMD::HandleInputKey(UPlayerInput* pPlayerInput,
	const FKey& Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
	if (Hmd && EventType == IE_Pressed && IsStereoEnabled())
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
		PositionOffset = FVector::ZeroVector;
		DeltaControlRotation = FRotator::ZeroRotator;
		BaseOrientation = LastHmdOrientation = DeltaControlOrientation = FQuat::Identity;
		BaseOffset = OVR::Vector3f(0, 0, 0);
		WorldToMetersScale = 100.f;
		Flags.bWorldToMetersOverride = false;
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

#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS

