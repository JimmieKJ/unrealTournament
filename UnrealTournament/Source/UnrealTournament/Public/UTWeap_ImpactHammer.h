// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UTWeap_ImpactHammer.generated.h"

UCLASS()
class AUTWeap_ImpactHammer : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	/** time that weapon must be charged for 100% damage/impulse */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactHammer)
	float FullChargeTime;

	/** minimum charge damage multiplier, even if user immediately releases button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactHammer)
	float MinChargePct;

	/** damage multiplier to self when hitting world geometry (impact jumps, etc) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactHammer)
	float SelfDamageScale;

	/** momentum multiplier to self when hitting world geometry (impact jumps, etc) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactHammer)
	float SelfForceScale;

	/** sound played when impact jumping (fire at wall) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactHammer)
	USoundBase* ImpactJumpSound;

	/** set when automatically firing charged mode due to proximity */
	UPROPERTY(BlueprintReadWrite, Category = ImpactHammer)
	AActor* AutoHitTarget;

	/** return whether the automatic charged fire release should happen against the specified target */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	bool AllowAutoHit(AActor* PotentialTarget);

	/** Velocity Z imparted by "easy" impact jump. Not charge or jump dependent (although get a small bonus with timed jump). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactJumping)
	float EasyImpactJumpZ;

	/** Damage imparted by "easy" impact jump. Not charge dependent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactJumping)
	float EasyImpactDamage;

	/** Max total horizontal velocity after impact jump. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactJumping)
	float ImpactMaxHorizontalVelocity;

	/** Max total vertical velocity after impact jump. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactJumping)
	float ImpactMaxVerticalVelocity;

	/** called to make client mirror server autohit success */
	UFUNCTION(Client, Reliable)
	virtual void ClientAutoHit(AActor* Target);

	virtual void FireInstantHit(bool bDealDamage = true, FHitResult* OutHit = NULL) override;

	virtual void Tick(float DeltaTime) override;
};