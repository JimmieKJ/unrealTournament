// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateFiring.h"

#include "UTWeaponStateInactive.generated.h"

/** base class of states that fire the weapon and live in the weapon's FiringState array */
UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTWeaponStateInactive : public UUTWeaponState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateInactive(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	virtual void BeginState(const UUTWeaponState* PrevState) override
	{
		for (int32 i = 0; i < GetOuterAUTWeapon()->FiringState.Num(); i++)
		{
			if (GetOuterAUTWeapon()->FiringState[i] != nullptr)
			{
				GetOuterAUTWeapon()->FiringState[i]->WeaponBecameInactive();
			}
		}
		GetOuterAUTWeapon()->ClearFireEvents();
		Super::BeginState(PrevState);
	}

	virtual void BringUp(float OverflowTime) override
	{
		GetOuterAUTWeapon()->GotoEquippingState(OverflowTime);
	}

	virtual void PutDown() override
	{}
};