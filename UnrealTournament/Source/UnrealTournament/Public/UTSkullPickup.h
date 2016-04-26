// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTDroppedPickup.h"
#include "UTSkullPickup.generated.h"

/** a dropped, partially used inventory item that was previously held by a player
* note that this is NOT a subclass of UTPickup
*/
UCLASS(NotPlaceable)
class UNREALTOURNAMENT_API AUTSkullPickup : public AUTDroppedPickup
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skull")
	uint8 SkullTeamIndex;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skull")
		USoundBase* SkullPickupSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skull")
	UParticleSystem* SkullPickupEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Skull")
		float BotDesireValue;

	virtual void SetInventory(AUTInventory* NewInventory) override;
	virtual bool AllowPickupBy_Implementation(APawn* Other, bool bDefaultAllowPickup) override;
	virtual void GiveTo_Implementation(APawn* Target) override;
	virtual USoundBase* GetPickupSound_Implementation() const override;
	virtual void PlayTakenEffects_Implementation(APawn* TakenBy) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const override;
	virtual float BotDesireability_Implementation(APawn* Asker, float PathDistance) override;
	virtual float DetourWeight_Implementation(APawn* Asker, float PathDistance) override;
};