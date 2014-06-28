// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#include "UTTeamScoreboard.h"

UUTTeamScoreboard::UUTTeamScoreboard(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
}

void UUTTeamScoreboard::DrawScoreboard(float RenderDelta)
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


	DrawPlayers(RenderDelta, 0, Canvas->ClipY * 0.15, Canvas->ClipX * 0.5, Canvas->ClipY * 0.8,0);
	DrawPlayers(RenderDelta, Canvas->ClipX * 0.5, Canvas->ClipY * 0.15, Canvas->ClipX * 0.5, Canvas->ClipY * 0.8,1);

	DrawHeader(RenderDelta, 0, 0, Canvas->ClipX, Canvas->ClipY);
	DrawFooter(RenderDelta, 0, 0, Canvas->ClipX, Canvas->ClipY);
}

void UUTTeamScoreboard::DrawPlayers(float RenderDelta, float X, float Y, float ClipX, float ClipY, int32 TeamFilter)
{
	switch (TeamFilter)
	{
		case 0 : Canvas->SetDrawColor(255,0,0,32); break;
		case 1 : Canvas->SetDrawColor(0,0,255,32); break;
	}

	Canvas->DrawTile(Canvas->DefaultTexture, X,0, ClipX, Canvas->ClipY,0,0,1,1);
	Super::DrawPlayers(RenderDelta, X, Y, ClipX ,ClipY, TeamFilter);
}
