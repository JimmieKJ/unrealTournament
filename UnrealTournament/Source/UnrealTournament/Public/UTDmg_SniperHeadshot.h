// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTDmg_SniperHeadshot.generated.h"

UCLASS(CustomConstructor)
class UUTDmg_SniperHeadshot : public UUTDamageType
{
	GENERATED_UCLASS_BODY()

	UUTDmg_SniperHeadshot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		GibHealthThreshold = -10000000;
		GibDamageThreshold = 1000000;
	}

	virtual void PlayDeathEffects_Implementation(AUTCharacter* DyingPawn) const override
	{
		DyingPawn->SpawnGib(DyingPawn->HeadBone);
		DyingPawn->SetHeadScale(0.0f);
	}
};