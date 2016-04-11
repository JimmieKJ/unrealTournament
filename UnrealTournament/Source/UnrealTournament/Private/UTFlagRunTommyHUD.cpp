// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTFlagRunTommyHUD.h"
#include "UTCTFGameState.h"
#include "UTCTFGameMode.h"
#include "UTCTFScoreboard.h"
#include "UTCTFRoundGameState.h"

AUTFlagRunTommyHUD::AUTFlagRunTommyHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void AUTFlagRunTommyHUD::DrawHUD()
{
	Super::DrawHUD();

	if (GetWorld())
	{
		AUTCTFRoundGameState* GS = GetWorld()->GetGameState<AUTCTFRoundGameState>();
		if (GS && !bShowScores && GS->GetMatchState() == MatchState::InProgress)
		{
			int RedKills = 0;
			int BlueKills = 0;
			int RedKillsRemaining = 0;
			int BlueKillsRemaining = 0;

			if (GS->bRedToCap)
			{
				RedKills = GS->OffenseKills;
				BlueKills = GS->DefenseKills;

				RedKills = RedKills % GS->OffenseKillsNeededForPowerup;
				BlueKills = BlueKills % GS->DefenseKillsNeededForPowerup;

				RedKillsRemaining = GS->OffenseKillsNeededForPowerup - RedKills;
				BlueKillsRemaining = GS->DefenseKillsNeededForPowerup - BlueKills;
			}
			else
			{
				RedKills = GS->DefenseKills;
				BlueKills = GS->OffenseKills;

				RedKills = RedKills % GS->DefenseKillsNeededForPowerup;
				BlueKills = BlueKills % GS->OffenseKillsNeededForPowerup;

				RedKillsRemaining = GS->DefenseKillsNeededForPowerup - RedKills;
				BlueKillsRemaining = GS->OffenseKillsNeededForPowerup - BlueKills;
			}
						
			FFontRenderInfo FontInfo;
			FontInfo.bClipText = true;
			FontInfo.bEnableShadow = true;

			float CharHeight = 1.f;
			float CharWidth = 1.f;
			MediumFont->GetCharSize('0', CharWidth, CharHeight);
			
			float XAdjust = 0.35f * Canvas->ClipX;
			float BlueSentenceLengthOffset = 0.5f * CharWidth * GetHUDWidgetScaleOverride() * 21; //21 = the number of characters in "Blue Kills Needed: XX"


			float XOffsetRed = (0.5f * Canvas->ClipX) - XAdjust;
			float XOffsetBlue = (0.5f * Canvas->ClipX) + XAdjust - BlueSentenceLengthOffset;
			
			float YOffset = 0.17f * Canvas->ClipY * GetHUDWidgetScaleOverride();
						
			Canvas->DrawText(MediumFont, FString("Red Kills Needed: ") + FString::FromInt(RedKillsRemaining), XOffsetRed, YOffset, GetHUDWidgetScaleOverride(), GetHUDWidgetScaleOverride(), FontInfo);
			Canvas->DrawText(MediumFont, FString("Blue Kills Needed: ") + FString::FromInt(BlueKillsRemaining), XOffsetBlue, YOffset, GetHUDWidgetScaleOverride(), GetHUDWidgetScaleOverride(), FontInfo);

		}
	}
}

bool AUTFlagRunTommyHUD::IsTeamOnOffense(AUTPlayerState* PS) const
{
	if (GetWorld() && GetWorld()->GetGameState() && PS && PS->Team)
	{
		AUTCTFGameState* GS = GetWorld()->GetGameState<AUTCTFGameState>();
		if (GS)
		{
			const bool bIsOnRedTeam = (PS->Team->TeamIndex == 0);
			return (GS->bRedToCap == bIsOnRedTeam);
		}
	}

	return true;
}