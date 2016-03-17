// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD_CTF.h"
#include "UTCTFGameState.h"
#include "UTCTFGameMode.h"
#include "UTCTFScoreboard.h"

AUTHUD_CTF::AUTHUD_CTF(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bDrawMinimap = true;
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
		MyUTScoreboard->SetScoringPlaysTimer(GetWorld()->GetGameState()->GetMatchState() == MatchState::MatchIntermission || GetWorld()->GetGameState()->GetMatchState() == MatchState::WaitingPostMatch);
	}

	if (GetWorld()->GetGameState()->GetMatchState() != MatchState::MatchIntermission)
	{
		Super::NotifyMatchStateChange();
	}
}

void AUTHUD_CTF::DrawMinimapSpectatorIcons()
{
	Super::DrawMinimapSpectatorIcons();

	AUTCTFGameState* GS = Cast<AUTCTFGameState>(GetWorld()->GetGameState());
	if (GS == NULL) return;

	const float RenderScale = float(Canvas->SizeY) / 1080.0f;
	bool bShowAllFlags = UTPlayerOwner && UTPlayerOwner->UTPlayerState && UTPlayerOwner->UTPlayerState->bOnlySpectator;

	for (int32 TeamIndex = 0; TeamIndex < 2; TeamIndex++)
	{
		AUTCTFFlagBase* Base = GS->GetFlagBase(TeamIndex);
		if (Base && Base->MyFlag)
		{
			bool bCanPickupFlag = (!GS->OnSameTeam(Base, UTPlayerOwner) ? Base->MyFlag->bEnemyCanPickup : Base->MyFlag->bFriendlyCanPickup);
			FVector2D Pos = WorldToMapToScreen(Base->GetActorLocation());
			Canvas->DrawColor = (TeamIndex == 0) ? FColor(255, 0, 0, 255) : FColor(0, 0, 255, 255);
			Canvas->DrawTile(SelectedPlayerTexture, Pos.X - 12.0f * RenderScale, Pos.Y - 12.0f * RenderScale, 24.0f * RenderScale, 24.0f * RenderScale, 0.0f, 0.0f, SelectedPlayerTexture->GetSurfaceWidth(), SelectedPlayerTexture->GetSurfaceHeight());
			if (Base->MyFlag->Team && (bShowAllFlags || bCanPickupFlag || Base->MyFlag->IsHome()))
			{
				Pos = WorldToMapToScreen(Base->MyFlag->GetActorLocation());
				DrawMinimapIcon(HUDAtlas, Pos, FVector2D(24.f, 24.f), FVector2D(843.f, 87.f), FVector2D(43.f, 41.f), Base->MyFlag->Team->TeamColor, true);
			}
		}
	}
}

bool AUTHUD_CTF::ShouldInvertMinimap()
{
	AUTCTFGameState* GS = Cast<AUTCTFGameState>(GetWorld()->GetGameState());
	if (GS == NULL)
	{
		return false;
	}

	// make sure this player's base is at the bottom
	if (UTPlayerOwner && UTPlayerOwner->UTPlayerState && UTPlayerOwner->UTPlayerState->Team)
	{
		AUTCTFFlagBase* HomeBase = GS->GetFlagBase(UTPlayerOwner->UTPlayerState->Team->TeamIndex);
		if (HomeBase)
		{
			FVector2D HomeBasePos(WorldToMapToScreen(HomeBase->GetActorLocation()));
			for (int32 TeamIndex = 0; TeamIndex < 2; TeamIndex++)
			{
				AUTCTFFlagBase* EnemyBase = GS->GetFlagBase(TeamIndex);
				FVector2D BasePos(WorldToMapToScreen(EnemyBase->GetActorLocation()));
				if ((EnemyBase != HomeBase) && (BasePos.Y > HomeBasePos.Y))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool AUTHUD_CTF::ShouldDrawMinimap() const
{
	return bDrawCTFMinimapHUDSetting && Super::ShouldDrawMinimap();
}

