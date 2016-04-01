// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTFlagRunHUD.h"
#include "UTCTFGameState.h"
#include "UTCTFGameMode.h"
#include "UTCTFScoreboard.h"

AUTFlagRunHUD::AUTFlagRunHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDrawMinimap = true;

	ConstructorHelpers::FObjectFinder<UTexture2D> PlayerStartTextureObject(TEXT("/Game/RestrictedAssets/UI/MiniMap/minimap_atlas.minimap_atlas"));
	PlayerStartIcon.U = 128;
	PlayerStartIcon.V = 192;
	PlayerStartIcon.UL = 64;
	PlayerStartIcon.VL = 64;
	PlayerStartIcon.Texture = PlayerStartTextureObject.Object;
}

void AUTFlagRunHUD::DrawHUD()
{
	Super::DrawHUD();

	AUTCTFGameState* GS = GetWorld()->GetGameState<AUTCTFGameState>();
	if (!bShowScores && GS && GS->GetMatchState() == MatchState::InProgress && GS->bAsymmetricVictoryConditions)
	{
		// draw pips for players alive on each team @TODO move to widget
		TArray<AUTPlayerState*> LivePlayers;
		int32 OldRedCount = RedPlayerCount;
		int32 OldBlueCount = BluePlayerCount;
		RedPlayerCount = 0;
		BluePlayerCount = 0;
		RedDeathCount = 0;
		BlueDeathCount = 0;
		for (APlayerState* PS : GS->PlayerArray)
		{
			AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
			if (UTPS != NULL && UTPS->Team != NULL && !UTPS->bOnlySpectator  && !UTPS->bIsInactive)
			{
				if (UTPS->bHasLifeLimit)
				{
					if (UTPS->Team->TeamIndex == 0)
					{
						RedPlayerCount++;
					}
					else
					{
						BluePlayerCount++;
					}
				}
				else
				{
					// @todo fixmesteve optimize - one pass through pawns, set the value of cachedcharacter, show those with none or dead
					AUTCharacter* Character = UTPS->GetUTCharacter();
					if (!Character || Character->IsDead())
					{
						if (UTPS->Team->TeamIndex == 0)
						{
							RedDeathCount++;
						}
						else
						{
							BlueDeathCount++;
						}
					}
				}
			}
		}
		if (OldRedCount > RedPlayerCount)
		{
			RedDeathTime = GetWorld()->GetTimeSeconds();
		}
		if (OldBlueCount > BluePlayerCount)
		{
			BlueDeathTime = GetWorld()->GetTimeSeconds();
		}

		float XAdjust = 0.015f * Canvas->ClipX * GetHUDWidgetScaleOverride();
		float PipSize = 0.02f * Canvas->ClipX * GetHUDWidgetScaleOverride();
		float XOffset = 0.5f * Canvas->ClipX - XAdjust - PipSize;
		float YOffset = 0.1f * Canvas->ClipY * GetHUDWidgetScaleOverride();

		Canvas->SetLinearDrawColor(FLinearColor::Red, 0.5f);
		for (int32 i = 0; i < RedPlayerCount; i++)
		{
			Canvas->DrawTile(PlayerStartIcon.Texture, XOffset, YOffset, PipSize, PipSize, PlayerStartIcon.U, PlayerStartIcon.V, PlayerStartIcon.UL, PlayerStartIcon.VL, BLEND_Translucent);
			XOffset -= 1.1f*PipSize;
		}
		float TimeSinceRedDeath = GetWorld()->GetTimeSeconds() - RedDeathTime;
		if (TimeSinceRedDeath < 0.5f)
		{
			Canvas->SetLinearDrawColor(FLinearColor::Red, 0.5f - TimeSinceRedDeath);
			float ScaledSize = 1.f + 2.f*TimeSinceRedDeath;
			Canvas->DrawTile(PlayerStartIcon.Texture, XOffset - 0.5f*(ScaledSize - 1.f)*PipSize, YOffset - 0.5f*(ScaledSize - 1.f)*PipSize, PipSize, PipSize, PlayerStartIcon.U, PlayerStartIcon.V, PlayerStartIcon.UL, PlayerStartIcon.VL, BLEND_Translucent);
		}

		XOffset = 0.5f * Canvas->ClipX + XAdjust;
		Canvas->SetLinearDrawColor(FLinearColor::Blue, 0.5f);
		for (int32 i = 0; i < BluePlayerCount; i++)
		{
			Canvas->DrawTile(PlayerStartIcon.Texture, XOffset, YOffset, PipSize, PipSize, PlayerStartIcon.U, PlayerStartIcon.V, PlayerStartIcon.UL, PlayerStartIcon.VL, BLEND_Translucent);
			XOffset += 1.1f*PipSize;
		}
		float TimeSinceBlueDeath = GetWorld()->GetTimeSeconds() - BlueDeathTime;
		if (TimeSinceBlueDeath < 0.5f)
		{
			Canvas->SetLinearDrawColor(FLinearColor::Blue, 0.5f - TimeSinceBlueDeath);
			float ScaledSize = 1.f + 2.f*TimeSinceBlueDeath;
			Canvas->DrawTile(PlayerStartIcon.Texture, XOffset - 0.5f*(ScaledSize - 1.f)*PipSize, YOffset - 0.5f*(ScaledSize - 1.f)*PipSize, PipSize, PipSize, PlayerStartIcon.U, PlayerStartIcon.V, PlayerStartIcon.UL, PlayerStartIcon.VL, BLEND_Translucent);
		}

		// draw skull here
		PipSize *= 0.6f;
		Canvas->SetLinearDrawColor(FLinearColor::White, 0.5f);
		XOffset = 0.5f * Canvas->ClipX - XAdjust - PipSize;
		for (int32 i = 0; i < RedDeathCount; i++)
		{
			Canvas->DrawTile(HUDAtlas, XOffset, YOffset, PipSize, PipSize, 725, 0, 28, 36, BLEND_Translucent);
			XOffset -= 1.8f*PipSize;
		}

		XOffset = 0.5f * Canvas->ClipX + XAdjust;
		for (int32 i = 0; i < BlueDeathCount; i++)
		{
			Canvas->DrawTile(HUDAtlas, XOffset, YOffset, PipSize, PipSize, 725, 0, 28, 36, BLEND_Translucent);
			XOffset += 1.8f*PipSize;
		}
	}
}