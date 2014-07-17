// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProjectile.h"
#include "UTProj_FlakShard.generated.h"

/**
 * Flak Cannon Shard
 * - Can bounce up to 2 times
 * - Deals damage only if moving fast enough
 * - Damage is decreased over time
 */
UCLASS(Abstract, meta = (ChildCanTick))
class AUTProj_FlakShard : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	/** Limit number of bounces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flak Cannon")
	int32 BouncesRemaining;

	/** Increment lifespan when projectile starts its last bounce */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flak Cannon")
	float BounceFinalLifeSpanIncrement;

	/** Minimum speed at which damage can be applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float MinDamageSpeed;
	/** damage lost per second in flight */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float DamageAttenuation;
	/** amount of time for full damage before attenuation starts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float DamageAttenuationDelay;

	virtual void ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal) override;
	virtual FRadialDamageParams GetDamageParams_Implementation(AActor* OtherActor, const FVector& HitLocation, float& OutMomentum) const override;
	virtual void OnBounce(const struct FHitResult& ImpactResult, const FVector& ImpactVelocity) override;
};
