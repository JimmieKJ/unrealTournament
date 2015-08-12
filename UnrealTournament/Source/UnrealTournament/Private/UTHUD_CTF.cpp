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
	if (MyUTScoreboard != NULL)
	{
		MyUTScoreboard->SetScoringPlaysTimer(GetWorld()->GetGameState()->GetMatchState() == MatchState::MatchIsAtHalftime || GetWorld()->GetGameState()->GetMatchState() == MatchState::WaitingPostMatch);
	}

	UUTLocalPlayer* UTLP = Cast<UUTLocalPlayer>(UTPlayerOwner->Player);
	if (UTLP != nullptr)
	{
		if (GetWorld()->GetGameState()->GetMatchState() == MatchState::WaitingPostMatch
			|| GetWorld()->GetGameState()->GetMatchState() == MatchState::PlayerIntro
			|| GetWorld()->GetGameState()->GetMatchState() == MatchState::MatchIsAtHalftime)
		{

			UTLP->OpenMatchSummary(Cast<AUTGameState>(GetWorld()->GetGameState()));
		}
		else
		{
			UTLP->CloseMatchSummary();
		}
	}
}

