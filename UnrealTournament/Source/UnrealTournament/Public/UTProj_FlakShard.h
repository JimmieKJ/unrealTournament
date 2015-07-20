// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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
class UNREALTOURNAMENT_API AUTProj_FlakShard : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	/** projectile mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Effects)
	class UMeshComponent* Mesh;

	/** material instance to apply heat amount to mesh */
	UPROPERTY(BlueprintReadWrite, Category = Effects)
	UMaterialInstanceDynamic* MeshMI;

	/** particle component for trail */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Effects)
	class UParticleSystemComponent* Trail;

	/** Limit number of bounces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flak Cannon")
	int32 BouncesRemaining;

	/** How much velocity is damped on first bounce. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flak Cannon")
		float FirstBounceDamping;

	/** How much velocity is damped on successive bounces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flak Cannon")
		float BounceDamping;

	/** Pct of damage applied on hits after shard has bounced. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flak Cannon")
		float BounceDamagePct;

	/** Max angle to randomize bounce, in radians. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flak Cannon")
		float RandomBounceCone;

	/** Minimum speed at which damage can be applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float MinDamageSpeed;

	/** damage lost per second in flight */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float DamageAttenuation;

	/** amount of time for full damage before attenuation starts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float DamageAttenuationDelay;

	/** self (instigator) damage lost per second in flight */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
		float SelfDamageAttenuation;

	/** amount of time for full damage before self damageattenuation starts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
		float SelfDamageAttenuationDelay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
		float FullGravityDelay;

	/** amount of time for the hot effects to fade */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	float HeatFadeTime;

	/** color for trail particles when hot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	FLinearColor HotTrailColor;

	/** color for trail particles after HeadFadeTime */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	FLinearColor ColdTrailColor;

	/** Number of extra visual shards inside this projectiles overlap sphere. */
	UPROPERTY(EditAnywhere, Category = Effects)
		int32 NumSatelliteShards;

	/* Extra visual shards. */
	UPROPERTY(BlueprintReadOnly, Category = Effects)
		TArray<UStaticMeshComponent*> SatelliteShards;

	virtual void RemoveSatelliteShards();

	virtual void PostInitializeComponents() override;

	virtual void ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal) override;
	virtual FRadialDamageParams GetDamageParams_Implementation(AActor* OtherActor, const FVector& HitLocation, float& OutMomentum) const override;
	virtual void OnBounce(const struct FHitResult& ImpactResult, const FVector& ImpactVelocity) override;

	virtual void Tick(float DeltaTime) override;
	virtual void CatchupTick(float CatchupTickDelta) override;
	virtual void BeginPlay() override;
};
