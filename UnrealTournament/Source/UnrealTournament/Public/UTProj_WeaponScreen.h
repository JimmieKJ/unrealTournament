// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UTProj_WeaponScreen.generated.h"

/** projectile that expands and explodes other projectiles, transferring Instigator so the shooter of this projectile gets kill credit */
UCLASS()
class AUTProj_WeaponScreen : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	/** blocked projectiles are changed to this damage type so there's a more informative death message explaining the change in damage credit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponScreen)
	TSubclassOf<UDamageType> BlockedProjDamageType;

	/** rate at which to scale per second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponScreen)
	FVector ScaleRate;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser) override;
	virtual void ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal) override;
	virtual void Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal) override
	{
		// don't explode on general impacts
	}
	virtual void OnStop(const FHitResult& Hit) override;
	virtual void Tick(float DeltaTime) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = Collision)
	TSubobjectPtr<UBoxComponent> CollisionBox;
};