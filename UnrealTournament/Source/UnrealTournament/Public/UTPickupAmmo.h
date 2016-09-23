// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTCharacter.h"
#include "UTPickupAmmo.generated.h"

UCLASS(Blueprintable)
class UNREALTOURNAMENT_API AUTPickupAmmo : public AUTPickup
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, AssetRegistrySearchable, Category = Pickup)
	FStoredAmmo Ammo;

	virtual bool AllowPickupBy_Implementation(APawn* Other, bool bDefaultAllowPickup) override;
	virtual void GiveTo_Implementation(APawn* Target) override;

	virtual float BotDesireability_Implementation(APawn* Asker, AController* RequestOwner, float TotalDistance) override;
	virtual float DetourWeight_Implementation(APawn* Asker, float TotalDistance) override;
};