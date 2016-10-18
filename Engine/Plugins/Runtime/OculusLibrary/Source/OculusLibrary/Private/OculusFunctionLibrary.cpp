// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusFunctionLibraryPrivatePCH.h"
#include "OculusFunctionLibrary.h"
#include "AsyncLoadingSplash.h"

#include "IOculusRiftPlugin.h"

#define GEARVR_SUPPORTED_PLATFORMS (PLATFORM_ANDROID && PLATFORM_ANDROID_ARM)

#define OCULUS_SUPPORTED_PLATFORMS (OCULUS_RIFT_SUPPORTED_PLATFORMS || GEARVR_SUPPORTED_PLATFORMS)

#if OCULUS_SUPPORTED_PLATFORMS
	#include "HeadMountedDisplayCommon.h"
#endif //OCULUS_SUPPORTED_PLATFORMS

UOculusFunctionLibrary::UOculusFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FHeadMountedDisplay* UOculusFunctionLibrary::GetOculusHMD()
{
#if OCULUS_SUPPORTED_PLATFORMS
	if (GEngine && GEngine->HMDDevice.IsValid())
	{
		auto HMDType = GEngine->HMDDevice->GetHMDDeviceType();
		if (HMDType == EHMDDeviceType::DT_OculusRift || HMDType == EHMDDeviceType::DT_GearVR)
		{
			return static_cast<FHeadMountedDisplay*>(GEngine->HMDDevice.Get());
		}
	}
#endif
	return nullptr;
}

void UOculusFunctionLibrary::GetPose(FRotator& DeviceRotation, FVector& DevicePosition, FVector& NeckPosition, bool bUseOrienationForPlayerCamera, bool bUsePositionForPlayerCamera, const FVector PositionScale)
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
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
#endif // #if OCULUS_SUPPORTED_PLATFORMS
	{
		DeviceRotation = FRotator::ZeroRotator;
		DevicePosition = FVector::ZeroVector;
	}
}

void UOculusFunctionLibrary::SetBaseRotationAndBaseOffsetInMeters(FRotator Rotation, FVector BaseOffsetInMeters, EOrientPositionSelector::Type Options)
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
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
#endif // OCULUS_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::GetBaseRotationAndBaseOffsetInMeters(FRotator& OutRotation, FVector& OutBaseOffsetInMeters)
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OutRotation = OculusHMD->GetBaseRotation();
		OutBaseOffsetInMeters = OculusHMD->GetBaseOffsetInMeters();
	}
	else
	{
		OutRotation = FRotator::ZeroRotator;
		OutBaseOffsetInMeters = FVector::ZeroVector;
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::GetRawSensorData(FVector& AngularAcceleration, FVector& LinearAcceleration, FVector& AngularVelocity, FVector& LinearVelocity, float& TimeInSeconds)
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		FHeadMountedDisplay::SensorData Data;
		OculusHMD->GetRawSensorData(Data);

		AngularAcceleration = Data.AngularAcceleration;
		LinearAcceleration = Data.LinearAcceleration;
		AngularVelocity = Data.AngularVelocity;
		LinearVelocity = Data.LinearVelocity;
		TimeInSeconds = Data.TimeInSeconds;
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
}

bool UOculusFunctionLibrary::GetUserProfile(FHmdUserProfile& Profile)
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		FHeadMountedDisplay::UserProfile Data;
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
#endif // OCULUS_SUPPORTED_PLATFORMS
	return false;
}

void UOculusFunctionLibrary::EnablePlayerControllerFollowHmd(bool bEnable)
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OculusHMD->GetSettings()->Flags.bPlayerControllerFollowsHmd = bEnable;
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
}

bool UOculusFunctionLibrary::IsPlayerControllerFollowHmdEnabled()
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		return OculusHMD->GetSettings()->Flags.bPlayerControllerFollowsHmd != 0;
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
	return true;
}

void UOculusFunctionLibrary::EnablePlayerCameraManagerFollowHmd(bool bFollowHmdOrientation, bool bFollowHmdPosition)
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OculusHMD->GetSettings()->Flags.bPlayerCameraManagerFollowsHmdOrientation = bFollowHmdOrientation;
		OculusHMD->GetSettings()->Flags.bPlayerCameraManagerFollowsHmdPosition = bFollowHmdPosition;
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::GetPlayerCameraManagerFollowHmd(bool& bFollowHmdOrientation, bool& bFollowHmdPosition)
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		bFollowHmdOrientation = OculusHMD->GetSettings()->Flags.bPlayerCameraManagerFollowsHmdOrientation;
		bFollowHmdPosition = OculusHMD->GetSettings()->Flags.bPlayerCameraManagerFollowsHmdPosition;
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::SetPositionScale3D(FVector PosScale3D)
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OculusHMD->SetPositionScale3D(PosScale3D);
	}
#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::SetBaseRotationAndPositionOffset(FRotator BaseRot, FVector PosOffset, EOrientPositionSelector::Type Options)
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		if (Options == EOrientPositionSelector::Orientation || Options == EOrientPositionSelector::OrientationAndPosition)
		{
			GEngine->HMDDevice->SetBaseRotation(BaseRot);
		}
		if (Options == EOrientPositionSelector::Position || Options == EOrientPositionSelector::OrientationAndPosition)
		{
			OculusHMD->GetSettings()->PositionOffset = PosOffset;
		}
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::GetBaseRotationAndPositionOffset(FRotator& OutRot, FVector& OutPosOffset)
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		OutRot = OculusHMD->GetBaseRotation();
		OutPosOffset = OculusHMD->GetSettings()->PositionOffset;
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::AddLoadingSplashScreen(class UTexture2D* Texture, FVector TranslationInMeters, FRotator Rotation, FVector2D SizeInMeters, FRotator DeltaRotation, bool bClearBeforeAdd)
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		FAsyncLoadingSplash* Splash = OculusHMD->GetAsyncLoadingSplash();
		if (Splash)
		{
			if (bClearBeforeAdd)
			{
				Splash->ClearSplashes();
			}
			Splash->SetLoadingIconMode(false);

			FAsyncLoadingSplash::FSplashDesc Desc;
			Desc.LoadingTexture = Texture;
			Desc.QuadSizeInMeters = SizeInMeters;
			Desc.TransformInMeters = FTransform(Rotation, TranslationInMeters);
			Desc.DeltaRotation = FQuat(DeltaRotation);
			Splash->AddSplash(Desc);
		}
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::ClearLoadingSplashScreens()
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		FAsyncLoadingSplash* Splash = OculusHMD->GetAsyncLoadingSplash();
		if (Splash)
		{
			Splash->ClearSplashes();
			Splash->SetLoadingIconMode(false);
		}
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::ShowLoadingSplashScreen()
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		FAsyncLoadingSplash* Splash = OculusHMD->GetAsyncLoadingSplash();
		if (Splash)
		{
			Splash->SetLoadingIconMode(false);
			Splash->Show(FAsyncLoadingSplash::ShowManually);
		}
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::HideLoadingSplashScreen(bool bClear)
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		FAsyncLoadingSplash* Splash = OculusHMD->GetAsyncLoadingSplash();
		if (Splash)
		{
			Splash->Hide(FAsyncLoadingSplash::ShowManually);
			if (bClear)
			{
				Splash->ClearSplashes();
			}
		}
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::EnableAutoLoadingSplashScreen(bool bAutoShowEnabled)
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		FAsyncLoadingSplash* Splash = OculusHMD->GetAsyncLoadingSplash();
		if (Splash)
		{
			Splash->SetAutoShow(bAutoShowEnabled);
		}
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
}

bool UOculusFunctionLibrary::IsAutoLoadingSplashScreenEnabled()
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		FAsyncLoadingSplash* Splash = OculusHMD->GetAsyncLoadingSplash();
		if (Splash)
		{
			return Splash->IsAutoShow();
		}
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
	return false;
}

void UOculusFunctionLibrary::ShowLoadingIcon(class UTexture2D* Texture)
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		FAsyncLoadingSplash* Splash = OculusHMD->GetAsyncLoadingSplash();
		if (Splash)
		{
			Splash->ClearSplashes();
			FAsyncLoadingSplash::FSplashDesc Desc;
			Desc.LoadingTexture = Texture;
			Splash->AddSplash(Desc);
			Splash->SetLoadingIconMode(true);
			Splash->Show(FAsyncLoadingSplash::ShowManually);
		}
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::HideLoadingIcon()
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		FAsyncLoadingSplash* Splash = OculusHMD->GetAsyncLoadingSplash();
		if (Splash)
		{
			Splash->Hide(FAsyncLoadingSplash::ShowManually);
			Splash->ClearSplashes();
			Splash->SetLoadingIconMode(false);
		}
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
}

bool UOculusFunctionLibrary::IsLoadingIconEnabled()
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		FAsyncLoadingSplash* Splash = OculusHMD->GetAsyncLoadingSplash();
		if (Splash)
		{
			return Splash->IsLoadingIconMode();
		}
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
	return false;
}


void UOculusFunctionLibrary::SetLoadingSplashParams(FString TexturePath, FVector DistanceInMeters, FVector2D SizeInMeters, FVector RotationAxis, float RotationDeltaInDeg)
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		FAsyncLoadingSplash* Splash = OculusHMD->GetAsyncLoadingSplash();
		if (Splash)
		{
			Splash->ClearSplashes();
			Splash->SetLoadingIconMode(false);
			FAsyncLoadingSplash::FSplashDesc Desc;
			Desc.TexturePath = TexturePath;
			Desc.QuadSizeInMeters = SizeInMeters;
			Desc.TransformInMeters = FTransform(DistanceInMeters);
			Desc.DeltaRotation = FQuat(RotationAxis, FMath::DegreesToRadians(RotationDeltaInDeg));
			Splash->AddSplash(Desc);
		}
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
}

void UOculusFunctionLibrary::GetLoadingSplashParams(FString& TexturePath, FVector& DistanceInMeters, FVector2D& SizeInMeters, FVector& RotationAxis, float& RotationDeltaInDeg)
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		FAsyncLoadingSplash* Splash = OculusHMD->GetAsyncLoadingSplash();
		if (Splash)
		{
			FAsyncLoadingSplash::FSplashDesc Desc;
			if (Splash->GetSplash(0, Desc))
			{
				if (Desc.LoadingTexture && Desc.LoadingTexture->IsValidLowLevel())
				{
					TexturePath = Desc.LoadingTexture->GetPathName();
				}
				else
				{
					TexturePath = Desc.TexturePath;
				}
				DistanceInMeters = Desc.TransformInMeters.GetTranslation();
				SizeInMeters	 = Desc.QuadSizeInMeters;
				
				const FQuat rotation(Desc.DeltaRotation);
				rotation.ToAxisAndAngle(RotationAxis, RotationDeltaInDeg);
			}
		}
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
}

class IStereoLayers* UOculusFunctionLibrary::GetStereoLayers()
{
#if OCULUS_SUPPORTED_PLATFORMS
	FHeadMountedDisplay* OculusHMD = GetOculusHMD();
	if (OculusHMD != nullptr)
	{
		return OculusHMD;
	}
#endif // OCULUS_SUPPORTED_PLATFORMS
	return nullptr;
}
