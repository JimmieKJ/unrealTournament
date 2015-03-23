// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPickupHealth.generated.h"

UCLASS(Blueprintable, Abstract)
class UNREALTOURNAMENT_API AUTPickupHealth : public AUTPickup
{
	GENERATED_UCLASS_BODY()

	/** amount of health to restore */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Pickup)
	int32 HealAmount;
	/** if true, heal amount goes to SuperHealthMax instead of HealthMax */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Pickup)
	bool bSuperHeal;

	/** return the upper limit this pickup can increase the character's health to */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Pickup)
	int32 GetHealMax(AUTCharacter* P);

	virtual bool AllowPickupBy_Implementation(APawn* TouchedBy, bool bDefaultAllowPickup) override;
	virtual void GiveTo_Implementation(APawn* Target) override;

	virtual float BotDesireability_Implementation(APawn* Asker, float PathDistance) override;
	virtual float DetourWeight_Implementation(APawn* Asker, float PathDistance) override;
};