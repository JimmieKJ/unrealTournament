// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeapon.h"
#include "UTWeaponStateUnequipping.h"
#include "UTWeaponStateUnequipping_Enforcer.generated.h"

/** base class of states that fire the weapon and live in the weapon's FiringState array */
UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTWeaponStateUnequipping_Enforcer : public UUTWeaponStateUnequipping
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateUnequipping_Enforcer(const FObjectInitializer& OI)
	: Super(OI)
	{}

	virtual void BeginState(const UUTWeaponState* PrevState) override;

};