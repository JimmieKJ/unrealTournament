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
	UUTLocalPlayer* UTLP = Cast<UUTLocalPlayer>(UTPlayerOwner->Player);
	if (UTLP != nullptr)
	{
		if (GetWorld()->GetGameState()->GetMatchState() == MatchState::WaitingPostMatch
			|| GetWorld()->GetGameState()->GetMatchState() == MatchState::PlayerIntro)
		{
			OpenMatchSummary();			
		}
		else if (GetWorld()->GetGameState()->GetMatchState() == MatchState::MatchIsAtHalftime)
		{
			FTimerHandle TempHandle;
			GetWorldTimerManager().SetTimer(MatchSummaryHandle, this, &AUTHUD_CTF::OpenMatchSummary, 7.0f);
		}
		else
		{
			UTLP->CloseMatchSummary();
			GetWorldTimerManager().ClearTimer(MatchSummaryHandle);
		}
	}
}

void AUTHUD_CTF::OpenMatchSummary()
{
	UUTLocalPlayer* UTLP = Cast<UUTLocalPlayer>(UTPlayerOwner->Player);
	if (UTLP != nullptr)
	{
		UTLP->OpenMatchSummary(Cast<AUTGameState>(GetWorld()->GetGameState()));
	}
}

