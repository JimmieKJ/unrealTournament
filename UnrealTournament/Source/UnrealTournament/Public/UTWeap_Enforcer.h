// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UTWeap_Enforcer.generated.h"

UCLASS()
class AUTWeap_Enforcer : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	/** How much spread increases for each shot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Spread)
	float SpreadIncrease;

	/** How much spread increases for each shot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Burst)
		float SpreadResetInterval;

	/** Max spread  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Burst)
		float MaxSpread;

	/** How much spread increases for each shot */
	UPROPERTY(BlueprintReadWrite, Category = Burst)
		float LastFireTime;

	virtual void PlayFiringEffects() override;
	virtual void FireInstantHit(bool bDealDamage, FHitResult* OutHit) override;
};

