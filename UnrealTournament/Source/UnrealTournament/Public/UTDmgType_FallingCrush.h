// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTDmgType_FallingCrush.generated.h"

UCLASS(CustomConstructor)
class UUTDmgType_FallingCrush : public UUTDamageType
{
	GENERATED_UCLASS_BODY()

	UUTDmgType_FallingCrush(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{
		bCausedByWorld = true;
		ConsoleDeathMessage = NSLOCTEXT("UTDeathMessages", "DeathMessage_FallingCrush", "{Player1Name} landed on {Player2Name}'s skull.");
		MaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "MaleSuicideMessage_FallingCrush", "{Player2Name} crushed himself.");
		FemaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "FemaleSuicideMessage_FallingCrush", "{Player2Name} crushed herself.");
	}
};