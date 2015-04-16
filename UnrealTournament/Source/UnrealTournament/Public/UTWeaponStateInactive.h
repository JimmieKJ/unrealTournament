// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateInactive.generated.h"

/** base class of states that fire the weapon and live in the weapon's FiringState array */
UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTWeaponStateInactive : public UUTWeaponState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateInactive(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	virtual void BringUp(float OverflowTime)
	{
		GetOuterAUTWeapon()->GotoEquippingState(OverflowTime);
	}
};