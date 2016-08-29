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

	// The Player State of the person killed
	UPROPERTY(Replicated, ReplicatedUsing = OnReceivedOwnerPlayerState)
	AUTPlayerState* OwnerPlayerState;

	// The player State of the killer
	UPROPERTY()
	AUTPlayerState* KillerPlayerState;

	UPROPERTY(Replicated)
	float Value;

	UPROPERTY()
	FRotator AutoRotate;

	UPROPERTY()
	float BounceZ;

	UFUNCTION()
	virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	virtual void Tick(float DeltaTime);
	virtual void PostInitializeComponents();

	UFUNCTION()
	void Init(AUTPlayerState* inOwnerPlayerState, AUTPlayerState* inKillerPlayerState, float inValue);

protected:

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = GameObject)
	USoundBase* PickupSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = GameObject)
	TArray<UMaterialInterface*> TeamMaterials;

	UFUNCTION()
	virtual void OnReceivedOwnerPlayerState();

	UPROPERTY()
	TArray<AUTCharacter*> TouchingCharacters;

};