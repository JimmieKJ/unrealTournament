// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD_CTF.h"
#include "UTCTFGameState.h"
#include "UTCTFGameMode.h"
#include "UTCTFScoreboard.h"

AUTHUD_CTF::AUTHUD_CTF(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

FLinearColor AUTHUD_CTF::GetBaseHUDColor()
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

void AUTHUD_CTF::NotifyMatchStateChange()
{
	UUTCTFScoreboard* CTFScoreboard = Cast<UUTCTFScoreboard>(MyUTScoreboard);
	if (CTFScoreboard != NULL)
	{
		if (GetWorld()->GetGameState()->GetMatchState() == MatchState::MatchIsAtHalftime || GetWorld()->GetGameState()->GetMatchState() == MatchState::WaitingPostMatch)
		{
			GetWorldTimerManager().SetTimer(CTFScoreboard->OpenScoringPlaysHandle, CTFScoreboard, &UUTCTFScoreboard::OpenScoringPlaysPage, 10.0f, false);
		}
		else
		{
			GetWorldTimerManager().ClearTimer(CTFScoreboard->OpenScoringPlaysHandle);
		}
	}
}
