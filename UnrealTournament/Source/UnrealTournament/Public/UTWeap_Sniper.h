// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeap_Sniper.generated.h"

UCLASS(Abstract)
class AUTWeap_Sniper : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	/** target head area size bonus when moving slowly (crouch speed or less) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	float StoppedHeadshotScale;

	/** target head area size bonus when moving slowly (crouch speed or less) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	float SlowHeadshotScale;

	/** target head area size bonus when have just stopped accelerating */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	float AimedHeadshotScale;

	/** target head area size bonus when moving quickly */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	float RunningHeadshotScale;

	/** damage for headshot (instant hit only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InstantHitDamage)
	float HeadshotDamage;

	/** damage type for headshot (instant hit only, if NULL use standard damage type) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InstantHitDamage)
	TSubclassOf<UDamageType> HeadshotDamageType;

	virtual float GetHeadshotScale() const override;

	virtual AUTProjectile* FireProjectile();
	virtual void FireInstantHit(bool bDealDamage = true, FHitResult* OutHit = NULL);

	virtual float GetAISelectRating_Implementation() override;
};