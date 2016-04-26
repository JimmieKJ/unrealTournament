// Dropped pickup that avoids being destroyed via environmental effects and has a HUD beacon
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTDroppedPickup.h"

#include "UTDroppedPickupImportant.generated.h"

UCLASS()
class AUTDroppedPickupImportant : public AUTDroppedPickup
{
	GENERATED_BODY()
public:
	AUTDroppedPickupImportant(const FObjectInitializer& OI);

	virtual void BeginPlay() override;
	virtual void MoveToSafeLocation(const FBox& AvoidZone);
	virtual void FellOutOfWorld(const class UDamageType& dmgType) override;
};