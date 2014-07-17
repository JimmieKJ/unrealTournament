// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeapon.h"
#include "UTWeap_BioRifle.generated.h"

/**
 * 
 */
UCLASS()
class AUTWeap_BioRifle : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	UAnimMontage* ChargeAnimation;

	/** maximum number of globs we can charge for the alternate fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	uint8 MaxGlobStrength;

	/** Delay between consuming globs for the alternate fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bio)
	float GlobConsumeTime;

	virtual void OnStartedFiring_Implementation() override;
	virtual void OnContinuedFiring_Implementation() override;
	virtual void OnStoppedFiring_Implementation() override;

	virtual void StartCharge();

	virtual void FireShot() override;

	virtual void IncreaseGlobStrength();
	virtual void ClearGlobStrength();
	uint8 GlobStrength;

	/** hook to to play glob anim while charging*/
	UFUNCTION(BlueprintNativeEvent, Category = Bio)
	void OnStartCharging();

	/** hook to to play glob anim while charging*/
	UFUNCTION(BlueprintNativeEvent, Category = Bio)
	void OnChargeShot();
};
