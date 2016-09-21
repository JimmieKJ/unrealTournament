// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProjectile.h"
#include "UTMovementBaseInterface.h"
#include "UTProj_StingerShard2.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTProj_StingerShard2 : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	virtual void DamageImpactedActor_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal) override;
	virtual void OnPawnSphereOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	virtual void Tick(float DeltaTime);

	/** <= 0 disables separate direct hit logic */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 DirectHitDamage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DirectHitMomentum;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OverlapSphereGrowthRate;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxOverlapSphereSize;

	/** projectile explodes when cos angle (dot product result) between velocity direction and target direction is < this value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AirExplodeAngle;

	UPROPERTY()
	TArray<APawn*> PotentialTargets;
};