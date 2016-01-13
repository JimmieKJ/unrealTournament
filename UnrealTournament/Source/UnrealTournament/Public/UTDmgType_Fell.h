// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTDmgType_Fell.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTDmgType_Fell : public UUTDamageType
{
	GENERATED_UCLASS_BODY()

	UUTDmgType_Fell(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		bCausedByWorld = true;
		ConsoleDeathMessage = NSLOCTEXT("UTDeathMessages", "DeathMessage_Fell", "{Player1Name} knocked {Player2Name} over the edge.");
		MaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "MaleSuicideMessage_Fell", "{Player2Name} fell to his death.");
		FemaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "FemaleSuicideMessage_Fell", "{Player2Name} fell to her death.");
		SelfVictimMessage = NSLOCTEXT("UTDeathMessages", "SelfVictimMessage_Fell", "You left a small crater.");
	}
};