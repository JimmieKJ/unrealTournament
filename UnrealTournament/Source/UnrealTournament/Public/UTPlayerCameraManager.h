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

	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;

	virtual void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
};

