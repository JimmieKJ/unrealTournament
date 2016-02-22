// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTDmgType_Drown.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTDmgType_Drown : public UUTDamageType
{
	GENERATED_UCLASS_BODY()

	UUTDmgType_Drown(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		bCausedByWorld = true;
		bBlockedByArmor = false;
		bCausesBlood = false;
		bCausesPainSound = false;
		ConsoleDeathMessage = NSLOCTEXT("UTDeathMessages", "DeathMessage_Drown", "{Player1Name} caused {Player2Name} to drown.");
		MaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "MaleSuicideMessage_Drown", "{Player2Name} drowned.");
		FemaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "FemaleSuicideMessage_Drown", "{Player2Name} drowned.");
		SelfVictimMessage = NSLOCTEXT("UTDeathMessages", "SelfVictimMessage_Drown", "You learned how long you can stay underwater.");
	}
};