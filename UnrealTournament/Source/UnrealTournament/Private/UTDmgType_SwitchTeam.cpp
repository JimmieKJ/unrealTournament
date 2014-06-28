// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTDmgType_SwitchTeam.h"

UUTDmgType_SwitchTeam::UUTDmgType_SwitchTeam(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bDontCountForKills = true;
}

