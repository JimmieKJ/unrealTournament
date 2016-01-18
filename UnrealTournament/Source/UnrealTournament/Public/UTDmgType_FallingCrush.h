// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTDmgType_FallingCrush.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTDmgType_FallingCrush : public UUTDamageType
{
	GENERATED_UCLASS_BODY()

	UUTDmgType_FallingCrush(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		ConsoleDeathMessage = NSLOCTEXT("UTDeathMessages", "DeathMessage_FallingCrush", "{Player1Name} landed on {Player2Name}'s skull.");
		MaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "MaleSuicideMessage_FallingCrush", "{Player2Name} crushed himself.");
		FemaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "FemaleSuicideMessage_FallingCrush", "{Player2Name} crushed herself.");
	}
};