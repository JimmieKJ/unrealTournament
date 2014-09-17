// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD.h"
#include "UTHUD_DM.h"
#include "UTHUD_TeamDM.h"
#include "UTHUDWidget_TeamScore.h"
#include "UTHUDWidget_DMPlayerScore.h"

AUTHUD_TeamDM::AUTHUD_TeamDM(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
}

FLinearColor AUTHUD_TeamDM::GetBaseHUDColor()
{
	FLinearColor TeamColor = Super::GetBaseHUDColor();

	AUTPlayerState* PS = Cast<AUTPlayerState>(UTPlayerOwner->PlayerState);
	if (PS != NULL && PS->Team != NULL)
	{
		TeamColor = PS->Team->TeamColor;
	}
	return TeamColor;
}
