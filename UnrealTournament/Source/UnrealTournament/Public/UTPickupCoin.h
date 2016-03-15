// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTDroppedPickup.h"
#include "UTPickupCoin.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTPickupCoin : public AUTPickupInventory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Game)
	int32 Value;

	virtual void GiveTo_Implementation(APawn* Target) override;
};
