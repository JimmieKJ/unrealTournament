// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTRemoteRedeemer.h"
#include "UTWeap_Redeemer.generated.h"

UCLASS(Abstract)
class AUTWeap_Redeemer : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category=Weapon)
	TSubclassOf<AUTRemoteRedeemer> RemoteRedeemerClass;

	/** Sound to play on bring up - temp here because we don't have really Redeemer bringup anim */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
		USoundBase* BringupSound;

	/** Sound to play on bring up - temp here because we don't have really Redeemer bringup anim */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
		USoundBase* PutDownSound;

	virtual AUTProjectile* FireProjectile() override;

	virtual void BringUp(float OverflowTime) override;
	virtual bool PutDown() override;

	virtual float SuggestAttackStyle_Implementation() override;
	virtual float SuggestDefenseStyle_Implementation() override;
	virtual float GetAISelectRating_Implementation() override;
	virtual bool CanAttack_Implementation(AActor* Target, const FVector& TargetLoc, bool bDirectOnly, bool bPreferCurrentMode, uint8& BestFireMode, FVector& OptimalTargetLoc) override;
};