// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProjectile.generated.h"

UCLASS()
class AUTProjectile : public AActor
{
	GENERATED_UCLASS_BODY()

	/** Sphere collision component */
	UPROPERTY(VisibleDefaultsOnly, Category=Projectile)
	TSubobjectPtr<USphereComponent> CollisionComp;

	/** Projectile movement component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Movement)
	TSubobjectPtr<class UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	FRadialDamageParams DamageParams;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	TSubclassOf<UDamageType> MyDamageType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	float Momentum;

	UPROPERTY(BlueprintReadWrite, Category = Projectile)
	AController* InstigatorController;

	/** actor we hit directly and already applied damage to */
	UPROPERTY()
	AActor* ImpactedActor;

	// TODO: we should encapsulate all explosion aspects in a single struct
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
	UParticleSystem* ExplosionEffect;

	/** true if already exploded (to avoid recursion, etc) */
	bool bExploded;

	virtual void BeginPlay();
	virtual void TornOff();

	/** turns off projectile ambient effects, collision, physics, etc
	 * needed because we need a delay between explosion and actor destruction for replication purposes
	 */
	virtual void ShutDown();

	/** called when projectile hits something */
	UFUNCTION()
	void OnHit(AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION(BlueprintCallable, Category = Projectile)
	virtual void Explode(const FVector& HitLocation, const FVector& HitNormal);

protected:
	/** workaround to Instigator not exposed in blueprint spawn at engine level
	 * ONLY USED IN SPAWN ACTOR NODE
	 */
	UPROPERTY(BlueprintReadWrite, Meta = (ExposeOnSpawn = "true"), Category = "Spawn")
	APawn* SpawnInstigator;
};

