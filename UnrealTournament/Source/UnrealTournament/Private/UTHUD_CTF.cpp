// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD_CTF.h"
#include "UTCTFGameState.h"
#include "UTCTFGameMode.h"
#include "UTCTFScoreboard.h"

AUTHUD_CTF::AUTHUD_CTF(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bDrawMinimap = false;
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
	if (GetWorld()->GetGameState()->GetMatchState() != MatchState::MatchIntermission)
	{
		Super::NotifyMatchStateChange();
	}
}

int32 AUTHUD_CTF::GetScoreboardPage() 
{ 
	return ((GetWorld()->GetGameState()->GetMatchState() == MatchState::MatchIntermission) || GetWorld()->GetGameState()->HasMatchEnded()) ? 1 : ScoreboardPage;
};


void AUTHUD_CTF::DrawMinimapSpectatorIcons()
{
	Super::DrawMinimapSpectatorIcons();

	AUTCTFGameState* GS = Cast<AUTCTFGameState>(GetWorld()->GetGameState());
	if (GS == NULL) return;

	const float RenderScale = ( float(Canvas->SizeY) / 1080.0f) * GetHUDMinimapScale();
	bool bShowAllFlags = UTPlayerOwner && UTPlayerOwner->UTPlayerState && UTPlayerOwner->UTPlayerState->bOnlySpectator;

	for (int32 TeamIndex = 0; TeamIndex < 2; TeamIndex++)
	{
		AUTCTFFlagBase* Base = GS->GetFlagBase(TeamIndex);
		AUTCarriedObject* Flag = Base ? Base->MyFlag : nullptr;
		if (Flag)
		{
			bool bCanPickupFlag = (!GS->OnSameTeam(Base, UTPlayerOwner) ? Flag->bEnemyCanPickup : Flag->bFriendlyCanPickup);
			FVector2D Pos = WorldToMapToScreen(Base->GetActorLocation());
			if (!Flag->IsHome() || Flag->bEnemyCanPickup || Flag->bFriendlyCanPickup)
			{
				Canvas->DrawColor = (TeamIndex == 0) ? FColor(255, 0, 0, 255) : FColor(0, 0, 255, 255);
				Canvas->DrawTile(SelectedPlayerTexture, Pos.X - 12.0f * RenderScale, Pos.Y - 12.0f * RenderScale, 24.0f * RenderScale, 24.0f * RenderScale, 0.0f, 0.0f, SelectedPlayerTexture->GetSurfaceWidth(), SelectedPlayerTexture->GetSurfaceHeight());
			}
			if (Flag->Team && (bShowAllFlags || bCanPickupFlag || Flag->IsHome() || Flag->bCurrentlyPinged))
			{
				Pos = WorldToMapToScreen(Base->MyFlag->GetActorLocation());
				if (Flag->IsHome() && !Flag->bEnemyCanPickup && !Flag->bFriendlyCanPickup)
				{
					DrawMinimapIcon(HUDAtlas, Pos, FVector2D(36.f, 36.f), TeamIconUV[(Flag->Team->TeamIndex==0) ? 0 : 1], FVector2D(72.f, 72.f), Base->MyFlag->Team->TeamColor, true);
				}
				else
				{
					float TimeD = 2.f*GetWorld()->GetTimeSeconds();
					int32 TimeI = int32(TimeD);
					float Scale = Base->MyFlag->IsHome() ? 24.f : ((TimeI % 2 == 0) ? 24.f + 12.f*(TimeD - TimeI) : 36.f - 12.f*(TimeD - TimeI));
					DrawMinimapIcon(HUDAtlas, Pos, FVector2D(Scale, Scale), FVector2D(843.f, 87.f), FVector2D(43.f, 41.f), Base->MyFlag->Team->TeamColor, true);
				}
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
				if (EnemyBase)
				{
					FVector2D BasePos(WorldToMapToScreen(EnemyBase->GetActorLocation()));
					if ((EnemyBase != HomeBase) && (BasePos.Y > HomeBasePos.Y))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

