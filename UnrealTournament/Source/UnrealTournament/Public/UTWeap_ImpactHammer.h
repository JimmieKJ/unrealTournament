// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeap_ImpactHammer.generated.h"

UCLASS(abstract)
class UNREALTOURNAMENT_API AUTWeap_ImpactHammer : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	/** time that weapon must be charged for 100% damage/impulse */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactHammer)
	float FullChargeTime;

	/** minimum charge to activate autofire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactHammer)
	float MinAutoChargePct;

	/** minimum charge for full impact jump */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactHammer)
	float FullImpactChargePct;

	/** sound played when impact jumping (fire at wall) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactHammer)
	USoundBase* ImpactJumpSound;

	/** sound played when reach new charge level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactHammer)
	USoundBase* ChargeClickSound;

	/** set when automatically firing charged mode due to proximity */
	UPROPERTY(BlueprintReadWrite, Category = ImpactHammer)
	AActor* AutoHitTarget;

	/** Transient, used for improving autohit detection in net games. */
	UPROPERTY()
	bool bForwardPredictOwner;

	/** Trace dist for impact jumping against world geometry */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactHammer)
	float ImpactJumpTraceDist;

	/** return whether the automatic charged fire release should happen against the specified target */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	bool AllowAutoHit(AActor* PotentialTarget);

	/** called to make client mirror server autohit success */
	UFUNCTION(Client, Reliable)
	virtual void ClientAutoHit(AActor* Target);

	virtual FVector GetFireStartLoc(uint8 FireMode = 255) override;

	virtual bool NeedsAmmoDisplay_Implementation() const override
	{
		return false;
	}

	virtual void FireInstantHit(bool bDealDamage = true, FHitResult* OutHit = NULL) override;

	virtual void Tick(float DeltaTime) override;

	virtual void GivenTo(AUTCharacter* NewOwner, bool bAutoActivate) override;
	virtual bool CanAttack_Implementation(AActor* Target, const FVector& TargetLoc, bool bDirectOnly, bool bPreferCurrentMode, uint8& BestFireMode, FVector& OptimalTargetLoc) override;
	virtual float GetAISelectRating_Implementation() override;
	virtual float SuggestAttackStyle_Implementation() override
	{
		return 1.0f;
	}
	virtual float SuggestDefenseStyle_Implementation() override
	{
		return -0.5f;
	}
	virtual bool DoAssistedJump() override;

	virtual void FiringInfoUpdated_Implementation(uint8 InFireMode, uint8 FlashCount, FVector InFlashLocation) override;

	virtual void DrawWeaponCrosshair_Implementation(UUTHUDWidget* WeaponHudWidget, float RenderDelta) override;
};