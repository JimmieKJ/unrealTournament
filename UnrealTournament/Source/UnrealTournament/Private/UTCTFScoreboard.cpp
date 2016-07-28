// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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
	ScoringPlayScore = NSLOCTEXT("CTF", "ScoringPlayScore", "{RedScore} - {BlueScore}");

	ColumnHeaderScoreX = 0.63f;
	ColumnHeaderCapsX = 0.735;
	ColumnHeaderAssistsX = 0.7925;
	ColumnHeaderReturnsX = 0.85;
	ReadyX = 0.7f;

	static ConstructorHelpers::FObjectFinder<USoundBase> OtherSpreeSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/UI/A_UI_EnemySpree01.A_UI_EnemySpree01'"));
	ScoreUpdateSound = OtherSpreeSoundFinder.Object;

	CH_Caps = NSLOCTEXT("UTCTFScoreboard", "ColumnHeader_PlayerCaps", "C");
	CH_Assists = NSLOCTEXT("UTCTFScoreboard", "ColumnHeader_PlayerAssists", "A");
	CH_Returns = NSLOCTEXT("UTCTFScoreboard", "ColumnHeader_PlayerReturns", "R");
}

bool UUTCTFScoreboard::ShouldDrawScoringStats()
{
	return UTGameState && ((UTGameState->GetMatchState() == MatchState::MatchIntermission) || UTGameState->HasMatchEnded());
}

void UUTCTFScoreboard::DrawScoreHeaders(float RenderDelta, float& YOffset)
{
	float XOffset = ScaledEdgeSize;
	float Height = 23.f*RenderScale;

	for (int32 i = 0; i < 2; i++)
	{
		// Draw the background Border
		DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset, YOffset, ScaledCellWidth, Height, 149, 138, 32, 32, 1.0, FLinearColor(0.72f, 0.72f, 0.72f, 0.85f));
		DrawText(CH_PlayerName, XOffset + (ScaledCellWidth * ColumnHeaderPlayerX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Left, ETextVertPos::Center);
		if (UTGameState && UTGameState->HasMatchStarted())
		{
			DrawText(CH_Score, XOffset + (ScaledCellWidth * ColumnHeaderScoreX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			AUTCTFGameState* CTFState = Cast<AUTCTFGameState>(UTGameState);
			if (CTFState && (CTFState->bAttackerLivesLimited || CTFState->bDefenderLivesLimited))
			{
				DrawText(CH_Caps, XOffset + (ScaledCellWidth * ColumnHeaderCapsX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
				DrawText(NSLOCTEXT("UTScoreboard", "LivesRemaining", "Lives"), XOffset + (ScaledCellWidth * 0.5f*(ColumnHeaderAssistsX + ColumnHeaderReturnsX)), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			}
			else
			{
				DrawText(CH_Caps, XOffset + (ScaledCellWidth * ColumnHeaderCapsX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
				DrawText(CH_Assists, XOffset + (ScaledCellWidth * ColumnHeaderAssistsX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
				DrawText(CH_Returns, XOffset + (ScaledCellWidth * ColumnHeaderReturnsX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			}
		}
		DrawText((GetWorld()->GetNetMode() == NM_Standalone) ?  CH_Skill : CH_Ping, XOffset + (ScaledCellWidth * ColumnHeaderPingX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		XOffset = Canvas->ClipX - ScaledCellWidth - ScaledEdgeSize;
	}

	YOffset += Height + 4;
}

void UUTCTFScoreboard::DrawPlayerScore(AUTPlayerState* PlayerState, float XOffset, float YOffset, float Width, FLinearColor DrawColor)
{
	DrawText(FText::AsNumber(int32(PlayerState->Score)), XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnY, UTHUDOwner->SmallFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	DrawText(FText::AsNumber(PlayerState->FlagCaptures), XOffset + (Width * ColumnHeaderCapsX), YOffset + ColumnY, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	DrawText(FText::AsNumber(PlayerState->Assists), XOffset + (Width * ColumnHeaderAssistsX), YOffset + ColumnY, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	DrawText(FText::AsNumber(PlayerState->FlagReturns), XOffset + (Width * ColumnHeaderReturnsX), YOffset + ColumnY, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
}

void UUTCTFScoreboard::DrawStatsLeft(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom)
{
	DrawScoringPlays(DeltaTime, YPos, XOffset, ScoreWidth, PageBottom);
}

void UUTCTFScoreboard::DrawStatsRight(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom)
{
	DrawTeamScoreBreakdown(DeltaTime, YPos, XOffset, ScoreWidth, PageBottom);
}

int32 UUTCTFScoreboard::GetSmallPlaysCount(int32 NumPlays) const
{
	return  FMath::Clamp(2 * (NumPlays - 6), 0, NumPlays - 1);
}

void UUTCTFScoreboard::DrawScoringPlays(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight)
{
	AUTCTFGameState* CTFState = Cast<AUTCTFGameState>(UTGameState);
	int32 CurrentPeriod = -1;
//	ScoreWidth *= 1.09f;
	Canvas->SetLinearDrawColor(FLinearColor::White);
	FFontRenderInfo TextRenderInfo;
	TextRenderInfo.bEnableShadow = true;
	TextRenderInfo.bClipText = true;

	float XL, SmallYL;
	Canvas->TextSize(UTHUDOwner->SmallFont, "TEST", XL, SmallYL, RenderScale, RenderScale);
	float TinyYL;
	Canvas->TextSize(UTHUDOwner->TinyFont, ScoringPlaysHeader.ToString(), XL, TinyYL, RenderScale, RenderScale);
	float MedYL;
	Canvas->TextSize(UTHUDOwner->MediumFont, ScoringPlaysHeader.ToString(), XL, MedYL, RenderScale, RenderScale);

	float ScoreHeight = 0.85f * (MedYL + TinyYL);
	int32 TotalPlays = CTFState->GetScoringPlays().Num();

	Canvas->DrawText(UTHUDOwner->MediumFont, ScoringPlaysHeader, XOffset + (ScoreWidth - XL) * 0.5f, YPos, RenderScale, RenderScale, TextRenderInfo);
	YPos += 1.f * MedYL;
	if (CTFState->GetScoringPlays().Num() == 0)
	{
		float YL;
		Canvas->TextSize(UTHUDOwner->MediumFont, NoScoringText.ToString(), XL, YL, RenderScale, RenderScale);
		YPos += 0.2f * MedYL;
		DrawText(NoScoringText, XOffset + (ScoreWidth - XL) * 0.5f, YPos, UTHUDOwner->MediumFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, RenderScale, 1.f, FLinearColor::White, FLinearColor(0.0f,0.0f,0.0f,0.0f), ETextHorzPos::Left, ETextVertPos::Top, TextRenderInfo);
	}

	float OldTimeLineOffset = TimeLineOffset;
	TimeLineOffset += DeltaTime;
	if (TimeLineOffset < 0.f)
	{
		return;
	}
	float TimeFloor = FMath::FloorToInt(TimeLineOffset);
	if (UTPlayerOwner && (TimeFloor + 1 <= TotalPlays) && (TimeFloor != FMath::FloorToInt(OldTimeLineOffset)))
	{
		UTPlayerOwner->UTClientPlaySound(ScoreUpdateSound);
	}
	int32 NumPlays = FMath::Min(TotalPlays, int32(TimeFloor) + 1);
	int32 SmallPlays = GetSmallPlaysCount(NumPlays);
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
			if ((CTFState->CTFRound == 0) && (Play.Period > CurrentPeriod))
			{
				CurrentPeriod++;
				if (Play.Period < 3)
				{
					YPos += 0.3f*SmallYL;
					DrawText(PeriodText[Play.Period], XOffset + 0.2f*ScoreWidth, YPos, UTHUDOwner->TinyFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, RenderScale, 1.f, FLinearColor::White, FLinearColor(0.0f,0.0f,0.0f,0.0f), ETextHorzPos::Left, ETextVertPos::Center, TextRenderInfo);
					YPos += 0.7f * SmallYL;
				}
			}
			if (SkippedPlays > 0)
			{
				SkippedPlays--;
				continue;
			}
			bool bIsSmallPlay = (SmallPlays > 0);
			if (bIsSmallPlay)
			{
				SmallPlays--;
			}
			if (bGroupRoundPairs && (DrawnPlays %2 == 1))
			{
				// draw group background
				FLinearColor DrawColor = FLinearColor::White;
				float CurrentScoreHeight = (DrawnPlays == NumPlays) ? (ScoreHeight + 8.f * RenderScale) : (2.f * ScoreHeight + 16.f * RenderScale);
				float BackAlpha = ((DrawnPlays == NumPlays) && (NumPlays == TimeFloor + 1)) ? FMath::Max(0.5f, PctOffset) : 0.3f;
				DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset - 0.0175f*ScoreWidth, YPos - 4.f*RenderScale, 1.06f*ScoreWidth, CurrentScoreHeight, 149, 138, 32, 32, BackAlpha, DrawColor);
			}
			// draw background
			FLinearColor DrawColor = FLinearColor::White;
			float CurrentScoreHeight = bIsSmallPlay ? 0.5f*ScoreHeight : ScoreHeight;
			float BackAlpha = ((DrawnPlays == NumPlays) && (NumPlays == TimeFloor + 1)) ? FMath::Max(0.5f, PctOffset) : 0.5f;
			DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset - 0.0075f*ScoreWidth, YPos, 1.04f*ScoreWidth, CurrentScoreHeight, 149, 138, 32, 32, BackAlpha, DrawColor);

			float BoxYPos = YPos;
			DrawScoringPlayInfo(Play, CurrentScoreHeight, SmallYL, MedYL, DeltaTime, YPos, XOffset, ScoreWidth, TextRenderInfo, bIsSmallPlay);
			YPos = (bGroupRoundPairs && (DrawnPlays % 2 == 0)) ? BoxYPos + CurrentScoreHeight + 20.f*RenderScale : BoxYPos + CurrentScoreHeight + 8.f*RenderScale;
		}
	}
}

void UUTCTFScoreboard::DrawScoringPlayInfo(const FCTFScoringPlay& Play, float CurrentScoreHeight, float SmallYL, float MedYL, float DeltaTime, float& YPos, float XOffset, float ScoreWidth, FFontRenderInfo TextRenderInfo, bool bIsSmallPlay)
{
	AUTCTFGameState* CTFState = Cast<AUTCTFGameState>(UTGameState);
	float BoxYPos = YPos;

	// draw scoring team icon
	int32 IconIndex = Play.Team->TeamIndex;
	IconIndex = FMath::Min(IconIndex, 1);
	float IconHeight = 0.9f*CurrentScoreHeight;
	float IconOffset = 0.13f*ScoreWidth;
	DrawTexture(UTHUDOwner->HUDAtlas, XOffset + IconOffset, YPos + 0.1f*CurrentScoreHeight, IconHeight, IconHeight, UTHUDOwner->TeamIconUV[IconIndex].X, UTHUDOwner->TeamIconUV[IconIndex].Y, 72, 72, 1.f, Play.Team->TeamColor);

	FString ScoredByLine = Play.ScoredBy.GetPlayerName();
	if (Play.ScoredByCaps > 1)
	{
		ScoredByLine += FString::Printf(TEXT(" (%i)"), Play.ScoredByCaps);
	}

	// time of game
	FString TimeStampLine = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), Play.RemainingTime, false, true, false).ToString();
	Canvas->SetLinearDrawColor(FLinearColor::White);
	Canvas->DrawText(UTHUDOwner->SmallFont, TimeStampLine, XOffset + 0.01f*ScoreWidth, YPos + 0.5f*CurrentScoreHeight - 0.5f*SmallYL, RenderScale, RenderScale, TextRenderInfo);

	// scored by
	Canvas->SetLinearDrawColor(Play.Team->TeamColor);
	float ScorerNameYOffset = bIsSmallPlay ? BoxYPos + 0.5f*CurrentScoreHeight - 0.6f*MedYL : YPos;
	float NameSizeX, NameSizeY;
	Canvas->TextSize(UTHUDOwner->MediumFont, ScoredByLine, NameSizeX, NameSizeY, RenderScale, RenderScale);
	Canvas->DrawText(UTHUDOwner->MediumFont, ScoredByLine, XOffset + 0.35f*ScoreWidth, ScorerNameYOffset, RenderScale * FMath::Min(1.f, ScoreWidth*0.4f / FMath::Max(NameSizeX, 1.f)), RenderScale, TextRenderInfo);
	YPos += 0.8f* MedYL;

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
		else if (!Play.bDefenseWon && !Play.bAnnihilation)
		{
			AssistLine = UnassistedText.ToString();
		}
		Canvas->SetLinearDrawColor(FLinearColor::White);
		Canvas->DrawText(UTHUDOwner->TinyFont, AssistLine, XOffset + 0.3f*ScoreWidth, YPos, 0.75f*RenderScale, RenderScale, TextRenderInfo);
	}

	FFormatNamedArguments Args;
	Args.Add("RedScore", FText::AsNumber(Play.TeamScores[0]));
	Args.Add("BlueScore", FText::AsNumber(Play.TeamScores[1]));
	float ScoringOffsetX, ScoringOffsetY;
	Canvas->TextSize(UTHUDOwner->MediumFont, FText::Format(ScoringPlayScore, Args).ToString(), ScoringOffsetX, ScoringOffsetY, RenderScale, RenderScale);
	float SingleXL, SingleYL;
	float ScoreX = XOffset + 0.98f*ScoreWidth - ScoringOffsetX;
	Canvas->SetLinearDrawColor(CTFState->Teams[0]->TeamColor);
	FString SingleScorePart = FString::Printf(TEXT(" %i"), Play.TeamScores[0]);
	Canvas->TextSize(UTHUDOwner->MediumFont, SingleScorePart, SingleXL, SingleYL, RenderScale, RenderScale);
	Canvas->DrawText(UTHUDOwner->MediumFont, SingleScorePart, ScoreX, ScorerNameYOffset, RenderScale, RenderScale, TextRenderInfo);
	Canvas->SetLinearDrawColor(FLinearColor::White);
	ScoreX += SingleXL;
	Canvas->TextSize(UTHUDOwner->MediumFont, "-", SingleXL, SingleYL, RenderScale, RenderScale);
	ScoreX += SingleXL;
	Canvas->DrawText(UTHUDOwner->MediumFont, "-", ScoreX, ScorerNameYOffset, RenderScale, RenderScale, TextRenderInfo);
	Canvas->SetLinearDrawColor(CTFState->Teams[1]->TeamColor);
	ScoreX += SingleXL;
	Canvas->DrawText(UTHUDOwner->MediumFont, FString::Printf(TEXT(" %i"), Play.TeamScores[1]), ScoreX, ScorerNameYOffset, RenderScale, RenderScale, TextRenderInfo);
}

void UUTCTFScoreboard::DrawTeamStats(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight, const FStatsFontInfo& StatsFontInfo)
{
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "FlagCaps", "Flag Captures"), UTGameState->Teams[0]->Score, UTGameState->Teams[1]->Score, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	DrawClockTeamStatsLine(NSLOCTEXT("UTScoreboard", "TeamFlagHeldTime", "Flag Held Time"), NAME_TeamFlagHeldTime, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth , false);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "TeamGrabs", "Flag Grabs"), UTGameState->Teams[0]->GetStatsValue(NAME_TeamFlagGrabs), UTGameState->Teams[1]->GetStatsValue(NAME_TeamFlagGrabs), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "TeamKills", "Kills"), UTGameState->Teams[0]->GetStatsValue(NAME_TeamKills), UTGameState->Teams[1]->GetStatsValue(NAME_TeamKills), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );

	float SectionSpacing = 0.6f * StatsFontInfo.TextHeight;
	YPos += SectionSpacing;

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

	DrawPlayerStatsLine(NSLOCTEXT("UTScoreboard", "TopScorer", "Top Scorer"), TopScorerRed, TopScorerBlue, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	DrawPlayerStatsLine(NSLOCTEXT("UTScoreboard", "TopFlagRunner", "Top Flag Runner"), UTGameState->Teams[0]->TopAttacker, UTGameState->Teams[1]->TopAttacker, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	DrawPlayerStatsLine(NSLOCTEXT("UTScoreboard", "TopDefender", "Top Defender"), UTGameState->Teams[0]->TopDefender, UTGameState->Teams[1]->TopDefender, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	DrawPlayerStatsLine(NSLOCTEXT("UTScoreboard", "TopSupport", "Top Support"), UTGameState->Teams[0]->TopSupporter, UTGameState->Teams[1]->TopSupporter, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	DrawPlayerStatsLine(NSLOCTEXT("UTScoreboard", "TopKills", "Top Kills"), TopKillerRed, TopKillerBlue, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	YPos += SectionSpacing;

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "BeltPickups", "Shield Belt Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ShieldBeltCount), UTGameState->Teams[1]->GetStatsValue(NAME_ShieldBeltCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "VestPickups", "Armor Vest Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ArmorVestCount), UTGameState->Teams[1]->GetStatsValue(NAME_ArmorVestCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "PadPickups", "Thigh Pad Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ArmorPadsCount), UTGameState->Teams[1]->GetStatsValue(NAME_ArmorPadsCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth );

	int32 OptionalLines = 0;
	int32 TeamStat0 = UTGameState->Teams[0]->GetStatsValue(NAME_UDamageCount);
	int32 TeamStat1 = UTGameState->Teams[1]->GetStatsValue(NAME_UDamageCount);
	if (TeamStat0 > 0 || TeamStat1 > 0)
	{
		DrawStatsLine(NSLOCTEXT("UTScoreboard", "UDamagePickups", "UDamage Pickups"), TeamStat0, TeamStat1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
		DrawClockTeamStatsLine(NSLOCTEXT("UTScoreboard", "UDamage", "UDamage Control"), NAME_UDamageTime, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, true);
		OptionalLines += 2;
	}
	TeamStat0 = UTGameState->Teams[0]->GetStatsValue(NAME_BerserkCount);
	TeamStat1 = UTGameState->Teams[1]->GetStatsValue(NAME_BerserkCount);
	if (TeamStat0 > 0 || TeamStat1 > 0)
	{
		DrawStatsLine(NSLOCTEXT("UTScoreboard", "BerserkPickups", "Berserk Pickups"), TeamStat0, TeamStat1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
		DrawClockTeamStatsLine(NSLOCTEXT("UTScoreboard", "Berserk", "Berserk Control"), NAME_BerserkTime, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, true);
		OptionalLines += 2;
	}
	TeamStat0 = UTGameState->Teams[0]->GetStatsValue(NAME_InvisibilityCount);
	TeamStat1 = UTGameState->Teams[1]->GetStatsValue(NAME_InvisibilityCount);
	if (TeamStat0 > 0 || TeamStat1 > 0)
	{
		DrawStatsLine(NSLOCTEXT("UTScoreboard", "InvisibilityPickups", "Invisibility Pickups"), TeamStat0, TeamStat1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
		DrawClockTeamStatsLine(NSLOCTEXT("UTScoreboard", "Invisibility", "Invisibility Control"), NAME_InvisibilityTime, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, true);
		OptionalLines += 2;
	}

	TeamStat0 = UTGameState->Teams[0]->GetStatsValue(NAME_KegCount);
	TeamStat1 = UTGameState->Teams[1]->GetStatsValue(NAME_KegCount);
	if (TeamStat0 > 0 || TeamStat1 > 0)
	{
		DrawStatsLine(NSLOCTEXT("UTScoreboard", "KegPickups", "Keg Pickups"), TeamStat0, TeamStat1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
		OptionalLines += 1;
	}

	if (OptionalLines < 7)
	{
		TeamStat0 = UTGameState->Teams[0]->GetStatsValue(NAME_HelmetCount);
		TeamStat1 = UTGameState->Teams[1]->GetStatsValue(NAME_HelmetCount);
		if (TeamStat0 > 0 || TeamStat1 > 0)
		{
			DrawStatsLine(NSLOCTEXT("UTScoreboard", "HelmetPickups", "Helmet Pickups"), TeamStat0, TeamStat1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
			OptionalLines += 1;
		}
	}

	if (OptionalLines < 7)
	{
		int32 BootJumpsRed = UTGameState->Teams[0]->GetStatsValue(NAME_BootJumps);
		int32 BootJumpsBlue = UTGameState->Teams[1]->GetStatsValue(NAME_BootJumps);
		if ((BootJumpsRed != 0) || (BootJumpsBlue != 0))
		{
			DrawStatsLine(NSLOCTEXT("UTScoreboard", "JumpBootJumps", "JumpBoot Jumps"), BootJumpsRed, BootJumpsBlue, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
			OptionalLines += 1;
		}
	}
	// later do redeemer shots -and all these also to individual
	// track individual movement stats as well
	// @TODO FIXMESTEVE make all the loc text into properties instead of recalc
}



