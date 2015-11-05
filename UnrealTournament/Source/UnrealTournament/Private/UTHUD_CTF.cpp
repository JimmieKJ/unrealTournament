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

	if (GetWorld()->GetGameState()->GetMatchState() == MatchState::MatchIsAtHalftime)
	{
		// no match summary at half-time for now
		//GetWorldTimerManager().SetTimer(MatchSummaryHandle, this, &AUTHUD::OpenMatchSummary, 7.0f);
	}
	else
	{
		Super::NotifyMatchStateChange();
	}
}

void AUTHUD_CTF::DrawMinimapSpectatorIcons()
{
	AUTCTFGameState* GS = Cast<AUTCTFGameState>(GetWorld()->GetGameState());
	if (GS == NULL) return;

	const float RenderScale = float(Canvas->SizeY) / 1080.0f;

	for (int32 Team = 0; Team < 2; Team++)
	{
		AUTCTFFlagBase* Base = GS->GetFlagBase(Team);
		if (Base && Base->MyFlag && Base->MyFlag->Team)
		{
			FVector2D Pos = WorldToMapToScreen(Base->GetActorLocation());
			Canvas->DrawColor = (Team == 0) ? FColor(255, 0, 0, 255) : FColor(0, 0, 255, 255);
			Canvas->DrawTile(SelectedPlayerTexture, Pos.X - 12.0f * RenderScale, Pos.Y - 12.0f * RenderScale, 24.0f * RenderScale, 24.0f * RenderScale, 0.0f, 0.0f, SelectedPlayerTexture->GetSurfaceWidth(), SelectedPlayerTexture->GetSurfaceHeight());

			Pos = WorldToMapToScreen(Base->MyFlag->GetActorLocation());
			DrawMinimapIcon(OldHudTexture, Pos, FVector2D(30.f, 30.f), FVector2D(843.f, 87.f), FVector2D(43.f, 41.f), Base->MyFlag->Team->TeamColor, true);
		}
	}

	Super::DrawMinimapSpectatorIcons();
}


