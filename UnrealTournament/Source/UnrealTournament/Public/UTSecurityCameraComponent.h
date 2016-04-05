// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "UTSecurityCameraComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREALTOURNAMENT_API UUTSecurityCameraComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UUTSecurityCameraComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "SecurityCam")
		float DetectionRadius;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "SecurityCam")
		bool bCameraEnabled;

	UPROPERTY(BlueprintReadOnly, Category = "SecurityCam")
		class AUTCharacter* DetectedFlagCarrier;

	/** Event called when a flag carrier is detected that wasn't previously tracked. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SecurityCam")
		void OnFlagCarrierDetected(class AUTCharacter* FlagCarrier);

	/** Event called when a flag carrier that was detected is no longer tracked. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SecurityCam")
		void OnFlagCarrierDetectionLost(class AUTCharacter* FlagCarrier);
};


