// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPickupAmmo.generated.h"

UCLASS(Blueprintable)
class AUTPickupAmmo : public AUTPickup
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Pickup)
	FStoredAmmo Ammo;

	virtual void ProcessTouch_Implementation(APawn* TouchedBy) OVERRIDE;
	virtual void GiveTo_Implementation(APawn* Target) OVERRIDE;
};