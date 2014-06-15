// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#include "UTScoreboard.h"

UUTScoreboard::UUTScoreboard(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
}

void UUTScoreboard::DrawScoreboard(float RenderDelta)
{
	// Give Blueprint a chance to draw it's scoreboard
	if (eventDraw(RenderDelta))
	{
		return;
	}


	// Calculate any scaling if needed by looking at the height of everything.  NOTE: we 

	float xl;
	Canvas->StrLen(UTHUDOwner->MediumFont, UTGameState->PlayerArray[0]->PlayerName, xl, CellHeight);

	if (CellHeight < 47) CellHeight = 47;  // 47 = the Height of the Big numbers used by TempDrawNumber.  This is throwaway code

	float ZoneYSize = Canvas->ClipY * 0.6;
	float ScoreYSize = CellHeight * UTGameState->PlayerArray.Num();

	CellScale = (ScoreYSize <= ZoneYSize ? 1.0 : ZoneYSize / ScoreYSize);
	CellHeight *= CellScale;
	ResScale = Canvas->ClipY / 720.0;

	DrawHeader(RenderDelta);
	DrawPlayers(RenderDelta);
	DrawFooter(RenderDelta);
}

void UUTScoreboard::DrawHeader(float RenderDelta)
{
	float Y = Canvas->ClipY * 0.19 - 35 * ResScale;;
	float X = Canvas->ClipX * 0.15;
	Canvas->SetDrawColor(23,72,155,200);
	Canvas->DrawTile(Canvas->DefaultTexture, X, Y - (6 * ResScale), Canvas->ClipX * 0.7,18 * ResScale,0,0,1,1);

	Canvas->DrawColor = FLinearColor::White;
	Canvas->DrawTile(UTHUDOwner->OldHudTexture, X + 10 * ResScale, Y - 35 * ResScale, 82 * ResScale, 69 * ResScale, 734,190, 82,70);

	// Draw the Time...

	FText TimeElapsed = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), UTGameState->ElapsedTime);
	UTHUDOwner->TempDrawString( TimeElapsed, Canvas->ClipX * 0.84, Y + (12 * ResScale), ETextHorzPos::Right, ETextVertPos::Bottom, UTHUDOwner->GetFontFromSizeIndex(1), FLinearColor::White);
}

void UUTScoreboard::DrawPlayers(float RenderDelta)
{
	float NameX = Canvas->ClipX * 0.18;
	float ScoreX = Canvas->ClipX * 0.8;
	float Y = Canvas->ClipY * 0.2;

	for (int i=0;i<UTGameState->PlayerArray.Num();i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS)
		{
			FLinearColor DrawColor =  (PS == UTHUDOwner->UTPlayerOwner->UTPlayerState ? FLinearColor::Yellow : FLinearColor :: White);
			UTHUDOwner->TempDrawString(FText::FromString( PS->PlayerName), NameX, Y, ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, DrawColor, CellScale);
			UTHUDOwner->TempDrawNumber(PS->Score, ScoreX, Y,DrawColor, 0.0f, CellScale, 0, true);
			UTHUDOwner->TempDrawString(FText::FromString( FString::Printf(TEXT("Kills: %i"),PS->Kills)), ScoreX + 2, Y, ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, DrawColor, CellScale * 0.5f);
			UTHUDOwner->TempDrawString(FText::FromString( FString::Printf(TEXT("Deaths: %i"),PS->Deaths)), ScoreX + 2, Y + (CellHeight * 0.5), ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, DrawColor, CellScale * 0.5f);
			Y += CellHeight;
		}
	}
}

void UUTScoreboard::DrawFooter(float RenderDelta)
{
	float Y = Canvas->ClipY * 0.81 * ResScale;;
	float X = Canvas->ClipX * 0.15;
	Canvas->SetDrawColor(23,72,155,200);
	Canvas->DrawTile(Canvas->DefaultTexture, X, Y, Canvas->ClipX * 0.7,6 * ResScale,0,0,1,1);

	Y += 10 * ResScale;

	UTHUDOwner->TempDrawString(FText::FromString(UTHUDOwner->GetWorld()->GetMapName()), X, Y, ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, FLinearColor::White);

	FText Msg = FText::GetEmpty();

	if (UTGameState->IsMatchInProgress())
	{
		if (UTGameState->IsMatchInOvertime())
		{
			Msg = NSLOCTEXT("UTScoreboard","OvertimeText","-OVERTIME-");
		}
		else if (UTGameState->TimeLimit > 0)
		{
			Msg = UTHUDOwner->ConvertTime(FText::GetEmpty(), NSLOCTEXT("UTScoreBoard","RemainingPrefix"," : Time Remaining"), UTGameState->RemainingTime);

		}
	}
	else if (!UTGameState->HasMatchStarted())
	{
		Msg = NSLOCTEXT("UTScoreboard","WaitingToBegin","Waiting For Match to Start!");
	}
	else if (!UTGameState->HasMatchEnded())
	{
		Msg = NSLOCTEXT("UTScoreboard","MatchOver","Match is Over!");
	}

	if (!Msg.IsEmpty())
	{
		UTHUDOwner->TempDrawString( Msg, Canvas->ClipX * 0.84, Y + (14 * ResScale), ETextHorzPos::Right, ETextVertPos::Bottom, UTHUDOwner->GetFontFromSizeIndex(1), FLinearColor::White);
	}

}

