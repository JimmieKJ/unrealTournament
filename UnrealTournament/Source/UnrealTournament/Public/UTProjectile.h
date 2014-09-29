// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTResetInterface.h"
#include "UTProjectile.generated.h"

UCLASS(meta = (ChildCanTick))
class UNREALTOURNAMENT_API AUTProjectile : public AActor, public IUTResetInterface
{
	GENERATED_UCLASS_BODY()

	/** Sphere collision component */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Projectile)
	TSubobjectPtr<USphereComponent> CollisionComp;

	/** Projectile movement component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Projectile)
	TSubobjectPtr<class UProjectileMovementComponent> ProjectileMovement;

	/** additional Z axis speed added to projectile on spawn - NOTE: blueprint changes only work in defaults or construction script as value is applied to velocity on spawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Projectile)
	float TossZ;

	/** Percentage of instigator's velocity imparted to this projectile */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Projectile)
	float InstigatorVelocityPct;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	FRadialDamageParams DamageParams;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	TSubclassOf<UDamageType> MyDamageType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	float Momentum;
	/** whether the projectile can impact its Instigator (player who fired it) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	bool bCanHitInstigator;

	UPROPERTY(BlueprintReadWrite, Category = Projectile)
	AController* InstigatorController;
	/** if not NULL, Controller that gets credit for damage to enemies on the same team as InstigatorController (including damaging itself)
	 * this is used to grant two way kill credit for mechanics where the opposition is partially responsible for the damage(e.g.blowing up an enemy's projectile in flight)
	 */
	UPROPERTY(BlueprintReadWrite, Category = Projectile)
	AController* FFInstigatorController;
	/** when FFInstigatorController is assigned damage credit, optionally also change the damage type to this (primarily for death message clarity) */
	UPROPERTY(BlueprintReadWrite, Category = Damage)
	TSubclassOf<UDamageType> FFDamageType;

	/** actor we hit directly and already applied damage to */
	UPROPERTY()
	AActor* ImpactedActor;

	/** DEPRECATED */
	UPROPERTY(VisibleAnywhere, Category = Effects)
	UParticleSystem* ExplosionEffect;
	/** DEPRECATED */
	UPROPERTY(VisibleAnywhere, Category = Effects)
	USoundBase* ExplosionSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	TSubclassOf<class AUTImpactEffect> ExplosionEffects;

	/** Bounce effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
	UParticleSystem* BounceEffect;
	/** Sound played when projectile bounces off wall */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
	USoundBase* BounceSound;

	/** return base damage and momentum; OtherActor is set for direct hits and NULL when invoking radial damage (explosion) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Projectile)
	FRadialDamageParams GetDamageParams(AActor* OtherActor, const FVector& HitLocation, float& OutMomentum) const;

	/** this is used with SendInitialReplication() to force projectiles to be initially replicated prior to initial physics,
	 * which fixes most cases of projectiles never appearing on clients (because they got blown up before ever being sent)
	 * we use a special tick function instead of BeginPlay() so that COND_Initial replication conditions continue to work as expected
	 */
	FActorTickFunction InitialReplicationTick;

	/** send initial replication to all clients for which the projectile is relevant, called via InitialReplicationTick */
	void SendInitialReplication();

	/** set to force bReplicatedMovement for the next replication pass; this is used with SendInitialReplication() to make sure the client receives the post-physics location in addition
	 * to spawning with the pre-physics location
	 */
	bool bForceNextRepMovement;

	/** Fake client side projectile (spawned on owning client). */
	UPROPERTY()
	bool bFakeClientProjectile;

	/** Fake projectile on this client providing visuals for me */
	UPROPERTY()
	AUTProjectile* MyFakeProjectile;

	/** Perform any custom initialization for this projectile as fake client side projectile */
	virtual void InitFakeProjectile(class AUTPlayerController* OwningPlayer);

	/** true if already exploded (to avoid recursion, etc) */
	bool bExploded;

	virtual void BeginPlay() override;
	virtual void TornOff() override;
	virtual void Destroyed() override;

	/** simulate LifeSpan on the client side
	 * this is harmless because the Destroy() call won't work if the projectile isn't torn off
	 * and makes simulating lifetime related projectile effects easier on clients
	 */
	virtual void SetLifeSpan(float InLifespan) override
	{
		// Store the new value
		InitialLifeSpan = InLifespan;
		if (InLifespan > 0.0f)
		{
			GetWorldTimerManager().SetTimer(this, &AActor::LifeSpanExpired, InLifespan);
		}
		else
		{
			GetWorldTimerManager().ClearTimer(this, &AActor::LifeSpanExpired);
		}
	}
	// this override is needed because for some reason the above timer is considered a different delegate than that created by calling the same code in AActor...
	float GetLifeSpan() const override
	{
		// Timer remaining returns -1.0f if there is no such timer - return this as ZERO
		float CurrentLifespan = GetWorldTimerManager().GetTimerRemaining(this, &AActor::LifeSpanExpired);
		return (CurrentLifespan != -1.0f) ? CurrentLifespan : 0.0f;
	}

	virtual void PostNetReceiveLocationAndRotation() override;
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;
	virtual void PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker) override;

	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

	/** turns off projectile ambient effects, collision, physics, etc
	 * needed because we need a delay between explosion and actor destruction for replication purposes
	 */
	UFUNCTION(BlueprintCallable, Category = Projectile)
	virtual void ShutDown();
	/** blueprint hook for shutdown in case any blueprint-created effects need to be turned off */
	UFUNCTION(BlueprintImplementableEvent)
	void OnShutdown();

	virtual void Reset_Implementation() override
	{
		Destroy();
	}

	/** called when projectile hits something */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Projectile)
	void ProcessHit(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal);
	/** deal damage to Actor directly hit (note that this Actor will then be ignored for any radial damage) */
	UFUNCTION(BlueprintNativeEvent, Category = Projectile)
	void DamageImpactedActor(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal);
	UFUNCTION()
	virtual void OnStop(const FHitResult& Hit);
	UFUNCTION()
	virtual void OnBounce(const struct FHitResult& ImpactResult, const FVector& ImpactVelocity);
	UFUNCTION()
	virtual void OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Projectile)
	void Explode(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp = NULL);

	/** Whether this projectile always interacts with other projectiles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
	bool bAlwaysShootable;

	/** If true, this projectile always interacts with other energy projectiles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
	bool bIsEnergyProjectile;

	UFUNCTION(BlueprintCallable, Category = Projectile)
	virtual bool InteractsWithProj(AUTProjectile* OtherProj);

	/** Projectile size for hitting pawns */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Projectile)
		float OverlapRadius;

	/** Overlap sphere for hitting pawns */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Projectile)
	TSubobjectPtr<USphereComponent> PawnOverlapSphere;

protected:
	/** workaround to Instigator not exposed in blueprint spawn at engine level
	 * ONLY USED IN SPAWN ACTOR NODE
	 */
	UPROPERTY(BlueprintReadWrite, Meta = (ExposeOnSpawn = "true"), Category = "Spawn")
	APawn* SpawnInstigator;
};

