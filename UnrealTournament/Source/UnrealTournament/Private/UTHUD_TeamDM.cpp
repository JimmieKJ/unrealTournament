// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD.h"
#include "UTHUD_DM.h"
#include "UTHUD_TeamDM.h"

AUTHUD_TeamDM::AUTHUD_TeamDM(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

FLinearColor AUTHUD_TeamDM::GetBaseHUDColor()
{
	FLinearColor TeamColor = Super::GetBaseHUDColor();
	APawn* HUDPawn = Cast<APawn>(UTPlayerOwner->GetViewTarget());
	if (HUDPawn)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(HUDPawn->PlayerState);
		if (PS != NULL && PS->Team != NULL)
		{
			TeamColor = PS->Team->TeamColor;
		}
	}
	return TeamColor;
}
