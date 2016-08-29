// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Kismet/HeadMountedDisplayFunctionLibrary.h"
#include "HeadMountedDisplay.h"

DEFINE_LOG_CATEGORY_STATIC(LogUHeadMountedDisplay, Log, All);

UHeadMountedDisplayFunctionLibrary::UHeadMountedDisplayFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled()
{
	return GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed();
}

bool UHeadMountedDisplayFunctionLibrary::EnableHMD(bool bEnable)
{
	if (GEngine->HMDDevice.IsValid())
	{
		GEngine->HMDDevice->EnableHMD(bEnable);
		if (bEnable)
		{
			return GEngine->HMDDevice->EnableStereo(true);
		}
		else
		{
			GEngine->HMDDevice->EnableStereo(false);
			return true;
		}
	}
	return false;
}

FName UHeadMountedDisplayFunctionLibrary::GetHMDDeviceName()
{
	FName DeviceName(NAME_None);

	if (GEngine->HMDDevice.IsValid())
	{
		DeviceName = GEngine->HMDDevice->GetDeviceName();
	}

	return DeviceName;
}

void UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(FRotator& DeviceRotation, FVector& DevicePosition)
{
	if(GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		FQuat OrientationAsQuat;
		FVector Position(0.f);

		GEngine->HMDDevice->GetCurrentOrientationAndPosition(OrientationAsQuat, Position);

		DeviceRotation = OrientationAsQuat.Rotator();
		DevicePosition = Position;
	}
	else
	{
		DeviceRotation = FRotator::ZeroRotator;
		DevicePosition = FVector::ZeroVector;
	}
}

bool UHeadMountedDisplayFunctionLibrary::HasValidTrackingPosition()
{
	if(GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		return GEngine->HMDDevice->HasValidTrackingPosition();
	}

	return false;
}

int32 UHeadMountedDisplayFunctionLibrary::GetNumOfTrackingSensors()
{
	if (GEngine->HMDDevice.IsValid())
	{
		return GEngine->HMDDevice->GetNumOfTrackingSensors();
	}
	return 0;
}

void UHeadMountedDisplayFunctionLibrary::GetPositionalTrackingCameraParameters(FVector& CameraOrigin, FRotator& CameraRotation, float& HFOV, float& VFOV, float& CameraDistance, float& NearPlane, float& FarPlane)
{
	bool isActive;
	GetTrackingSensorParameters(CameraOrigin, CameraRotation, HFOV, VFOV, CameraDistance, NearPlane, FarPlane, isActive, 0);
}

void UHeadMountedDisplayFunctionLibrary::GetTrackingSensorParameters(FVector& Origin, FRotator& Rotation, float& HFOV, float& VFOV, float& Distance, float& NearPlane, float& FarPlane, bool& IsActive, int32 Index)
{
	IsActive = false;
	if (Index >= 0 && GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed() && GEngine->HMDDevice->DoesSupportPositionalTracking())
	{
		FQuat Orientation;
		IsActive = GEngine->HMDDevice->GetTrackingSensorProperties((uint8)Index, Origin, Orientation, HFOV, VFOV, Distance, NearPlane, FarPlane);
		Rotation = Orientation.Rotator();
	}
	else
	{
		// No HMD, zero the values
		Origin = FVector::ZeroVector;
		Rotation = FRotator::ZeroRotator;
		HFOV = VFOV = 0.f;
		NearPlane = 0.f;
		FarPlane = 0.f;
		Distance = 0.f;
	}
}

bool UHeadMountedDisplayFunctionLibrary::IsInLowPersistenceMode()
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		return GEngine->HMDDevice->IsInLowPersistenceMode();
	}
	else
	{
		return false;
	}
}

void UHeadMountedDisplayFunctionLibrary::EnableLowPersistenceMode(bool bEnable)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		GEngine->HMDDevice->EnableLowPersistenceMode(bEnable);
	}
}

void UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(float Yaw, EOrientPositionSelector::Type Options)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		switch (Options)
		{
		case EOrientPositionSelector::Orientation:
			GEngine->HMDDevice->ResetOrientation(Yaw);
			break;
		case EOrientPositionSelector::Position:
			GEngine->HMDDevice->ResetPosition();
			break;
		default:
			GEngine->HMDDevice->ResetOrientationAndPosition(Yaw);
		}
	}
}

void UHeadMountedDisplayFunctionLibrary::SetClippingPlanes(float Near, float Far)
{
	if (GEngine->HMDDevice.IsValid())
	{
		GEngine->HMDDevice->SetClippingPlanes(Near, Far);
	}
}

/** 
 * Sets screen percentage to be used in VR mode.
 *
 * @param ScreenPercentage	(in) Specifies the screen percentage to be used in VR mode. Use 0.0f value to reset to default value.
 */
void UHeadMountedDisplayFunctionLibrary::SetScreenPercentage(float ScreenPercentage)
{
	if (GEngine->StereoRenderingDevice.IsValid())
	{
		GEngine->StereoRenderingDevice->SetScreenPercentage(ScreenPercentage);
	}
}

/** 
 * Returns screen percentage to be used in VR mode.
 *
 * @return (float)	The screen percentage to be used in VR mode.
 */
float UHeadMountedDisplayFunctionLibrary::GetScreenPercentage()
{
	if (GEngine->StereoRenderingDevice.IsValid())
	{
		return GEngine->StereoRenderingDevice->GetScreenPercentage();
	}
	return 0.0f;
}

void UHeadMountedDisplayFunctionLibrary::SetWorldToMetersScale(UObject* WorldContext, float NewScale)
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetWorldSettings()->WorldToMeters = NewScale;
	}
}

float UHeadMountedDisplayFunctionLibrary::GetWorldToMetersScale(UObject* WorldContext)
{
	return WorldContext ? WorldContext->GetWorld()->GetWorldSettings()->WorldToMeters : 0.f;
}

void UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(TEnumAsByte<EHMDTrackingOrigin::Type> InOrigin)
{
	if (GEngine->HMDDevice.IsValid())
	{
		EHMDTrackingOrigin::Type Origin = EHMDTrackingOrigin::Eye;
		switch (InOrigin)
		{
		case EHMDTrackingOrigin::Eye:
			Origin = EHMDTrackingOrigin::Eye;
			break;
		case EHMDTrackingOrigin::Floor:
			Origin = EHMDTrackingOrigin::Floor;
			break;
		default:
			break;
		}
		GEngine->HMDDevice->SetTrackingOrigin(Origin);
	}
}

TEnumAsByte<EHMDTrackingOrigin::Type> UHeadMountedDisplayFunctionLibrary::GetTrackingOrigin()
{
	EHMDTrackingOrigin::Type Origin = EHMDTrackingOrigin::Eye;

	if (GEngine->HMDDevice.IsValid())
	{
		Origin = GEngine->HMDDevice->GetTrackingOrigin();
	}

	return Origin;
}

void UHeadMountedDisplayFunctionLibrary::GetVRFocusState(bool& bUseFocus, bool& bHasFocus)
{
	if (GEngine->HMDDevice.IsValid())
	{
		bUseFocus = GEngine->HMDDevice->DoesAppUseVRFocus();
		bHasFocus = GEngine->HMDDevice->DoesAppHaveVRFocus();
	}
	else
	{
		bUseFocus = bHasFocus = false;
	}
}