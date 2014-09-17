// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProjectile.h"
#include "UTProj_Redeemer.generated.h"

UCLASS(Abstract, meta = (ChildCanTick))
class AUTProj_Redeemer : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	FVector ExplodeHitLocation;
	float ExplodeMomentum;

	float ExplosionTimings[5];
	float ExplosionRadii[6];
	float CollisionFreeRadius;

	void Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp) override;

	void ExplodeStage(float RangeMultiplier);

	UFUNCTION()
	void ExplodeStage1();
	UFUNCTION()
	void ExplodeStage2();
	UFUNCTION()
	void ExplodeStage3();
	UFUNCTION()
	void ExplodeStage4();
	UFUNCTION()
	void ExplodeStage5();
	UFUNCTION()
	void ExplodeStage6();
};
