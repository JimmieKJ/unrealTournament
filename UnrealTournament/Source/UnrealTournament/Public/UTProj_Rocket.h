// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProjectile.h"
#include "UTProj_Rocket.generated.h"

UCLASS()
class AUTProj_Rocket : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	/** If set, rocket seeks this target. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = RocketSeeking)
	AActor* TargetActor;

	/**The speed added to velocity in the direction of the target*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketSeeking)
		float AdjustmentSpeed;

	virtual void Tick(float DeltaTime) override;
};
