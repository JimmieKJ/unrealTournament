// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTWaterVolume.generated.h"

/**
* PhysicsVolume: A bounding volume which affects actor physics
* Each AActor is affected at any time by one PhysicsVolume
*/

UCLASS(BlueprintType)
class UNREALTOURNAMENT_API AUTWaterVolume : public APhysicsVolume
{
	GENERATED_UCLASS_BODY()

	virtual void ActorEnteredVolume(class AActor* Other) override;
	virtual void ActorLeavingVolume(class AActor* Other) override;

	/** Sound played when actor enters this volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Sounds)
		USoundBase* EntrySound;

	/** Sound played when actor exits this volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Sounds)
		USoundBase* ExitSound;

	/** Pawn Velocity reduction on entry (scales velocity Z component)*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CharacterMovement)
	float PawnEntryVelZScaling;

	/** Pawn braking ability in this fluid */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CharacterMovement)
	float BrakingDecelerationSwimming;

	/** Current water current direction (if GetCurrentFor() is not implemented */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CharacterMovement)
	FVector WaterCurrentDirection;

	/** Current water current magnitude (if GetCurrentFor() is not implemented */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CharacterMovement)
		float WaterCurrentSpeed;

	/** Max relative downstream swim speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CharacterMovement)
		float MaxRelativeSwimSpeed;

	/** allows blueprint to provide custom current velocity for an actor. */
	UFUNCTION(BlueprintNativeEvent, Category=WaterVolume)
	FVector GetCurrentFor(AActor* Actor) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		FText VolumeName;
};

