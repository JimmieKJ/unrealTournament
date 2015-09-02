// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeaponStateFiringBeam.h"

#include "UTWeaponStateFiringLinkBeam.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTWeaponStateFiringLinkBeam : public UUTWeaponStateFiringBeam
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	float AccumulatedFiringTime;

    virtual void FireShot() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndState() override;
};
