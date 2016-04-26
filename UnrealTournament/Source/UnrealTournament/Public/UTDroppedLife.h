// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProjectileMovementComponent.h"
#include "UTDroppedPickup.h"
#include "UTDroppedLife.generated.h"

const float DROPPED_LIFE_CAPTURE_TIME = 3.0f;

UCLASS(NotPlaceable)
class UNREALTOURNAMENT_API AUTDroppedLife : public AUTDroppedPickup
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(Replicated)
	AUTPlayerState* OwnerPlayerState;

	UPROPERTY(Replicated)
	float StealAmount;

	UPROPERTY(Replicated)
	int32 CaptureCount;

	UPROPERTY()
	FRotator AutoRotate;

	UPROPERTY()
	float BounceZ;

	UFUNCTION()
	virtual void OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	virtual void OnOverlapEnd(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	virtual void Tick(float DeltaTime);

	virtual void PostInitializeComponents();
	virtual void Destroyed() override;

	virtual void PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir);

protected:

	UPROPERTY()
	TArray<AUTCharacter*> TouchingCharacters;

};