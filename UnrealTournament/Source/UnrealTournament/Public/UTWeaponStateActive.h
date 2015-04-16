// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateActive.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTWeaponStateActive : public UUTWeaponState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateActive(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	virtual void BeginState(const UUTWeaponState* PrevState) override;
	virtual bool BeginFiringSequence(uint8 FireModeNum, bool bClientFired) override;
};