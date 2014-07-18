// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD_CTF.h"
#include "UTCTFGAmeState.h"
#include "UTHUDWidget_CTFScore.h"

AUTHUD_CTF::AUTHUD_CTF(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	HudWidgetClasses.Add(UUTHUDWidget_CTFScore::StaticClass());
}


void AUTHUD_CTF::PostRender()
{
	Super::PostRender();
}

FLinearColor AUTHUD_CTF::GetBaseHUDColor()
{
	FLinearColor TeamColor = Super::GetBaseHUDColor();

	AUTPlayerState* PS = Cast<AUTPlayerState>(UTPlayerOwner->PlayerState);
	if (PS != NULL && PS->Team != NULL)
	{
		TeamColor = PS->Team->TeamColor;
	}


	return TeamColor;
}
