// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProjectile.h"
#include "UTProj_RocketSeeking.generated.h"

/**
 * 
 */
UCLASS()
class AUTProj_RocketSeeking : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintReadOnly, Replicated, Category = RocketSeeking)
	AActor* TargetActor;

	/**The speed added to velocity in the direction of the target*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketSeeking)
	float AdjustmentSpeed;

	virtual void Tick(float DeltaTime) override;
	
};
