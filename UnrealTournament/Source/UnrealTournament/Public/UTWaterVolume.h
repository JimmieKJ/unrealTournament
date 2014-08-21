// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTWaterVolume.generated.h"

/**
* PhysicsVolume: A bounding volume which affects actor physics
* Each AActor is affected at any time by one PhysicsVolume
*/

UCLASS(BlueprintType)
class AUTWaterVolume : public APhysicsVolume
{
	GENERATED_UCLASS_BODY()

	virtual void ActorEnteredVolume(class AActor* Other) override;
	virtual void ActorLeavingVolume(class AActor* Other) override;

	/** Sound played when actor enters this volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sounds)
		USoundBase* EntrySound;

	/** Sound played when actor exits this volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sounds)
		USoundBase* ExitSound;
};