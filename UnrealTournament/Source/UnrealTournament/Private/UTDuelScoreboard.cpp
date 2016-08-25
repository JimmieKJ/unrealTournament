// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTDuelScoreboard.h"
#include "StatNames.h"

UUTDuelScoreboard::UUTDuelScoreboard(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	TeamScoringHeader = NSLOCTEXT("UTTeamScoreboard", "DuelBreakDownHeader", "Overall Breakdown");
}

void UUTDuelScoreboard::DrawTeamStats(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom, const FStatsFontInfo& StatsFontInfo)
{
	AUTPlayerState* TopScorerRed = NULL;
	AUTPlayerState* TopScorerBlue = NULL;
	TArray<AUTPlayerState*> MemberPS;
	//Check all player states. Including InactivePRIs
	for (TActorIterator<AUTPlayerState> It(GetWorld()); It; ++It)
	{
		AUTPlayerState* PS = (*It);
		if (PS && (PS->GetTeamNum() == 0))
		{
			TopScorerRed = PS;
		}
		else if (PS && (PS->GetTeamNum() == 1))
		{
			TopScorerBlue = PS;
		}
	}

	DrawPlayerStatsLine(NSLOCTEXT("UTScoreboard", "Dueler", " "), TopScorerRed, TopScorerBlue, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "TeamKills", "Kills"), UTGameState->Teams[0]->GetStatsValue(NAME_TeamKills), UTGameState->Teams[1]->GetStatsValue(NAME_TeamKills), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	FText RedFavoriteWeapon = (TopScorerRed && TopScorerRed->FavoriteWeapon) ? TopScorerRed->FavoriteWeapon->GetDefaultObject<AUTWeapon>()->DisplayName : FText::GetEmpty();
	FText BlueFavoriteWeapon = (TopScorerBlue && TopScorerBlue->FavoriteWeapon) ? TopScorerBlue->FavoriteWeapon->GetDefaultObject<AUTWeapon>()->DisplayName : FText::GetEmpty();
	if (!RedFavoriteWeapon.IsEmpty() || !BlueFavoriteWeapon.IsEmpty())
	{
		DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "FavoriteWeapon", "Favorite Weapon"), RedFavoriteWeapon.ToString(), BlueFavoriteWeapon.ToString(), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	}

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "BeltPickups", "Shield Belt Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ShieldBeltCount), UTGameState->Teams[1]->GetStatsValue(NAME_ShieldBeltCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "LargeArmorPickups", "Large Armor Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ArmorVestCount), UTGameState->Teams[1]->GetStatsValue(NAME_ArmorVestCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "MediumArmorPickups", "Medium Armor Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ArmorPadsCount), UTGameState->Teams[1]->GetStatsValue(NAME_ArmorPadsCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "SmallArmorPickups", "Small Armor Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_HelmetCount), UTGameState->Teams[1]->GetStatsValue(NAME_HelmetCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);

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

	DrawClockTeamStatsLine(NSLOCTEXT("UTScoreboard", "UDamage", "UDamage Control"), NAME_UDamageTime, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, true);
	DrawClockTeamStatsLine(NSLOCTEXT("UTScoreboard", "Berserk", "Berserk Control"), NAME_BerserkTime, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, true);
	DrawClockTeamStatsLine(NSLOCTEXT("UTScoreboard", "Invisibility", "Invisibility Control"), NAME_InvisibilityTime, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, true);

	int32 BootJumpsRed = UTGameState->Teams[0]->GetStatsValue(NAME_BootJumps);
	int32 BootJumpsBlue = UTGameState->Teams[1]->GetStatsValue(NAME_BootJumps);
	if ((BootJumpsRed != 0) || (BootJumpsBlue != 0))
	{
		DrawStatsLine(NSLOCTEXT("UTScoreboard", "JumpBootJumps", "JumpBoot Jumps"), BootJumpsRed, BootJumpsBlue, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	}
}