// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTDmgType_Suicide.h"

UUTDmgType_Suicide::UUTDmgType_Suicide(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ConsoleDeathMessage = NSLOCTEXT("UTDeathMessages", "DeathMessage_Suicide", "{Player2Name} suicided.");
	MaleSuicideMessage = ConsoleDeathMessage;
	FemaleSuicideMessage = ConsoleDeathMessage;
}

