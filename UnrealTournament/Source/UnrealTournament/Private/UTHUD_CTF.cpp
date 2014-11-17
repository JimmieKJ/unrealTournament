// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD_CTF.h"
#include "UTCTFGameState.h"
#include "UTHUDWidget_CTFScore.h"
#include "UTHUDWidget_CTFFlagStatus.h"
#include "UTHUDWidget_GameClock.h"
#include "UTHUDWidget_CTFPlayerScore.h"

AUTHUD_CTF::AUTHUD_CTF(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
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
