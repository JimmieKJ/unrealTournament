// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProj_WeaponScreen.generated.h"

/** projectile that expands and explodes other projectiles, transferring Instigator so the shooter of this projectile gets kill credit */
UCLASS()
class UNREALTOURNAMENT_API AUTProj_WeaponScreen : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	/** if set, friendly projectiles are ignored */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponScreen)
	bool bOnlyAffectEnemies;

	/** whether this projectile dies on blocking collision */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponScreen)
	bool bExplodeOnStop;

	/** if set, slow projectiles down instead of blowing them up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponScreen)
	bool bSlowProjectiles;

	/** if set, apply momentum to pawns touched by the projectile */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponScreen)
	bool bCauseMomentumToPawns;

	/** Momentum scaling for friendly pawns */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponScreen)
	float FriendlyMomentumScaling;

	/** Pawns touched so far (so no repeat hits) */
	UPROPERTY(BlueprintReadWrite, Category = WeaponScreen)
	TArray<APawn*> HitPawns;

	/** blocked projectiles are changed to this damage type so there's a more informative death message explaining the change in damage credit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponScreen)
	TSubclassOf<UDamageType> BlockedProjDamageType;

	/** rate at which to scale per second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponScreen)
	FVector ScaleRate;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser) override;
	virtual void ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal) override;
	virtual void Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp = NULL) override
	{
		// don't explode on general impacts
	}
	virtual void TornOff() override;
	virtual void OnStop(const FHitResult& Hit) override;
	virtual void Tick(float DeltaTime) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = Collision)
	UBoxComponent* CollisionBox;
};