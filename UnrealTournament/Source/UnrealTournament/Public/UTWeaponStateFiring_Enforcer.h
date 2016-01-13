// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateFiring.h"
#include "UTWeaponStateFiring_Enforcer.generated.h"

/** base class of states that fire the weapon and live in the weapon's FiringState array */
UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTWeaponStateFiring_Enforcer : public UUTWeaponStateFiring
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateFiring_Enforcer(const FObjectInitializer& OI)
	: Super(OI)
	{}

	virtual void BeginState(const UUTWeaponState* PrevState) override;
	virtual void EndState() override;

	virtual void UpdateTiming() override;

	virtual void PutDown() override;
};