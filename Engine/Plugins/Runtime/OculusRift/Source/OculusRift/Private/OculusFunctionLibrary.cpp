// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HMDPrivatePCH.h"
#include "OculusFunctionLibrary.h"
#include "OculusRiftHMD.h"

UOculusFunctionLibrary::UOculusFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FOculusRiftHMD* UOculusFunctionLibrary::GetOculusHMD()
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	if (GEngine->HMDDevice.IsValid() && (GEngine->HMDDevice->GetHMDDeviceType() == EHMDDeviceType::DT_OculusRift))
	{
		return static_cast<FOculusRiftHMD*>(GEngine->HMDDevice.Get());
	}

	UE_LOG(LogHMD, Warning, TEXT("Called Oculus function library on non-Oculus device or device was not found"));
#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS

	return nullptr;
}

void UOculusFunctionLibrary::GetPose(FRotator& DeviceRotation, FVector& DevicePosition, FVector& NeckPosition, bool bUseOrienationForPlayerCamera, bool bUsePositionForPlayerCamera, const FVector PositionScale)
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	FOculusRiftHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD && OculusHMD->IsHeadTrackingAllowed())
	{
		FQuat OrientationAsQuat;

		OculusHMD->GetCurrentHMDPose(OrientationAsQuat, DevicePosition, bUseOrienationForPlayerCamera, bUsePositionForPlayerCamera, PositionScale);

		DeviceRotation = OrientationAsQuat.Rotator();

		NeckPosition = OculusHMD->GetNeckPosition(OrientationAsQuat, DevicePosition, PositionScale);

		//UE_LOG(LogUHeadMountedDisplay, Log, TEXT("POS: %.3f %.3f %.3f"), DevicePosition.X, DevicePosition.Y, DevicePosition.Z);
		//UE_LOG(LogUHeadMountedDisplay, Log, TEXT("NECK: %.3f %.3f %.3f"), NeckPosition.X, NeckPosition.Y, NeckPosition.Z);
		//UE_LOG(LogUHeadMountedDisplay, Log, TEXT("ROT: sYaw %.3f Pitch %.3f Roll %.3f"), DeviceRotation.Yaw, DeviceRotation.Pitch, DeviceRotation.Roll);
	}
	else
#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
	{
		DeviceRotation = FRotator::ZeroRotator;
		DevicePosition = FVector::ZeroVector;
	}
}

void UOculusFunctionLibrary::SetBaseRotationAndBaseOffsetInMeters(FRotator Rotation, FVector BaseOffsetInMeters, EOrientPositionSelector::Type Options)
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	FOculusRiftHMD* OculusHMD = GetOculusHMD();
	if ((OculusHMD != nullptr) && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		if ((Options == EOrientPositionSelector::Orientation) || (Options == EOrientPositionSelector::OrientationAndPosition))
		{
			OculusHMD->SetBaseRotation(Rotation);
		}
		if ((Options == EOrientPositionSelector::Position) || (Options == EOrientPositionSelector::OrientationAndPosition))
		{
			OculusHMD->SetBaseOffsetInMeters(BaseOffsetInMeters);
		}
	}
#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::GetBaseRotationAndBaseOffsetInMeters(FRotator& OutRotation, FVector& OutBaseOffsetInMeters)
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	FOculusRiftHMD* OculusHMD = GetOculusHMD();
	if ((OculusHMD != nullptr) && GEngine->HMDDevice->IsHeadTrackingAllowed())
	{
		OutRotation = OculusHMD->GetBaseRotation();
		OutBaseOffsetInMeters = OculusHMD->GetBaseOffsetInMeters();
	}
	else
	{
		OutRotation = FRotator::ZeroRotator;
		OutBaseOffsetInMeters = FVector::ZeroVector;
	}
#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::GetRawSensorData(FVector& Accelerometer, FVector& Gyro, FVector& Magnetometer, float& Temperature, float& TimeInSeconds)
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	FOculusRiftHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		FOculusRiftHMD::SensorData Data;
		OculusHMD->GetRawSensorData(Data);

		Accelerometer = Data.Accelerometer;
		Gyro = Data.Gyro;
		Magnetometer = Data.Magnetometer;
		Temperature = Data.Temperature;
		TimeInSeconds = Data.TimeInSeconds;
	}
#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
}

bool UOculusFunctionLibrary::GetUserProfile(FHmdUserProfile& Profile)
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	FOculusRiftHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		FOculusRiftHMD::UserProfile Data;
		if (OculusHMD->GetUserProfile(Data))
		{
			Profile.Name = Data.Name;
			Profile.Gender = Data.Gender;
			Profile.PlayerHeight = Data.PlayerHeight;
			Profile.EyeHeight = Data.EyeHeight;
			Profile.IPD = Data.IPD;
			Profile.NeckToEyeDistance = Data.NeckToEyeDistance;
			Profile.ExtraFields.Reserve(Data.ExtraFields.Num());
			for (TMap<FString, FString>::TIterator It(Data.ExtraFields); It; ++It)
			{
				Profile.ExtraFields.Add(FHmdUserProfileField(*It.Key(), *It.Value()));
			}
			return true;
		}
	}
#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
	return false;
}

void UOculusFunctionLibrary::EnablePlayerControllerFollowHmd(bool bEnable)
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	FOculusRiftHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OculusHMD->GetSettings()->Flags.bPlayerControllerFollowsHmd = bEnable;
	}
#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
}

bool UOculusFunctionLibrary::IsPlayerControllerFollowHmdEnabled()
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	FOculusRiftHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		return OculusHMD->GetSettings()->Flags.bPlayerControllerFollowsHmd != 0;
	}
#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
	return true;
}

void UOculusFunctionLibrary::EnablePlayerCameraManagerFollowHmd(bool bFollowHmdOrientation, bool bFollowHmdPosition)
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	FOculusRiftHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OculusHMD->GetSettings()->Flags.bPlayerCameraManagerFollowsHmdOrientation = bFollowHmdOrientation;
		OculusHMD->GetSettings()->Flags.bPlayerCameraManagerFollowsHmdPosition = bFollowHmdPosition;
	}
#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::GetPlayerCameraManagerFollowHmd(bool& bFollowHmdOrientation, bool& bFollowHmdPosition)
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	FOculusRiftHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		bFollowHmdOrientation = OculusHMD->GetSettings()->Flags.bPlayerCameraManagerFollowsHmdOrientation;
		bFollowHmdPosition = OculusHMD->GetSettings()->Flags.bPlayerCameraManagerFollowsHmdPosition;
	}
#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
}
