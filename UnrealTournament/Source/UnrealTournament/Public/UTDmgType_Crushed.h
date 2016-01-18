// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTDmgType_Crushed.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTDmgType_Crushed : public UUTDamageType
{
	GENERATED_UCLASS_BODY()

	UUTDmgType_Crushed(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		bCausedByWorld = true;
		ConsoleDeathMessage = NSLOCTEXT("UTDeathMessages", "DeathMessage_FallingCrush", "{Player1Name} caused {Player2Name} to be crushed.");
		MaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "MaleSuicideMessage_FallingCrush", "{Player2Name} was crushed by a large object.");
		FemaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "FemaleSuicideMessage_FallingCrush", "{Player2Name} was crushed by a large object.");
		SelfVictimMessage = NSLOCTEXT("UTDeathMessages", "SelfVictimMessage_Crushed", "You were crushed by a large object.");
	}
};