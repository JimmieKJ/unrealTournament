// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTFlagRunTommyHUD.h"
#include "UTCTFGameState.h"
#include "UTCTFGameMode.h"
#include "UTCTFScoreboard.h"

AUTFlagRunTommyHUD::AUTFlagRunTommyHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void AUTFlagRunTommyHUD::DrawHUD()
{
	Super::DrawHUD();

	if (GetWorld())
	{
		AUTCTFGameState* GS = GetWorld()->GetGameState<AUTCTFGameState>();
		if (GS)
		{
			int RedKills = 0;
			int BlueKills = 0;

			for (APlayerState* PS : GS->PlayerArray)
			{
				AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
				if (UTPS && UTPS->Team)
				{
					if (UTPS->Team->TeamIndex == 0)
					{
						RedKills += UTPS->RoundKills;
					}
					else
					{
						BlueKills += UTPS->RoundKills;
					}
				}
			}

			RedKills = RedKills % 10;
			BlueKills = BlueKills % 10;

			const int RedKillsRemaining = 10 - RedKills;
			const int BlueKillsRemaining = 10 - BlueKills;

			FFontRenderInfo FontInfo;
			FontInfo.bClipText = true;
			FontInfo.bEnableShadow = true;


			float CharHeight = 1.f;
			float CharWidth = 1.f;
			MediumFont->GetCharSize('0', CharWidth, CharHeight);
			
			float XAdjustRed = 0.085f * Canvas->ClipX;
			float XAdjustBlue = 0.015f * Canvas->ClipX;
			
			float XOffsetRed = 0.5f * Canvas->ClipX - XAdjustRed - (CharWidth * 20);
			float YOffsetRed = 0.17f * Canvas->ClipY;

			float XOffsetBlue = 0.5f * Canvas->ClipX + XAdjustBlue + (CharWidth * 21);
			float YOffsetBlue = 0.17f * Canvas->ClipY;

			Canvas->DrawText(MediumFont, FString("Red Kills Needed: ") + FString::FromInt(RedKillsRemaining), XOffsetRed, YOffsetRed, 1.f, 1.f, FontInfo);
			Canvas->DrawText(MediumFont, FString("Blue Kills Needed: ") + FString::FromInt(BlueKillsRemaining), XOffsetBlue, YOffsetBlue, 1.f, 1.f, FontInfo);

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