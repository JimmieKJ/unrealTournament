// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPlayerCameraManager.generated.h"

UCLASS()
class AUTPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_UCLASS_BODY()

	/** post process settings used when there are no post process volumes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = PostProcess)
	FPostProcessSettings DefaultPPSettings;
	
	FVector LastThirdPersonCameraLoc;

	UPROPERTY()
	float ThirdPersonCameraSmoothingSpeed;

	UPROPERTY()
	FVector EndGameFreeCamOffset;

	/** Offset to Z free camera position */
	UPROPERTY()
	float EndGameFreeCamDistance;

	/** get CameraStyle after state based and gametype based override logic
	 * generally UT code should always query the current camera style through this method to account for ragdoll, etc
	 */
	virtual FName GetCameraStyleWithOverrides() const;

	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;

	virtual void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
};

