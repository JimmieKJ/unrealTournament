// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateInactive.generated.h"

/** base class of states that fire the weapon and live in the weapon's FiringState array */
UCLASS(CustomConstructor)
class UUTWeaponStateInactive : public UUTWeaponState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateInactive(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	virtual void BringUp()
	{
		GetOuterAUTWeapon()->GotoState(GetOuterAUTWeapon()->EquippingState);
	}
};