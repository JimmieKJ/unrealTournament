// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTDroppedPickup.h"
#include "UTPickupCrown.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTPickupCrown : public AUTDroppedPickup
{
	GENERATED_UCLASS_BODY()

	virtual void GiveTo_Implementation(APawn* Target) override;
	virtual void Destroyed();
	virtual void Tick( float DeltaSeconds );

};
