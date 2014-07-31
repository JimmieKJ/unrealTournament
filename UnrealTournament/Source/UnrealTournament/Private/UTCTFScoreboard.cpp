// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFScoreboard.h"
#include "UTTeamScoreboard.h"

UUTCTFScoreboard::UUTCTFScoreboard(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
}

void UUTCTFScoreboard::DrawScoreboard(float RenderDelta)
{
	CellHeight = 48;
	Super::DrawScoreboard(RenderDelta);
}

void UUTCTFScoreboard::DrawPlayers(float RenderDelta, float X, float Y, float ClipX, float ClipY, int32 TeamFilter)
{
	float NameX = X + ClipX * 0.05;
	float ScoreX = X + ClipX * 0.95;
	float DrawY = Y;

	float TextScale = CellScale * ResScale;


	FLinearColor BackgroundColors[2];
	switch (TeamFilter)
	{
		case 0 :	
			BackgroundColors[0] = FLinearColor(1.0,0.0,0.0,0.25);
			BackgroundColors[1] = FLinearColor(1.0,0.0,0.0,0.35);
			break;

		default:	
			BackgroundColors[0] = FLinearColor(0.0,0.0,1.0,0.25);
			BackgroundColors[1] = FLinearColor(0.0,0.0,1.0,0.35);
			break;
	}

	int step=0;
	for (int i=0;i<UTGameState->PlayerArray.Num();i++)
	{

		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS)
		{
			if (TeamFilter < 0 || (PS->Team != NULL && PS->Team->GetTeamNum() == TeamFilter))
			{
				Canvas->SetLinearDrawColor(BackgroundColors[step]);
				Canvas->DrawTile(Canvas->DefaultTexture, NameX, DrawY, ScoreX - NameX, CellHeight * TextScale,0,0,1,1);
				step = 1 - step;

				FLinearColor DrawColor =  (PS == UTHUDOwner->UTPlayerOwner->UTPlayerState ? FLinearColor::Yellow : FLinearColor :: White);
				UTHUDOwner->TempDrawString(FText::FromString( PS->PlayerName), NameX, DrawY, ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, DrawColor,TextScale,true);
				UTHUDOwner->TempDrawNumber(PS->Score, ScoreX, DrawY,DrawColor, 0.0f, TextScale, 0, true);

				DrawY += 26 * TextScale;

				FLinearColor DarkColor = DrawColor;
				DarkColor *= 0.5;
				DarkColor.A = 1.0;

				FString Stats = FString::Printf(TEXT("c: %i  r: %i  a: %i  k: %i  d: %i"), PS->FlagCaptures, PS->FlagReturns, PS->Assists, PS->Kills, PS->Deaths);
				UTHUDOwner->TempDrawString(FText::FromString(Stats), NameX, DrawY, ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, DarkColor,TextScale * 0.5,true);
				DrawY += 22 * TextScale;
			}
		}
	}
}
