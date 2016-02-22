// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeaponStateFiringBeam.h"

#include "UTWeaponStateFiringLinkBeam.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTWeaponStateFiringLinkBeam : public UUTWeaponStateFiringBeam
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	float AccumulatedFiringTime;

	UPROPERTY()
		bool bPendingEndFire;

	virtual void RefireCheckTimer() override;
	virtual void FireShot() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndState() override;
	virtual void EndFiringSequence(uint8 FireModeNum) override;
};
