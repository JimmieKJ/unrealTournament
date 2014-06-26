// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD.h"
#include "UTHUD_TeamDM.h"
#include "UTHUDWidget_TeamScore.h"

AUTHUD_TeamDM::AUTHUD_TeamDM(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	HudWidgetClasses.Add(UUTHUDWidget_TeamScore::StaticClass());
}