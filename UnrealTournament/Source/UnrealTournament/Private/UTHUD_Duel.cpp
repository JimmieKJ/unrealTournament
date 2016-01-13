// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD_DM.h"
#include "UTHUD_Duel.h"

AUTHUD_Duel::AUTHUD_Duel(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

FLinearColor AUTHUD_Duel::GetBaseHUDColor()
{
	return FLinearColor::White;
}
