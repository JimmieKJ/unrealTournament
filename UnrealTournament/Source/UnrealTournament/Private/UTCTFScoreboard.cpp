// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFScoreboard.h"
#include "UTTeamScoreboard.h"
#include "UTCTFGameState.h"
#include "UTCTFScoring.h"
#include "StatNames.h"

UUTCTFScoreboard::UUTCTFScoreboard(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ScoringPlaysHeader = NSLOCTEXT("CTF", "ScoringPlaysHeader", "SCORING PLAYS");
	AssistedByText = NSLOCTEXT("CTF", "AssistedBy", "Assisted by");
	UnassistedText = NSLOCTEXT("CTF", "Unassisted", "Unassisted");
	NoScoringText = NSLOCTEXT("CTF", "NoScoring", "No Scoring");
	PeriodText[0] = NSLOCTEXT("UTScoreboard", "FirstHalf", "First Half");
	PeriodText[1] = NSLOCTEXT("UTScoreboard", "SecondHalf", "Second Half");
	PeriodText[2] = NSLOCTEXT("UTScoreboard", "Overtime", "Overtime");

	ColumnHeaderScoreX = 0.65;
	ColumnHeaderCapsX = 0.73;
	ColumnHeaderAssistsX = 0.79;
	ColumnHeaderReturnsX = 0.85;
	ReadyX = 0.7f;
	NumPages = 3;

	static ConstructorHelpers::FObjectFinder<USoundBase> OtherSpreeSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/UI/A_UI_EnemySpree01.A_UI_EnemySpree01'"));
	ScoreUpdateSound = OtherSpreeSoundFinder.Object;
}

void UUTCTFScoreboard::SetScoringPlaysTimer(bool bEnableTimer)
{
	if (UTHUDOwner)
	{
		if (bEnableTimer)
		{
			UTHUDOwner->GetWorld()->GetTimerManager().SetTimer(OpenScoringPlaysHandle, this, &UUTCTFScoreboard::OpenScoringPlaysPage, 6.0f, false);
		}
		else
		{
			UTHUDOwner->GetWorld()->GetTimerManager().ClearTimer(OpenScoringPlaysHandle);
		}
	}
}

void UUTCTFScoreboard::OpenScoringPlaysPage()
{
	if (UTHUDOwner)
	{
		if (UTHUDOwner->ScoreboardPage == 0)
		{
			SetPage(1);
			AUTCTFGameState* CTFState = Cast<AUTCTFGameState>(UTGameState);
			float DelayTime = FMath::Max(6.f, 1.f + CTFState->GetScoringPlays().Num());
			GetWorld()->GetTimerManager().SetTimer(OpenScoringPlaysHandle, this, &UUTCTFScoreboard::OpenScoringPlaysPage, DelayTime, false);
		}
		else if (UTHUDOwner->ScoreboardPage == 1)
		{
			SetPage(2);
		}
	}
}

void UUTCTFScoreboard::PageChanged_Implementation()
{
	GetWorld()->GetTimerManager().ClearTimer(OpenScoringPlaysHandle);
	TimeLineOffset = (UTGameState && (UTGameState->IsMatchAtHalftime() || UTGameState->HasMatchEnded())) ? -0.15f : 99999.f;
}

void UUTCTFScoreboard::DrawGameOptions(float RenderDelta, float& YOffset)
{
	if (UTGameState)
	{
		if (UTGameState->GoalScore > 0)
		{
			// Draw Game Text
			FText Score = FText::Format(NSLOCTEXT("UTScoreboard", "GoalScoreFormat", "First to {0} Caps"), FText::AsNumber(UTGameState->GoalScore));
			DrawText(Score, Size.X * 0.7f, YOffset + 40.f, UTHUDOwner->SmallFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		}

		FText StatusText = UTGameState->GetGameStatusText();
		if (!StatusText.IsEmpty())
		{
			DrawText(StatusText, Size.X * 0.985f, YOffset + 16.f, UTHUDOwner->SmallFont, 1.0, 1.0, FLinearColor::Yellow, ETextHorzPos::Right, ETextVertPos::Center);
		} 
		DrawText(UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), UTGameState->RemainingTime, true, true, true), Size.X * 0.985f, YOffset + 50.f, UTHUDOwner->NumberFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);
	}
}

void UUTCTFScoreboard::DrawScoreHeaders(float RenderDelta, float& YOffset)
{
	float XOffset = 0.f;
	float Width = (Size.X * 0.5f) - CenterBuffer;
	float Height = 23;

	FText CH_PlayerName = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerName", "Player");
	FText CH_Score = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerScore", "Score");
	FText CH_Caps = NSLOCTEXT("UTCTFScoreboard", "ColumnHeader_PlayerCaps", "C");
	FText CH_Assists = NSLOCTEXT("UTCTFScoreboard", "ColumnHeader_PlayerAssists", "A");
	FText CH_Returns = NSLOCTEXT("UTCTFScoreboard", "ColumnHeader_PlayerReturns", "R");
	FText CH_Ping = (GetWorld()->GetNetMode() == NM_Standalone) ? NSLOCTEXT("UTScoreboard", "ColumnHeader_BotSkill", "Skill") : NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerPing", "Ping");
	FText CH_Ready = NSLOCTEXT("UTScoreboard", "ColumnHeader_Ready", "");

	for (int32 i = 0; i < 2; i++)
	{
		// Draw the background Border
		DrawTexture(TextureAtlas, XOffset, YOffset, Width, Height, 149, 138, 32, 32, 1.0, FLinearColor(0.72f, 0.72f, 0.72f, 0.85f));

		DrawText(CH_PlayerName, XOffset + (Width * ColumnHeaderPlayerX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Left, ETextVertPos::Center);
		if (UTGameState && UTGameState->HasMatchStarted())
		{
			DrawText(CH_Score, XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			DrawText(CH_Caps, XOffset + (Width * ColumnHeaderCapsX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			DrawText(CH_Assists, XOffset + (Width * ColumnHeaderAssistsX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			DrawText(CH_Returns, XOffset + (Width * ColumnHeaderReturnsX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		}
		else
		{
			DrawText(CH_Ready, XOffset + (Width * ReadyX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		}

		DrawText(CH_Ping, XOffset + (Width * ColumnHeaderPingX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);

		XOffset = Size.X - Width;
	}

	YOffset += Height + 4;
}

void UUTCTFScoreboard::DrawPlayerScore(AUTPlayerState* PlayerState, float XOffset, float YOffset, float Width, FLinearColor DrawColor)
{
	DrawText(FText::AsNumber(int32(PlayerState->Score)), XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	DrawText(FText::AsNumber(PlayerState->FlagCaptures), XOffset + (Width * ColumnHeaderCapsX), YOffset + ColumnY, UTHUDOwner->SmallFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	DrawText(FText::AsNumber(PlayerState->Assists), XOffset + (Width * ColumnHeaderAssistsX), YOffset + ColumnY, UTHUDOwner->SmallFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	DrawText(FText::AsNumber(PlayerState->FlagReturns), XOffset + (Width * ColumnHeaderReturnsX), YOffset + ColumnY, UTHUDOwner->SmallFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
}

void UUTCTFScoreboard::DrawStatsLeft(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom)
{
	if (UTHUDOwner->ScoreboardPage == 1)
	{
		DrawScoringPlays(DeltaTime, YPos, XOffset, ScoreWidth, PageBottom);
	}
	else
	{
		DrawTeamScoreBreakdown(DeltaTime, YPos, XOffset, ScoreWidth, PageBottom);
	}
}

void UUTCTFScoreboard::DrawStatsRight(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom)
{
	if (UTHUDOwner->ScoreboardPage == 1)
	{
		DrawTeamScoreBreakdown(DeltaTime, YPos, XOffset, ScoreWidth, PageBottom);
	}
	else
	{
		DrawScoreBreakdown(DeltaTime, YPos, XOffset, ScoreWidth, PageBottom);
	}
}

void UUTCTFScoreboard::DrawScoringPlays(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight)
{
	AUTCTFGameState* CTFState = Cast<AUTCTFGameState>(UTGameState);
	int32 CurrentPeriod = -1;

	Canvas->SetLinearDrawColor(FLinearColor::White);
	FFontRenderInfo TextRenderInfo;
	TextRenderInfo.bEnableShadow = true;
	TextRenderInfo.bClipText = true;

	float XL, SmallYL;
	Canvas->TextSize(UTHUDOwner->SmallFont, "TEST", XL, SmallYL, RenderScale, RenderScale);
	float MedYL;
	Canvas->TextSize(UTHUDOwner->MediumFont, ScoringPlaysHeader.ToString(), XL, MedYL, RenderScale, RenderScale);

	float ScoreHeight = MedYL + SmallYL;
	float ScoringOffsetX, ScoringOffsetY;
	Canvas->TextSize(UTHUDOwner->MediumFont, "99 - 99", ScoringOffsetX, ScoringOffsetY, RenderScale, RenderScale);
	int32 TotalPlays = CTFState->GetScoringPlays().Num();

	Canvas->DrawText(UTHUDOwner->MediumFont, ScoringPlaysHeader, XOffset + (ScoreWidth - XL) * 0.5f, YPos, RenderScale, RenderScale, TextRenderInfo);
	YPos += 1.f * MedYL;
	if (CTFState->GetScoringPlays().Num() == 0)
	{
		float YL;
		Canvas->TextSize(UTHUDOwner->MediumFont, NoScoringText.ToString(), XL, YL, RenderScale, RenderScale);
		YPos += 0.2f * MedYL;
		DrawText(NoScoringText, XOffset + (ScoreWidth - XL) * 0.5f, YPos, UTHUDOwner->MediumFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, RenderScale, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top, TextRenderInfo);
	}

	float OldTimeLineOffset = TimeLineOffset;
	TimeLineOffset += 2.f*DeltaTime;
	float TimeFloor = FMath::FloorToInt(TimeLineOffset);
	if (UTPlayerOwner && (TimeFloor + 1 <= TotalPlays) && (TimeFloor != FMath::FloorToInt(OldTimeLineOffset)))
	{
		UTPlayerOwner->ClientPlaySound(ScoreUpdateSound);
	}
	int32 NumPlays = FMath::Min(TotalPlays, int32(TimeFloor) + 1);
	int32 SmallPlays = FMath::Clamp(2 * (NumPlays - 7), 0, NumPlays - 1);
	int32 SkippedPlays = FMath::Max(SmallPlays - 10, 0);
	int32 DrawnPlays = 0;
	float PctOffset = 1.f + TimeFloor - TimeLineOffset;
	for (const FCTFScoringPlay& Play : CTFState->GetScoringPlays())
	{
		DrawnPlays++;
		if (DrawnPlays > NumPlays)
		{
			break;
		}
		if (Play.Team != NULL)
		{
			if (Play.Period > CurrentPeriod)
			{
				CurrentPeriod++;
				if (CurrentPeriod < 3)
				{
					YPos += 0.3f*SmallYL;
					DrawText(PeriodText[CurrentPeriod], XOffset + 0.2f*ScoreWidth, YPos, UTHUDOwner->SmallFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, RenderScale, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center, TextRenderInfo);
					YPos += 0.7f * SmallYL;
				}
			}
			if (SkippedPlays > 0)
			{
				SkippedPlays--;
				continue;
			}
			float BoxYPos = YPos;
			bool bIsSmallPlay = (SmallPlays > 0);
			if (bIsSmallPlay)
			{
				SmallPlays--;
			}
			// draw background
			FLinearColor DrawColor = FLinearColor::White;
			float CurrentScoreHeight = bIsSmallPlay ? 0.5f*ScoreHeight : ScoreHeight;
			float IconHeight = 0.8f*CurrentScoreHeight;
			float IconOffset = bIsSmallPlay ? 0.12f*ScoreWidth : 0.1f*ScoreWidth;
			float BackAlpha = ((DrawnPlays == NumPlays) && (NumPlays == TimeFloor + 1)) ? FMath::Max(0.5f, PctOffset) : 0.5f;
			DrawTexture(TextureAtlas, XOffset, YPos, ScoreWidth, CurrentScoreHeight, 149, 138, 32, 32, BackAlpha, DrawColor);

			// draw scoring team icon
			int32 IconIndex = Play.Team->TeamIndex;
			IconIndex = FMath::Min(IconIndex, 1);
			DrawTexture(UTHUDOwner->HUDAtlas, XOffset + IconOffset, YPos + 0.1f*CurrentScoreHeight, IconHeight, IconHeight, UTHUDOwner->TeamIconUV[IconIndex].X, UTHUDOwner->TeamIconUV[IconIndex].Y, 72, 72, 1.f, Play.Team->TeamColor);

			FString ScoredByLine = Play.ScoredBy.GetPlayerName();
			if (Play.ScoredByCaps > 1)
			{
				ScoredByLine += FString::Printf(TEXT(" (%i)"), Play.ScoredByCaps);
			}

			// time of game
			FString TimeStampLine = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), Play.RemainingTime, false, true, false).ToString();
			Canvas->SetLinearDrawColor(FLinearColor::White);
			Canvas->DrawText(UTHUDOwner->SmallFont, TimeStampLine, XOffset + 0.04f*ScoreWidth, YPos + 0.5f*CurrentScoreHeight - 0.5f*SmallYL, RenderScale, RenderScale, TextRenderInfo);

			// scored by
			Canvas->SetLinearDrawColor(Play.Team->TeamColor);
			float ScorerNameYOffset = bIsSmallPlay ? BoxYPos + 0.5f*CurrentScoreHeight - 0.6f*MedYL : YPos;
			Canvas->DrawText(UTHUDOwner->MediumFont, ScoredByLine, XOffset + 0.2f*ScoreWidth, ScorerNameYOffset, RenderScale, RenderScale, TextRenderInfo);
			YPos += MedYL;

			if (!bIsSmallPlay)
			{
				// assists
				FString AssistLine;
				if (Play.Assists.Num() > 0)
				{
					AssistLine = AssistedByText.ToString() + TEXT(" ");
					for (const FCTFAssist& Assist : Play.Assists)
					{
						AssistLine += Assist.AssistName.GetPlayerName() + TEXT(", ");
					}
					AssistLine = AssistLine.LeftChop(2);
				}
				else
				{
					AssistLine = UnassistedText.ToString();
				}
				Canvas->SetLinearDrawColor(FLinearColor::White);
				Canvas->DrawText(UTHUDOwner->SmallFont, AssistLine, XOffset + 0.2f*ScoreWidth, YPos, RenderScale, RenderScale, TextRenderInfo);
			}

			float SingleXL, SingleYL;
			YPos = BoxYPos + 0.5f*CurrentScoreHeight - 0.6f*ScoringOffsetY;
			float ScoreX = XOffset + 0.95f*ScoreWidth - ScoringOffsetX;
			Canvas->SetLinearDrawColor(CTFState->Teams[0]->TeamColor);
			FString SingleScorePart = FString::Printf(TEXT(" %i"), Play.TeamScores[0]);
			Canvas->TextSize(UTHUDOwner->MediumFont, SingleScorePart, SingleXL, SingleYL, RenderScale, RenderScale);
			Canvas->DrawText(UTHUDOwner->MediumFont, SingleScorePart, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
			Canvas->SetLinearDrawColor(FLinearColor::White);
			ScoreX += SingleXL;
			Canvas->TextSize(UTHUDOwner->MediumFont, "-", SingleXL, SingleYL, RenderScale, RenderScale);
			ScoreX += SingleXL;
			Canvas->DrawText(UTHUDOwner->MediumFont, "-", ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
			Canvas->SetLinearDrawColor(CTFState->Teams[1]->TeamColor);
			ScoreX += SingleXL;
			Canvas->DrawText(UTHUDOwner->MediumFont, FString::Printf(TEXT(" %i"), Play.TeamScores[1]), ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
			YPos = BoxYPos + CurrentScoreHeight + 8.f*RenderScale;
		}
	}
}

void UUTCTFScoreboard::DrawPlayerStats(AUTPlayerState* PS, float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight, const FStatsFontInfo& StatsFontInfo)
{
	Canvas->DrawText(UTHUDOwner->TinyFont, TEXT("Amount"), XOffset + (ValueColumn - 0.025f)*ScoreWidth, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	Canvas->DrawText(UTHUDOwner->TinyFont, TEXT("Scoring"), XOffset + (ScoreColumn - 0.025f)*ScoreWidth, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	YPos += 0.5f * StatsFontInfo.TextHeight;
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "Kills", "Kills"), PS->Kills, -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	int32 FCKills = PS->GetStatsValue(NAME_FCKills);
	int32 FlagSupportKills = PS->GetStatsValue(NAME_FlagSupportKills);
	int32 RegularKills = PS->Kills - FCKills - FlagSupportKills;
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "RegKills", " - Regular Kills"), RegularKills, PS->GetStatsValue(NAME_RegularKillPoints), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "FlagSupportKills", " - FC Support Kills"), FlagSupportKills, PS->GetStatsValue(NAME_FlagSupportKillPoints), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "EnemyFCKills", " - Enemy FC Kills"), FCKills, PS->GetStatsValue(NAME_FCKillPoints), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "EnemyFCDamage", " Enemy FC Damage Bonus"), -1, PS->GetStatsValue(NAME_EnemyFCDamage), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "Deaths", "Deaths"), PS->Deaths, -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "ScorePM", "Score Per Minute"), FString::Printf(TEXT(" %6.2f"), ((PS->StartTime <  GetWorld()->GameState->ElapsedTime) ? PS->Score * 60.f / (GetWorld()->GameState->ElapsedTime - PS->StartTime) : 0.f)), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "KDRatio", "K/D Ratio"), FString::Printf(TEXT(" %6.2f"), ((PS->Deaths > 0) ? float(PS->Kills) / PS->Deaths : 0.f)), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	Canvas->DrawText(StatsFontInfo.TextFont, "----------------------------------------------------------------", XOffset, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	YPos += 0.5f * StatsFontInfo.TextHeight;

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "FlagGrabs", "Flag Grabs"), PS->GetStatsValue(NAME_FlagGrabs), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	FText ClockString = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), PS->GetStatsValue(NAME_FlagHeldTime), false);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "FlagHeldTime", "Flag Held Time"), ClockString.ToString(), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth , 0);
	ClockString = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), PS->GetStatsValue(NAME_FlagHeldDenyTime), false);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "FlagDenialTime", "Flag Denial Time"), ClockString.ToString(), FString::Printf(TEXT(" %i"), int32(PS->GetStatsValue(NAME_FlagHeldDeny))), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth , 0);

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "FlagCaps", "Flag Captures"), PS->FlagCaptures, PS->GetStatsValue(NAME_FlagCapPoints), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "FlagReturns", "Flag Returns"), PS->FlagReturns, PS->GetStatsValue(NAME_FlagReturnPoints), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "FlagAssists", "Assists"), PS->Assists, -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "CarryAssists", " - Carry Assists"), PS->GetStatsValue(NAME_CarryAssist), PS->GetStatsValue(NAME_CarryAssistPoints), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "ReturnAssists", " - Return Assists"), PS->GetStatsValue(NAME_ReturnAssist), PS->GetStatsValue(NAME_ReturnAssistPoints), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "DefendAssists", " - Support Assists"), PS->GetStatsValue(NAME_DefendAssist), PS->GetStatsValue(NAME_DefendAssistPoints), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	int32 TeamCaps = PS->Team ? PS->Team->Score - PS->FlagCaptures : 0;
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "TeamCaps", " - Team Cap Bonus"), TeamCaps, PS->GetStatsValue(NAME_TeamCapPoints), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );

	Canvas->DrawText(UTHUDOwner->TinyFont, TEXT("-----"), XOffset + (ScoreColumn - 0.015f)*ScoreWidth, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	YPos += 0.5f*StatsFontInfo.TextHeight;
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "Scoring", "TOTAL SCORE"), -1, PS->Score, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
}

void UUTCTFScoreboard::DrawTeamStats(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight, const FStatsFontInfo& StatsFontInfo)
{
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "FlagCaps", "Flag Captures"), UTGameState->Teams[0]->Score, UTGameState->Teams[1]->Score, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "TeamGrabs", "Flag Grabs"), UTGameState->Teams[0]->GetStatsValue(NAME_TeamFlagGrabs), UTGameState->Teams[1]->GetStatsValue(NAME_TeamFlagGrabs), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	DrawClockTeamStatsLine(NSLOCTEXT("UTScoreboard", "TeamFlagHeldTime", "Flag Held Time"), NAME_TeamFlagHeldTime, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth , false);

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "TeamKills", "Kills"), UTGameState->Teams[0]->GetStatsValue(NAME_TeamKills), UTGameState->Teams[1]->GetStatsValue(NAME_TeamKills), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );

	// find top scorer
	AUTPlayerState* TopScorerRed = FindTopTeamScoreFor(0);
	AUTPlayerState* TopScorerBlue = FindTopTeamScoreFor(1);

	// find top kills && KD
	AUTPlayerState* TopKillerRed = FindTopTeamKillerFor(0);
	AUTPlayerState* TopKillerBlue = FindTopTeamKillerFor(1);
	AUTPlayerState* TopKDRed = FindTopTeamKDFor(0);
	AUTPlayerState* TopKDBlue = FindTopTeamKDFor(1);
	AUTPlayerState* TopSPMRed = FindTopTeamSPMFor(0);
	AUTPlayerState* TopSPMBlue = FindTopTeamSPMFor(1);

	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "TopScorer", "Top Scorer"), GetPlayerNameFor(TopScorerRed), GetPlayerNameFor(TopScorerBlue), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth , 0);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "TopFlagRunner", "Top Flag Runner"), GetPlayerNameFor(UTGameState->Teams[0]->TopAttacker), GetPlayerNameFor(UTGameState->Teams[1]->TopAttacker), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth , 0);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "TopDefender", "Top Defender"), GetPlayerNameFor(UTGameState->Teams[0]->TopDefender), GetPlayerNameFor(UTGameState->Teams[1]->TopDefender), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth , 0);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "TopSupport", "Top Support"), GetPlayerNameFor(UTGameState->Teams[0]->TopSupporter), GetPlayerNameFor(UTGameState->Teams[1]->TopSupporter), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth , 0);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "TopKills", "Top Kills"), GetPlayerNameFor(TopKillerRed), GetPlayerNameFor(TopKillerBlue), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth , 0);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "TopKD", "Top K/D"), GetPlayerNameFor(TopKDRed), GetPlayerNameFor(TopKDBlue), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth , 0);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "TopSPM", "Top Score Per Minute"), GetPlayerNameFor(TopSPMRed), GetPlayerNameFor(TopSPMBlue), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "BeltPickups", "Shield Belt Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ShieldBeltCount), UTGameState->Teams[1]->GetStatsValue(NAME_ShieldBeltCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "VestPickups", "Armor Vest Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ArmorVestCount), UTGameState->Teams[1]->GetStatsValue(NAME_ArmorVestCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "PadPickups", "Thigh Pad Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ArmorPadsCount), UTGameState->Teams[1]->GetStatsValue(NAME_ArmorPadsCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "HelmetPickups", "Helmet Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_HelmetCount), UTGameState->Teams[1]->GetStatsValue(NAME_HelmetCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );

	int32 TeamStat0 = UTGameState->Teams[0]->GetStatsValue(NAME_UDamageCount);
	int32 TeamStat1 = UTGameState->Teams[1]->GetStatsValue(NAME_UDamageCount);
	if (TeamStat0 > 0 || TeamStat1 > 0)
	{
		DrawStatsLine(NSLOCTEXT("UTScoreboard", "UDamagePickups", "UDamage Pickups"), TeamStat0, TeamStat1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	}
	TeamStat0 = UTGameState->Teams[0]->GetStatsValue(NAME_BerserkCount);
	TeamStat1 = UTGameState->Teams[1]->GetStatsValue(NAME_BerserkCount);
	if (TeamStat0 > 0 || TeamStat1 > 0)
	{
		DrawStatsLine(NSLOCTEXT("UTScoreboard", "BerserkPickups", "Berserk Pickups"), TeamStat0, TeamStat1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	}
	TeamStat0 = UTGameState->Teams[0]->GetStatsValue(NAME_InvisibilityCount);
	TeamStat1 = UTGameState->Teams[1]->GetStatsValue(NAME_InvisibilityCount);
	if (TeamStat0 > 0 || TeamStat1 > 0)
	{
		DrawStatsLine(NSLOCTEXT("UTScoreboard", "InvisibilityPickups", "Invisibility Pickups"), TeamStat0, TeamStat1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	}
	TeamStat0 = UTGameState->Teams[0]->GetStatsValue(NAME_KegCount);
	TeamStat1 = UTGameState->Teams[1]->GetStatsValue(NAME_KegCount);
	if (TeamStat0 > 0 || TeamStat1 > 0)
	{
		DrawStatsLine(NSLOCTEXT("UTScoreboard", "KegPickups", "Keg Pickups"), TeamStat0, TeamStat1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	}

	DrawClockTeamStatsLine(NSLOCTEXT("UTScoreboard", "UDamage", "UDamage Control"), NAME_UDamageTime, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth , true);
	DrawClockTeamStatsLine(NSLOCTEXT("UTScoreboard", "Berserk", "Berserk Control"), NAME_BerserkTime, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth , true);
	DrawClockTeamStatsLine(NSLOCTEXT("UTScoreboard", "Invisibility", "Invisibility Control"), NAME_InvisibilityTime, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth , true);

	int32 BootJumpsRed = UTGameState->Teams[0]->GetStatsValue(NAME_BootJumps);
	int32 BootJumpsBlue = UTGameState->Teams[1]->GetStatsValue(NAME_BootJumps);
	if ((BootJumpsRed != 0) || (BootJumpsBlue != 0))
	{
		DrawStatsLine(NSLOCTEXT("UTScoreboard", "JumpBootJumps", "JumpBoot Jumps"), BootJumpsRed, BootJumpsBlue, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	}

	// later do redeemer shots -and all these also to individual
	// track individual movement stats as well
	// @TODO FIXMESTEVE make all the loc text into properties instead of recalc
}



