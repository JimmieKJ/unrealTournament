// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFScoreboard.h"
#include "UTTeamScoreboard.h"
#include "UTCTFGameState.h"

UUTCTFScoreboard::UUTCTFScoreboard(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ScoringPlaysHeader = NSLOCTEXT("CTF", "ScoringPlaysHeader", "SCORING SUMMARY");
	AssistedByText = NSLOCTEXT("CTF", "AssistedBy", "Assists:");
	UnassistedText = NSLOCTEXT("CTF", "Unassisted", "Unassisted");
}

void UUTCTFScoreboard::DrawScoreboard(float RenderDelta)
{
	// temp hack: draw scoring plays instead of scoreboard for end of halftime
	AUTCTFGameState* CTFState = Cast<AUTCTFGameState>(UTGameState);
	if (CTFState != NULL && CTFState->IsMatchAtHalftime() && CTFState->RemainingTime > 0 && CTFState->RemainingTime <= CTFState->TimeLimit / 2)
	{
		ResScale = Canvas->ClipY / 720.0;
		DrawScoringPlays();
	}
	else
	{
		CellHeight = 48;
		Super::DrawScoreboard(RenderDelta);
	}
}

void UUTCTFScoreboard::DrawScoringPlays()
{
	AUTCTFGameState* CTFState = Cast<AUTCTFGameState>(UTGameState);
	uint8 CurrentPeriod = 0;
	if (CTFState->HasMatchEnded())
	{
		// show scores for last played period
		for (const FCTFScoringPlay& Play : CTFState->GetScoringPlays())
		{
			CurrentPeriod = FMath::Max<uint8>(CurrentPeriod, Play.Period);
		}
	}
	// TODO: currently there's no intermission between second half and OT
	else if (CTFState->IsMatchAtHalftime())
	{
		CurrentPeriod = 0;
	}
	float YPos = Canvas->ClipY * 0.1f;
	const float XOffset = Canvas->ClipX * ((Canvas->ClipX / Canvas->ClipY > 1.5f) ? 0.2f : 0.1f);
	FFontRenderInfo TextRenderInfo;
	TextRenderInfo.bEnableShadow = true;
	TextRenderInfo.bClipText = true;
	{
		float XL, YL;
		Canvas->TextSize(UTHUDOwner->LargeFont, ScoringPlaysHeader.ToString(), XL, YL, ResScale, ResScale);
		Canvas->DrawText(UTHUDOwner->LargeFont, ScoringPlaysHeader, (Canvas->ClipX - XL) * 0.5, YPos, ResScale, ResScale, TextRenderInfo);
		YPos += YL * 1.2f;
	}
	TArray<int32> ScoresSoFar;
	ScoresSoFar.SetNumZeroed(CTFState->Teams.Num());
	TMap<FSafePlayerName, int32> CapCount;
	for (const FCTFScoringPlay& Play : CTFState->GetScoringPlays())
	{
		if (Play.Team != NULL) // should always be true...
		{
			ScoresSoFar.SetNumZeroed(FMath::Max<int32>(ScoresSoFar.Num(), int32(Play.Team->TeamIndex) + 1));
			ScoresSoFar[Play.Team->TeamIndex]++;
			int32* PlayerCaps = CapCount.Find(Play.ScoredBy);
			if (PlayerCaps != NULL)
			{
				(*PlayerCaps)++;
			}
			else
			{
				CapCount.Add(Play.ScoredBy, 1);
			}
			if (Play.Period >= CurrentPeriod)
			{
				// draw this cap
				Canvas->SetLinearDrawColor(Play.Team->TeamColor);
				float XL, YL;
				// scored by
				FString ScoredByLine = Play.ScoredBy.GetPlayerName();
				if (PlayerCaps != NULL)
				{
					ScoredByLine += FString::Printf(TEXT(" (%i)"), *PlayerCaps);
				}
				float PrevYPos = YPos;
				Canvas->TextSize(UTHUDOwner->MediumFont, ScoredByLine, XL, YL, ResScale, ResScale);
				Canvas->DrawText(UTHUDOwner->MediumFont, FText::FromString(ScoredByLine), XOffset, YPos, ResScale, ResScale, TextRenderInfo);
				YPos += YL;
				// assists
				FString AssistLine;
				if (Play.Assists.Num() > 0)
				{
					AssistLine = AssistedByText.ToString() + TEXT(" ");
					for (const FSafePlayerName& Assist : Play.Assists)
					{
						AssistLine += Assist.GetPlayerName() + TEXT(", ");
					}
					AssistLine = AssistLine.LeftChop(2);
				}
				else
				{
					AssistLine = UnassistedText.ToString();
				}
				Canvas->TextSize(UTHUDOwner->MediumFont, AssistLine, XL, YL, ResScale * 0.6f, ResScale * 0.6f);
				Canvas->DrawText(UTHUDOwner->MediumFont, FText::FromString(AssistLine), XOffset, YPos, ResScale * 0.6f, ResScale * 0.6f, TextRenderInfo);
				YPos += YL;
				// time of game
				FString TimeStampLine = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), Play.ElapsedTime, false, true, false).ToString();
				Canvas->TextSize(UTHUDOwner->MediumFont, TimeStampLine, XL, YL, ResScale * 0.6f, ResScale * 0.6f);
				Canvas->DrawText(UTHUDOwner->MediumFont, FText::FromString(TimeStampLine), XOffset, YPos, ResScale * 0.6f, ResScale * 0.6f, TextRenderInfo);
				YPos += YL;
				// team score after this cap
				Canvas->SetLinearDrawColor(FLinearColor::White);
				FString CurrentScoreLine;
				for (int32 Score : ScoresSoFar)
				{
					CurrentScoreLine += FString::Printf(TEXT("%i    "), Score);
				}
				Canvas->TextSize(UTHUDOwner->MediumFont, CurrentScoreLine, XL, YL, ResScale, ResScale);
				Canvas->DrawText(UTHUDOwner->MediumFont, FText::FromString(CurrentScoreLine), Canvas->ClipX - XOffset - XL, PrevYPos + (YPos - PrevYPos - YL) * 0.5, ResScale, ResScale, TextRenderInfo);

				YPos += YL * 0.25f;
				if (YPos >= Canvas->ClipY)
				{
					// TODO: pagination
					break;
				}
			}
		}
	}
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
		if (PS && !PS->bOnlySpectator)
		{
			if (TeamFilter < 0 || (PS->Team != NULL && PS->Team->GetTeamNum() == TeamFilter))
			{
				Canvas->SetLinearDrawColor(BackgroundColors[step]);
				Canvas->DrawTile(Canvas->DefaultTexture, NameX, DrawY, ScoreX - NameX, CellHeight * TextScale,0,0,1,1);
				step = 1 - step;

				FLinearColor DrawColor =  (PS == UTHUDOwner->UTPlayerOwner->UTPlayerState ? FLinearColor::Yellow : FLinearColor :: White);
				UTHUDOwner->DrawString(FText::FromString( PS->PlayerName), NameX, DrawY, ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, DrawColor,TextScale,true);
				UTHUDOwner->DrawNumber(PS->Score, ScoreX, DrawY,DrawColor, 0.0f, TextScale, 0, true);

				DrawY += 26 * TextScale;

				int32 Ping = PS->Ping * 4;
				if (UTHUDOwner->UTPlayerOwner->UTPlayerState == PS)
				{
					Ping = PS->ExactPing;
				}

				FString Stats = FString::Printf(TEXT("c: %i  r: %i  a: %i  k: %i  d: %i ping: %i"), PS->FlagCaptures, PS->FlagReturns, PS->Assists, PS->Kills, PS->Deaths, Ping);
				UTHUDOwner->DrawString(FText::FromString(Stats), NameX, DrawY, ETextHorzPos::Left, ETextVertPos::Top, UTHUDOwner->MediumFont, DrawColor,TextScale * 0.5,true);
				DrawY += 22 * TextScale;
			}
		}
	}
}
