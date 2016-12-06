// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTBot.h"

#include "UTMonsterAI.generated.h"

UCLASS()
class AUTMonsterAI : public AUTBot
{
	GENERATED_BODY()
public:
	/** class of monster to use, set when the monster can respawn so the controller is left around */
	UPROPERTY()
	TSubclassOf<AUTCharacter> PawnClass;

	virtual void CheckWeaponFiring(bool bFromWeapon = true) override;
};