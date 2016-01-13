// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTDmgType_FeignFail.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTDmgType_FeignFail : public UUTDamageType
{
	GENERATED_UCLASS_BODY()

	UUTDmgType_FeignFail(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		bCausedByWorld = true;
		ConsoleDeathMessage = NSLOCTEXT("UTDeathMessages", "DeathMessage_FeignFail", "{Player2Name} fell and couldn't get up.");
		MaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "MaleSuicideMessage_FeignFail", "{Player2Name} fell and he couldn't get up.");
		FemaleSuicideMessage = NSLOCTEXT("UTDeathMessages", "FemaleSuicideMessage_FeignFail", "{Player2Name} fell and she couldn't get up.");
		SelfVictimMessage = NSLOCTEXT("UTDeathMessages", "SelfVictimMessage_FeignFail", "You fell and you couldn't get up.");
	}
};