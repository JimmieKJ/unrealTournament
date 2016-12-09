// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "IHeadMountedDisplay.h"
#include "IOculusLibraryPlugin.h"
#include "OculusFunctionLibrary.h"

class FOculusLibraryPlugin : public IOculusLibraryPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}

	virtual void GetPose(FRotator& DeviceRotation, FVector& DevicePosition, FVector& NeckPosition, bool bUseOrienationForPlayerCamera = false, bool bUsePositionForPlayerCamera = false, const FVector PositionScale = FVector::ZeroVector) override
	{
		UOculusFunctionLibrary::GetPose(DeviceRotation, DevicePosition, NeckPosition, bUseOrienationForPlayerCamera, bUsePositionForPlayerCamera, PositionScale);
	}

	virtual void GetRawSensorData(FVector& AngularAcceleration, FVector& LinearAcceleration, FVector& AngularVelocity, FVector& LinearVelocity, float& TimeInSeconds) override
	{
		UOculusFunctionLibrary::GetRawSensorData(AngularAcceleration, LinearAcceleration, AngularVelocity, LinearVelocity, TimeInSeconds);
	}

	virtual bool GetUserProfile(struct FHmdUserProfile& Profile) override
	{
		return UOculusFunctionLibrary::GetUserProfile(Profile);
	}

	virtual void SetBaseRotationAndBaseOffsetInMeters(FRotator Rotation, FVector BaseOffsetInMeters, EOrientPositionSelector::Type Options) override
	{
		UOculusFunctionLibrary::SetBaseRotationAndBaseOffsetInMeters(Rotation, BaseOffsetInMeters, Options);
	}

	virtual void GetBaseRotationAndBaseOffsetInMeters(FRotator& OutRotation, FVector& OutBaseOffsetInMeters) override
	{
		UOculusFunctionLibrary::GetBaseRotationAndBaseOffsetInMeters(OutRotation, OutBaseOffsetInMeters);
	}

	virtual void EnablePlayerControllerFollowHmd(bool bEnable) override
	{
		UOculusFunctionLibrary::EnablePlayerControllerFollowHmd(bEnable);
	}

	virtual bool IsPlayerControllerFollowHmdEnabled() override
	{
		return UOculusFunctionLibrary::IsPlayerControllerFollowHmdEnabled();
	}

	virtual void EnablePlayerCameraManagerFollowHmd(bool bFollowHmdOrientation, bool bFollowHmdPosition) override
	{
		UOculusFunctionLibrary::EnablePlayerCameraManagerFollowHmd(bFollowHmdOrientation, bFollowHmdPosition);
	}

	virtual void GetPlayerCameraManagerFollowHmd(bool& bFollowHmdOrientation, bool& bFollowHmdPosition) override
	{
		UOculusFunctionLibrary::GetPlayerCameraManagerFollowHmd(bFollowHmdOrientation, bFollowHmdPosition);
	}

	virtual void SetBaseRotationAndPositionOffset(FRotator BaseRot, FVector PosOffset, EOrientPositionSelector::Type Options) override
	{
		UOculusFunctionLibrary::SetBaseRotationAndPositionOffset(BaseRot, PosOffset, Options);
	}

	virtual void GetBaseRotationAndPositionOffset(FRotator& OutRot, FVector& OutPosOffset) override
	{
		UOculusFunctionLibrary::GetBaseRotationAndPositionOffset(OutRot, OutPosOffset);
	}

	virtual class IStereoLayers* GetStereoLayers() override
	{
		return UOculusFunctionLibrary::GetStereoLayers();
	}
};

IMPLEMENT_MODULE( FOculusLibraryPlugin, OculusLibrary )






