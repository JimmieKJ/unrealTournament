// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeaponState.h"
#include "UTWeaponStateEquipping.h"
#include "UTWeaponStateEquipping_Enforcer.generated.h"



/** base class of states that fire the weapon and live in the weapon's FiringState array */
UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTWeaponStateEquipping_Enforcer : public UUTWeaponStateEquipping
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateEquipping_Enforcer(const FObjectInitializer& OI)
	: Super(OI)
	{}

	/** called to start the equip timer/anim; this isn't done automatically in BeginState() because we need to pass in any overflow time from the previous weapon's PutDown() */
	virtual void StartEquip(float OverflowTime) override;
};