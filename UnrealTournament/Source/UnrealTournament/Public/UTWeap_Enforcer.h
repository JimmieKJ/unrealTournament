// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UTWeap_Enforcer.generated.h"

UCLASS(abstract)
class AUTWeap_Enforcer : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	/** How much spread increases for each shot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Enforcer)
	float SpreadIncrease;

	/** How much spread increases for each shot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Enforcer)
		float SpreadResetInterval;

	/** Max spread  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Enforcer)
		float MaxSpread;

	/** Last time a shot was fired (used for calculating spread). */
	UPROPERTY(BlueprintReadWrite, Category = Enforcer)
		float LastFireTime;

	/** Stopping power against players with melee weapons.  Overrides normal momentum imparted by bullets. */
	UPROPERTY(BlueprintReadWrite, Category = Enforcer)
		float StoppingPower;

	virtual void PlayFiringEffects() override;
	virtual void FireInstantHit(bool bDealDamage, FHitResult* OutHit) override;

	virtual	float GetImpartedMomentumMag(AActor* HitActor) override;

};

