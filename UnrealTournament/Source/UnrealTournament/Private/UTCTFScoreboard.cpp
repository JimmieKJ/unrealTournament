// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFScoreboard.h"
#include "UTTeamScoreboard.h"

UUTCTFScoreboard::UUTCTFScoreboard(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
}

void UUTCTFScoreboard::DrawPlayers(float RenderDelta, float X, float Y, float ClipX, float ClipY, int32 TeamFilter)
{
	float NameX = X + ClipX * 0.15;
	float ScoreX = X + ClipX * 0.8;
	float DrawY = Y;

	for (int i=0;i<UTGameState->PlayerArray.Num();i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS)
		{
			if (TeamFilter < 0 || (PS->Team != NULL && PS->Team->GetTeamNum() == TeamFilter))
			{
				FLinearColor DrawColor =  (PS == UTHUDOwner->UTPlayerOwner->UTPlayerState ? FLinearColor::Yellow : FLinearColor :: White);
				UTHUDOwner->TempDrawString(FText::FromString( PS->PlayerName), NameX, DrawY, ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, DrawColor, CellScale);
				UTHUDOwner->TempDrawNumber(PS->Score, ScoreX, DrawY,DrawColor, 0.0f, CellScale, 0, true);
				UTHUDOwner->TempDrawString(FText::FromString( FString::Printf(TEXT("Caps: %i   Kills: %i"),PS->FlagCaptures,PS->Kills)), ScoreX + 2, DrawY, ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, DrawColor, CellScale * 0.5f);
				UTHUDOwner->TempDrawString(FText::FromString( FString::Printf(TEXT("Assists: %  Deaths: %i"),PS->Assists,PS->Deaths)), ScoreX + 2, DrawY + (CellHeight * 0.5), ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, DrawColor, CellScale * 0.5f);
				DrawY += CellHeight;
			}
		}
	}
}
