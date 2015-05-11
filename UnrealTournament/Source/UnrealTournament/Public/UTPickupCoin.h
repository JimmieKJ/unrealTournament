// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTDroppedPickup.h"
#include "UTPickupCoin.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTPickupCoin : public AUTDroppedPickup
{
	GENERATED_UCLASS_BODY()

	float Value;

	virtual void GiveTo_Implementation(APawn* Target) override;
	virtual void SetValue(float NewValue);

};
