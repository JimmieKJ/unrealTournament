// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateActive.generated.h"

UCLASS(CustomConstructor)
class UUTWeaponStateActive : public UUTWeaponState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateActive(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	virtual void BeginState(const UUTWeaponState* PrevState) override;
	virtual void BeginFiringSequence(uint8 FireModeNum) override;
};