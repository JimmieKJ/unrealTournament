// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTDmgType_Drown.generated.h"

UCLASS(CustomConstructor)
class UUTDmgType_Drown : public UUTDamageType
{
	GENERATED_UCLASS_BODY()

	UUTDmgType_Drown(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{
		bCausedByWorld = true;
		bBlockedByArmor = false;
		bCausesBlood = false;

		ConsoleDeathMessage = NSLOCTEXT("UTDeathMessages", "DeathMessage_Fell", "{Player1Name} caused {Player2Name} to drown.");
		MaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "MaleSuicideMessage_Fell", "{Player2Name} drowned.");
		FemaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "FemaleSuicideMessage_Fell", "{Player2Name} drowned.");
	}
};