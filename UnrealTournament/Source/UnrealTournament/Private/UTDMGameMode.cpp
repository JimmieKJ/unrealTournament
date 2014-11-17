// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUD_DM.h"
#include "UTDMGameMode.h"


AUTDMGameMode::AUTDMGameMode(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	HUDClass = AUTHUD_DM::StaticClass();
	DisplayName = NSLOCTEXT("UTGameMode", "DM", "Deathmatch");
}
