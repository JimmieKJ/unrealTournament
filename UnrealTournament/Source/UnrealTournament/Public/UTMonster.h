// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTCharacter.h"
#include "UTMonsterAI.h"

#include "UTMonster.generated.h"

UCLASS(Abstract)
class AUTMonster : public AUTCharacter
{
	GENERATED_BODY()
public:
	AUTMonster(const FObjectInitializer& OI)
		: Super(OI)
	{
		Cost = 5;
		bCanPickupItems = false;
		UTCharacterMovement->AutoSprintDelayInterval = 10000.0f;
	}
	/** display name shown on HUD/scoreboard/etc */
	UPROPERTY(EditDefaultsOnly)
	FText DisplayName;
	/** cost to spawn in the PvE game's point system */
	UPROPERTY(EditDefaultsOnly)
	int32 Cost;

	virtual void ApplyCharacterData(TSubclassOf<class AUTCharacterContent> Data)
	{}
};