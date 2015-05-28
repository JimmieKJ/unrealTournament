// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTDmgType_Telefragged.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTDmgType_Telefragged : public UUTDamageType
{
	GENERATED_UCLASS_BODY()

	UUTDmgType_Telefragged(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		bCausedByWorld = true;
		ConsoleDeathMessage = NSLOCTEXT("UTDeathMessages", "DeathMessage_Telefragged", "{Player2Name}'s atoms were scattered by {Player1Name}.");
		MaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "MaleSuicideMessage_Telefragged", "{Player2Name} telefragged himself.");
		FemaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "FemaleSuicideMessage_Telefragged", "{Player2Name} telefragged herself.");
		GibHealthThreshold = 10000000;
	}
};