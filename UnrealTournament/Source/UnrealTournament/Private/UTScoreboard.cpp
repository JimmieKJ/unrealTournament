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

	DrawHeader(RenderDelta, 0, 0, Canvas->ClipX, Canvas->ClipY);
	DrawPlayers(RenderDelta, 0, Canvas->ClipY * 0.15, Canvas->ClipX, Canvas->ClipY);
	DrawFooter(RenderDelta, 0, 0, Canvas->ClipX, Canvas->ClipY);
}

void UUTScoreboard::DrawHeader(float RenderDelta, float X, float Y, float ClipX, float ClipY)
{
	float DrawY = Y + ClipY * 0.11 - 35 * ResScale;
	float DrawX = X + ClipX * 0.15;

	Canvas->SetDrawColor(23,72,155,200);
	Canvas->DrawTile(Canvas->DefaultTexture, DrawX, DrawY - (6 * ResScale), ClipX * 0.7, 18 * ResScale,0,0,1,1);

	Canvas->DrawColor = FLinearColor::White;
	Canvas->DrawTile(UTHUDOwner->OldHudTexture, DrawX + 10 * ResScale, DrawY - 35 * ResScale, 82 * ResScale, 69 * ResScale, 734,190, 82,70);

	// Draw the Time...

	FText TimeElapsed = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), UTGameState->ElapsedTime);
	UTHUDOwner->DrawString( TimeElapsed, X + ClipX * 0.85, DrawY + (12 * ResScale), ETextHorzPos::Right, ETextVertPos::Bottom, UTHUDOwner->GetFontFromSizeIndex(1), FLinearColor::White);
}

void UUTScoreboard::DrawPlayers(float RenderDelta, float X, float Y, float ClipX, float ClipY, int32 TeamFilter)
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
				UTHUDOwner->DrawString(FText::FromString( PS->PlayerName), NameX, DrawY, ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, DrawColor, CellScale);
				UTHUDOwner->DrawNumber(PS->Score, ScoreX, DrawY,DrawColor, 0.0f, CellScale, 0, true);
				UTHUDOwner->DrawString(FText::FromString( FString::Printf(TEXT("Kills: %i"),PS->Kills)), ScoreX + 2, DrawY, ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, DrawColor, CellScale * 0.5f);
				UTHUDOwner->DrawString(FText::FromString( FString::Printf(TEXT("Deaths: %i"),PS->Deaths)), ScoreX + 2, DrawY + (CellHeight * 0.5), ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, DrawColor, CellScale * 0.5f);
				DrawY += CellHeight;
			}
		}
	}
}

void UUTScoreboard::DrawFooter(float RenderDelta, float X, float Y, float ClipX, float ClipY)
{
	float DrawY = Y + ClipY * 0.91 * ResScale;;
	float DrawX = X + ClipX * 0.15;
	Canvas->SetDrawColor(23,72,155,200);
	Canvas->DrawTile(Canvas->DefaultTexture, DrawX, DrawY, ClipX * 0.7,6 * ResScale,0,0,1,1);

	DrawY += 10 * ResScale;

	UTHUDOwner->DrawString(FText::FromString(UTHUDOwner->GetWorld()->GetMapName()), DrawX, DrawY, ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, FLinearColor::White);

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
		UTHUDOwner->DrawString( Msg, X + ClipX * 0.84, DrawY + (14 * ResScale), ETextHorzPos::Right, ETextVertPos::Bottom, UTHUDOwner->GetFontFromSizeIndex(1), FLinearColor::White);
	}

}

