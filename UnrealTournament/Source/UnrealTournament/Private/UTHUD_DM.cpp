// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD_DM.h"
#include "UTHUDWidget_DMPlayerLeaderboard.h"

AUTHUD_DM::AUTHUD_DM(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	HudWidgetClasses.Add(UUTHUDWidget_DMPlayerLeaderboard::StaticClass());
}
