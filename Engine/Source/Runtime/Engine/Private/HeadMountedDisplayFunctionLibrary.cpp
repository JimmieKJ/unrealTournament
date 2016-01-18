// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

void UHeadMountedDisplayFunctionLibrary::GetPositionalTrackingCameraParameters(FVector& CameraOrigin, FRotator& CameraRotation, float& HFOV, float& VFOV, float& CameraDistance, float& NearPlane, float&FarPlane)
{
	if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed() && GEngine->HMDDevice->DoesSupportPositionalTracking())
	{
		FQuat CameraOrientation;
		GEngine->HMDDevice->GetPositionalTrackingCameraProperties(CameraOrigin, CameraOrientation, HFOV, VFOV, CameraDistance, NearPlane, FarPlane);
		CameraRotation = CameraOrientation.Rotator();
	}
	else
	{
		// No HMD, zero the values
		CameraOrigin = FVector::ZeroVector;
		CameraRotation = FRotator::ZeroRotator;
		HFOV = VFOV = 0.f;
		NearPlane = 0.f;
		FarPlane = 0.f;
		CameraDistance = 0.f;
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