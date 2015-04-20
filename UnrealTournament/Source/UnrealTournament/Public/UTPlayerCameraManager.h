// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPlayerCameraManager.generated.h"

UCLASS(Config = Game)
class UNREALTOURNAMENT_API AUTPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_UCLASS_BODY()

	/** post process settings used when there are no post process volumes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = PostProcess)
	FPostProcessSettings DefaultPPSettings;

	/** post process settings used when we want to be in stylized mode */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = PostProcess)
	TArray<FPostProcessSettings> StylizedPPSettings;

	FVector LastThirdPersonCameraLoc;

	UPROPERTY()
		AActor* LastThirdPersonTarget;

	UPROPERTY(Config)
	float ThirdPersonCameraSmoothingSpeed;

	UPROPERTY()
	FVector FlagBaseFreeCamOffset;
	
	UPROPERTY()
	FVector EndGameFreeCamOffset;

	/** Offset to Z free camera position */
	UPROPERTY()
	float EndGameFreeCamDistance;

	/** Sweep to find valid third person camera offset. */
	virtual void CheckCameraSweep(FHitResult& OutHit, AActor* TargetActor, const FVector& Start, const FVector& End);

	/** get CameraStyle after state based and gametype based override logic
	 * generally UT code should always query the current camera style through this method to account for ragdoll, etc
	 */
	virtual FName GetCameraStyleWithOverrides() const;

	virtual void UpdateCamera(float DeltaTime) override;
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;
	virtual void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
};

