// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTCharacter.h"
#include "UTPickupAmmo.generated.h"

UCLASS(Blueprintable)
class AUTPickupAmmo : public AUTPickup
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Pickup)
	FStoredAmmo Ammo;

	virtual void ProcessTouch_Implementation(APawn* TouchedBy) override;
	virtual void GiveTo_Implementation(APawn* Target) override;

	virtual float BotDesireability_Implementation(APawn* Asker, float TotalDistance) override;
	virtual float DetourWeight_Implementation(APawn* Asker, float TotalDistance) override;
};