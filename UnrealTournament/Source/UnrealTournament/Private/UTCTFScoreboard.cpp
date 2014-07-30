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
	float XL,YL;
	UTHUDOwner->SmallFont->GetCharSize(TEXT('Q'),XL,YL);
	CellHeight = YL * 1.5;
	Super::DrawScoreboard(RenderDelta);
}

void UUTCTFScoreboard::DrawPlayers(float RenderDelta, float X, float Y, float ClipX, float ClipY, int32 TeamFilter)
{
	float NameX = X + ClipX * 0.05;
	float ScoreX = X + ClipX * 0.95;
	float DrawY = Y;

	switch (TeamFilter)
	{
		case 0 : Canvas->SetDrawColor(255,0,0,32); break;
		case 1 : Canvas->SetDrawColor(0,0,255,32); break;
	}

	float TextScale = CellScale * ResScale;

	Canvas->DrawTile(Canvas->DefaultTexture, X,0, ClipX, Canvas->ClipY,0,0,1,1);

	for (int i=0;i<UTGameState->PlayerArray.Num();i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS)
		{
			if (TeamFilter < 0 || (PS->Team != NULL && PS->Team->GetTeamNum() == TeamFilter))
			{
				FLinearColor DrawColor =  (PS == UTHUDOwner->UTPlayerOwner->UTPlayerState ? FLinearColor::Yellow : FLinearColor :: White);
				UTHUDOwner->TempDrawString(FText::FromString( PS->PlayerName), NameX, DrawY, ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, DrawColor,TextScale,true);
				UTHUDOwner->TempDrawNumber(PS->Score, ScoreX, DrawY,DrawColor, 0.0f, TextScale, 0, true);

				DrawY += CellHeight * 0.75;

				FLinearColor DarkColor = DrawColor;
				DarkColor *= 0.5;
				DarkColor.A = 1.0;

				FString Stats = FString::Printf(TEXT("c: %i  r: %i  a: %i  k: %i  d: %i"), PS->FlagCaptures, PS->FlagReturns, PS->Assists, PS->Kills, PS->Deaths);
				UTHUDOwner->TempDrawString(FText::FromString(Stats), NameX, DrawY, ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, DarkColor,TextScale * 0.5,true);
				DrawY += CellHeight * 0.25;
			}
		}
	}
}
