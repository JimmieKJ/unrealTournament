// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTTeamScoreboard.h"
#include "StatNames.h"

UUTTeamScoreboard::UUTTeamScoreboard(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NumPages = 2;
}

void UUTTeamScoreboard::DrawTeamPanel(float RenderDelta, float& YOffset)
{
	if (UTGameState == NULL || UTGameState->Teams.Num() < 2 || UTGameState->Teams[0] == NULL || UTGameState->Teams[1] == NULL) return;

	// Draw the front end End
	float Width = (Size.X * 0.5) - CenterBuffer;

	float FrontSize = 35;
	float EndSize = 16;
	float MiddleSize = Width - FrontSize - EndSize;
	float XOffset = 0;

	// Draw the Background
	DrawTexture(TextureAtlas, XOffset, YOffset + 22, 35, 65, 0, 188, 36, 65, 1.0, FLinearColor::Red);
	DrawTexture(TextureAtlas, XOffset + FrontSize, YOffset + 22, MiddleSize, 65, 39,188,64,65, 1.0, FLinearColor::Red);
	DrawTexture(TextureAtlas, XOffset + FrontSize + MiddleSize, YOffset + 22, EndSize, 65, 39,188,64,65, 1.0, FLinearColor::Red);

	DrawText(NSLOCTEXT("UTTeamScoreboard","RedTeam","RED"), 36, YOffset  + CellHeight, UTHUDOwner->HugeFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
	DrawText(FText::AsNumber(UTGameState->Teams[0]->Score), Width * 0.9, YOffset + 48, UTHUDOwner->ScoreFont, false, FVector2D(0, 0), FLinearColor::Black, true, FLinearColor::Black, 0.75, 1.0, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);

	XOffset = Size.X - Width;

	DrawTexture(TextureAtlas, XOffset + EndSize + MiddleSize, YOffset + 22, 35, 65, 196, 188, 36, 65 , 1.0, FLinearColor::Blue);
	DrawTexture(TextureAtlas, XOffset + EndSize, YOffset + 22, MiddleSize, 65, 130,188,64,65, 1.0, FLinearColor::Blue);
	DrawTexture(TextureAtlas, XOffset, YOffset + 22, EndSize, 65, 117, 188, 16, 65, 1.0, FLinearColor::Blue);

	DrawText(NSLOCTEXT("UTTeamScoreboard", "BlueTeam", "BLUE"), 1237, YOffset + CellHeight, UTHUDOwner->HugeFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);
	DrawText(FText::AsNumber(UTGameState->Teams[1]->Score), Size.X - Width + (Width * 0.1), YOffset + 48, UTHUDOwner->ScoreFont, false, FVector2D(0, 0), FLinearColor::Black, true, FLinearColor::Black, 0.75, 1.0, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
	YOffset += 119;
}

void UUTTeamScoreboard::DrawPlayerScores(float RenderDelta, float& YOffset)
{
	if (UTGameState == nullptr)
	{
		return;
	}

	int32 NumSpectators = 0;
	int32 XOffset = 0;

	for (int8 Team = 0; Team < 2; Team++)
	{
		int32 Place = 1;
		float DrawOffset = YOffset;
		for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PlayerState = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
			if (PlayerState)
			{
				if (!PlayerState->bOnlySpectator)
				{
					if (PlayerState->GetTeamNum() == Team)
					{
						DrawPlayer(Place, PlayerState, RenderDelta, XOffset, DrawOffset);
						Place++;
						DrawOffset += CellHeight;
					}
				} 
				else if (Team == 0)
				{
					NumSpectators++;
				}
			}
		}
		XOffset = Size.X - ((Size.X * 0.5) - CenterBuffer);
	}

	if (UTGameState->PlayerArray.Num() <= 28 && NumSpectators > 0)
	{
		FText SpectatorCount = (NumSpectators == 1)
			? NSLOCTEXT("UTScoreboard", "OneSpectator", "1 spectator is watching this match")
			: FText::Format(NSLOCTEXT("UTScoreboard", "SpectatorFormat", "{0} spectators are watching this match"), FText::AsNumber(NumSpectators));
		DrawText(SpectatorCount, Size.X * 0.5, 765, UTHUDOwner->SmallFont, 1.0f, 1.0f, FLinearColor(0.75f, 0.75f, 0.75f, 1.0f), ETextHorzPos::Center, ETextVertPos::Bottom);
	}
}

void UUTTeamScoreboard::SelectNext(int32 Offset, bool bDoNoWrap)
{
	AUTGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GS == NULL) return;

	GS->SortPRIArray();
	int32 SelectedIndex = GS->PlayerArray.Find(SelectedPlayer.Get());

	if (SelectedIndex >= 0 && SelectedIndex < GS->PlayerArray.Num())
	{
		AUTPlayerState* Next = NULL;
		int32 Step = Offset > 0 ? 1 : -1;
		do
		{
			SelectedIndex += Step;
			if (SelectedIndex < 0 || SelectedIndex >= GS->PlayerArray.Num()) return;

			Next = Cast<AUTPlayerState>(GS->PlayerArray[SelectedIndex]);
			if (Next && !Next->bOnlySpectator && !Next->bIsSpectator && GS->OnSameTeam(Next, SelectedPlayer.Get()))
			{
				// Valid potential player.
				Offset -= Step;
				if (Offset == 0)
				{
					SelectedPlayer = Next;
					return;
				}
			}
		}
		while (Next != SelectedPlayer.Get());
	}
	else
	{
		DefaultSelection(GS);
	}

}

void UUTTeamScoreboard::SelectionLeft()
{
	SelectionRight();
}

void UUTTeamScoreboard::SelectionRight()
{
	AUTGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GS == NULL) return;
	GS->SortPRIArray();

	if (SelectedPlayer.IsValid())
	{
		DefaultSelection(GS, SelectedPlayer->GetTeamNum() == 0 ? 1 : 0);
	}
	else
	{
		DefaultSelection(GS, 0);
	}
}

void UUTTeamScoreboard::DrawClockTeamStatsLine(FText StatsName, FName StatsID, float DeltaTime, float XOffset, float& YPos, const FStatsFontInfo& StatsFontInfo, float ScoreWidth, bool bSkipEmpty)
{
	int32 HighlightIndex = 0;
	int32 RedTeamValue = UTGameState->Teams[0]->GetStatsValue(StatsID);
	int32 BlueTeamValue = UTGameState->Teams[1]->GetStatsValue(StatsID);
	if (bSkipEmpty && (RedTeamValue == 0) && (BlueTeamValue == 0))
	{
		return;
	}
	if (RedTeamValue < BlueTeamValue)
	{
		HighlightIndex = 2;
	}
	else if (RedTeamValue > BlueTeamValue)
	{
		HighlightIndex = 1;
	}

	FText ClockStringRed = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), RedTeamValue, false);
	FText ClockStringBlue = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), BlueTeamValue, false);
	DrawTextStatsLine(StatsName, ClockStringRed.ToString(), ClockStringBlue.ToString(), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, HighlightIndex);
}

AUTPlayerState* UUTTeamScoreboard::FindTopTeamKillerFor(uint8 TeamNum)
{
	TArray<AUTPlayerState*> MemberPS;
	for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS && (PS->GetTeamNum() == TeamNum))
		{
			MemberPS.Add(PS);
		}
	}

	MemberPS.Sort([](const AUTPlayerState& A, const AUTPlayerState& B) -> bool
	{
		return A.Kills > B.Kills;
	});
	return ((MemberPS.Num() > 0) && (MemberPS[0]->Kills > 0)) ? MemberPS[0] : NULL;
}

AUTPlayerState* UUTTeamScoreboard::FindTopTeamKDFor(uint8 TeamNum)
{
	TArray<AUTPlayerState*> MemberPS;
	for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS && (PS->GetTeamNum() == TeamNum))
		{
			MemberPS.Add(PS);
		}
	}

	MemberPS.Sort([](const AUTPlayerState& A, const AUTPlayerState& B) -> bool
	{
		if (A.Deaths == 0)
		{
			if (B.Deaths == 0)
			{
				return A.Kills > B.Kills;
			}
			return true;
		}
		if (B.Deaths == 0)
		{
			return (B.Kills == 0);
		}
		return A.Kills / A.Deaths > B.Kills / B.Deaths;
	});
	return ((MemberPS.Num() > 0) && (MemberPS[0]->Kills > 0)) ? MemberPS[0] : NULL;
}

void UUTTeamScoreboard::SetScoringPlaysTimer(bool bEnableTimer)
{
	if (UTHUDOwner)
	{
		if (bEnableTimer)
		{
			UTHUDOwner->GetWorld()->GetTimerManager().SetTimer(OpenScoringPlaysHandle, this, &UUTTeamScoreboard::OpenScoringPlaysPage, 6.0f, false);
		}
		else
		{
			UTHUDOwner->GetWorld()->GetTimerManager().ClearTimer(OpenScoringPlaysHandle);
		}
	}
}

void UUTTeamScoreboard::OpenScoringPlaysPage()
{
	if (UTHUDOwner && UTHUDOwner->ScoreboardPage == 0)
	{
		SetPage(1);
	}
}

void UUTTeamScoreboard::DrawStatsLeft(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom)
{
	DrawTeamScoreBreakdown(DeltaTime, YPos, XOffset, ScoreWidth, PageBottom);
}

void UUTTeamScoreboard::DrawTeamScoreBreakdown(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom)
{
	Canvas->SetLinearDrawColor(FLinearColor::White);
	FStatsFontInfo StatsFontInfo;
	StatsFontInfo.TextRenderInfo.bEnableShadow = true;
	StatsFontInfo.TextRenderInfo.bClipText = true;
	StatsFontInfo.TextFont = UTHUDOwner->SmallFont;
	bHighlightStatsLineTopValue = true;

	FText CombinedHeader = NSLOCTEXT("UTTeamScoreboard", "TeamScoringBreakDownHeader", "Team Scoring Breakdown");
	float XL, SmallYL ;
	Canvas->TextSize(UTHUDOwner->SmallFont, "TEST", XL, SmallYL, RenderScale, RenderScale);
	StatsFontInfo.TextHeight = SmallYL;
	float MedYL;
	Canvas->TextSize(UTHUDOwner->MediumFont, CombinedHeader.ToString(), XL, MedYL, RenderScale, RenderScale);
	Canvas->DrawText(UTHUDOwner->MediumFont, CombinedHeader, XOffset + 0.5f*(ScoreWidth - XL), YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	YPos += 1.1f * MedYL;

	if (UTGameState == NULL || UTGameState->Teams.Num() < 2 || UTGameState->Teams[0] == NULL || UTGameState->Teams[1] == NULL)
	{
		return;
	}

	// draw team icons
	float IconHeight = MedYL;
	DrawTexture(UTHUDOwner->HUDAtlas, XOffset + ValueColumn*ScoreWidth - 0.25f*IconHeight, YPos, IconHeight, IconHeight, UTHUDOwner->TeamIconUV[0].X, UTHUDOwner->TeamIconUV[0].Y, 72, 72, 1.f, UTGameState->Teams[0]->TeamColor);
	DrawTexture(UTHUDOwner->HUDAtlas, XOffset + ScoreColumn*ScoreWidth - 0.25f*IconHeight, YPos, IconHeight, IconHeight, UTHUDOwner->TeamIconUV[1].X, UTHUDOwner->TeamIconUV[1].Y, 72, 72, 1.f, UTGameState->Teams[1]->TeamColor);
	YPos += 1.1f * MedYL;
	DrawTeamStats(DeltaTime, YPos, XOffset, ScoreWidth, PageBottom, StatsFontInfo);
}

void UUTTeamScoreboard::DrawTeamStats(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom, const FStatsFontInfo& StatsFontInfo)
{
	// draw team stats
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "TeamKills", "Kills"), UTGameState->Teams[0]->GetStatsValue(NAME_TeamKills), UTGameState->Teams[1]->GetStatsValue(NAME_TeamKills), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);

	// find top scorer
	AUTPlayerState* TopScorerRed = NULL;
	AUTPlayerState* TopScorerBlue = NULL;
	for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS && PS->Team)
		{
			if (!TopScorerRed && (PS->Team->TeamIndex == 0))
			{
				TopScorerRed = PS;
			}
			else if (!TopScorerBlue && (PS->Team->TeamIndex == 1))
			{
				TopScorerBlue = PS;
			}
			if (TopScorerRed && TopScorerBlue)
			{
				break;
			}
		}
	}

	// find top kills && KD
	AUTPlayerState* TopKillerRed = FindTopTeamKillerFor(0);
	AUTPlayerState* TopKillerBlue = FindTopTeamKillerFor(1);
	AUTPlayerState* TopKDRed = FindTopTeamKDFor(0);
	AUTPlayerState* TopKDBlue = FindTopTeamKDFor(1);

	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "TopScorer", "Top Scorer"), GetPlayerNameFor(TopScorerRed), GetPlayerNameFor(TopScorerBlue), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "TopKills", "Top Kills"), GetPlayerNameFor(TopKillerRed), GetPlayerNameFor(TopKillerBlue), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "TopKD", "Top K/D"), GetPlayerNameFor(TopKDRed), GetPlayerNameFor(TopKDBlue), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "BeltPickups", "Shield Belt Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ShieldBeltCount), UTGameState->Teams[1]->GetStatsValue(NAME_ShieldBeltCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "VestPickups", "Armor Vest Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ArmorVestCount), UTGameState->Teams[1]->GetStatsValue(NAME_ArmorVestCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "PadPickups", "Thigh Pad Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ArmorPadsCount), UTGameState->Teams[1]->GetStatsValue(NAME_ArmorPadsCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "HelmetPickups", "Helmet Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_HelmetCount), UTGameState->Teams[1]->GetStatsValue(NAME_HelmetCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);

	DrawClockTeamStatsLine(NSLOCTEXT("UTScoreboard", "UDamage", "UDamage Control"), NAME_UDamageTime, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, true);
	DrawClockTeamStatsLine(NSLOCTEXT("UTScoreboard", "Berserk", "Berserk Control"), NAME_BerserkTime, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, true);
	DrawClockTeamStatsLine(NSLOCTEXT("UTScoreboard", "Invisibility", "Invisibility Control"), NAME_InvisibilityTime, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, true);

	int32 BootJumpsRed = UTGameState->Teams[0]->GetStatsValue(NAME_BootJumps);
	int32 BootJumpsBlue = UTGameState->Teams[1]->GetStatsValue(NAME_BootJumps);
	if ((BootJumpsRed != 0) || (BootJumpsBlue != 0))
	{
		DrawStatsLine(NSLOCTEXT("UTScoreboard", "JumpBootJumps", "JumpBoot Jumps"), BootJumpsRed, BootJumpsBlue, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	}

	// @TOD FIXMESTEVE redeemer shots as part of map control
	// make all the loc text into properties instead of recalc
}

