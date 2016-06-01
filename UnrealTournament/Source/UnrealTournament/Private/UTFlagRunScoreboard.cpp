// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTFlagRunScoreboard.h"
#include "UTCTFRoundGameState.h"
#include "UTCTFScoring.h"

#include "StatNames.h"

UUTFlagRunScoreboard::UUTFlagRunScoreboard(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	CH_Powerup = NSLOCTEXT("UTFlagRunScoreboard", "ColumnHeader_Powerups", "PU");

	ColumnHeaderPowerupX = 0.715f;
	ColumnHeaderPowerupEndX = ColumnHeaderPowerupX + 0.0575f;
	const float AdditionalPadding = 0.09f;
	ColumnHeaderPowerupXDuringReadyUp = ColumnHeaderPingX - AdditionalPadding;
	bGroupRoundPairs = true;
	bUseRoundKills = true;
}

void UUTFlagRunScoreboard::DrawScoreHeaders(float RenderDelta, float& YOffset)
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
			DrawText(CH_Kills, XOffset + (ScaledCellWidth * ColumnHeaderScoreX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			DrawText(CH_Powerup, XOffset + (ScaledCellWidth * 0.5f * (ColumnHeaderPowerupX + ColumnHeaderPowerupEndX)), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			AUTCTFGameState* CTFState = Cast<AUTCTFGameState>(UTGameState);
			if (CTFState && (CTFState->bAttackerLivesLimited || CTFState->bDefenderLivesLimited))
			{
				DrawText(NSLOCTEXT("UTScoreboard", "LivesRemaining", "Lives"), XOffset + (ScaledCellWidth * 0.5f*(ColumnHeaderPowerupX + ColumnHeaderPingX)), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			}
		}
		else
		{
			DrawText(CH_Powerup, XOffset + (ScaledCellWidth * ColumnHeaderPowerupXDuringReadyUp), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		}
		DrawText((GetWorld()->GetNetMode() == NM_Standalone) ?  CH_Skill : CH_Ping, XOffset + (ScaledCellWidth * ColumnHeaderPingX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		XOffset = Canvas->ClipX - ScaledCellWidth - ScaledEdgeSize;
	}

	YOffset += Height + 4;
}

void UUTFlagRunScoreboard::DrawPlayerScore(AUTPlayerState* PlayerState, float XOffset, float YOffset, float Width, FLinearColor DrawColor)
{
	DrawText(FText::AsNumber(int32(PlayerState->RoundKills)), XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnY, UTHUDOwner->SmallFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	AUTCTFGameState* CTFState = Cast<AUTCTFGameState>(UTGameState);
	if (CTFState)
	{
		AUTInventory* SelectedPowerup = PlayerState && PlayerState->BoostClass ? PlayerState->BoostClass->GetDefaultObject<AUTInventory>() : nullptr;
		const float LivesXOffset = (Width * 0.5f*(ColumnHeaderPowerupX + ColumnHeaderPingX));

		//Only display powerups from your team
		if (SelectedPowerup && (ShouldShowPowerupForPlayer(PlayerState)))
		{
			const float U = SelectedPowerup->HUDIcon.U;
			const float V = SelectedPowerup->HUDIcon.V;
			const float UL = SelectedPowerup->HUDIcon.UL;
			const float VL = SelectedPowerup->HUDIcon.VL;
				
			const float AspectRatio = UL / VL;
			const float TextureSize = (Width * (ColumnHeaderPowerupEndX - ColumnHeaderPowerupX));

			//Draw a Red Dash if they have used their powerup, or an icon if they have not yet used it
			AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(CTFState);
			if (RCTFGameState)
			{
				const bool bIsAbleToEarnMorePowerups = (RCTFGameState->IsTeamOnOffense(PlayerState->GetTeamNum()) ? RCTFGameState->bIsOffenseAbleToGainPowerup : RCTFGameState->bIsDefenseAbleToGainPowerup);				
				
				//Draw Dash
				if (!bIsAbleToEarnMorePowerups && !RCTFGameState->IsMatchIntermission() && (PlayerState->GetRemainingBoosts() <= 0))
				{
					DrawText(NSLOCTEXT("UTScoreboard", "Dash", "-"), XOffset + (Width * ((ColumnHeaderPowerupEndX + ColumnHeaderPowerupX) / 2)), YOffset + ColumnY, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
				}
				//Draw powerup symbol
				else
				{
					UTexture* PowerupTexture = SelectedPowerup->HUDIcon.Texture;
					if (PowerupTexture)
					{
						DrawTexture(PowerupTexture, XOffset + (Width * ColumnHeaderPowerupX), YOffset, TextureSize, TextureSize * AspectRatio, U, V, UL, VL);
					}
				}
			}
		}
		else
		{
			DrawText(NSLOCTEXT("UTScoreboard", "Dash", "-"), XOffset + (Width * ColumnHeaderPowerupX), YOffset + ColumnY, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
		}

		if (CTFState->bAttackerLivesLimited || CTFState->bDefenderLivesLimited)
		{
			if (PlayerState->bHasLifeLimit && (PlayerState->RemainingLives >= 0))
			{
				DrawText(FText::AsNumber(PlayerState->RemainingLives+1), XOffset + LivesXOffset, YOffset + ColumnY, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
			}
			else
			{
				DrawText(NSLOCTEXT("UTScoreboard", "Dash", "-"), XOffset + LivesXOffset, YOffset + ColumnY, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
			}
		}
	}
	else
	{
		DrawText(FText::AsNumber(PlayerState->FlagCaptures), XOffset + (Width * ColumnHeaderCapsX), YOffset + ColumnY, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
		DrawText(FText::AsNumber(PlayerState->Assists), XOffset + (Width * ColumnHeaderAssistsX), YOffset + ColumnY, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
		DrawText(FText::AsNumber(PlayerState->FlagReturns), XOffset + (Width * ColumnHeaderReturnsX), YOffset + ColumnY, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	}
}

bool UUTFlagRunScoreboard::ShouldShowPowerupForPlayer(AUTPlayerState* PlayerState)
{
	if (UTGameState->IsMatchInProgress() && !UTGameState->IsMatchIntermission())
	{
		return true;
	}

	else if (UTPlayerOwner->GetTeamNum() == PlayerState->GetTeamNum())
	{
		return true;
	}

	return false;
}

void UUTFlagRunScoreboard::DrawReadyText(AUTPlayerState* PlayerState, float XOffset, float YOffset, float Width)
{
	Super::DrawReadyText(PlayerState, XOffset, YOffset, Width);

	AUTInventory* SelectedPowerup = PlayerState && PlayerState->BoostClass ? PlayerState->BoostClass->GetDefaultObject<AUTInventory>() : nullptr;
	AUTCTFGameState* CTFState = Cast<AUTCTFGameState>(UTGameState);
	if (CTFState && SelectedPowerup && SelectedPowerup->HUDIcon.Texture && ShouldShowPowerupForPlayer(PlayerState))
	{
		const float U = SelectedPowerup->HUDIcon.U;
		const float V = SelectedPowerup->HUDIcon.V;
		const float UL = SelectedPowerup->HUDIcon.UL;
		const float VL = SelectedPowerup->HUDIcon.VL;

		const float AspectRatio = UL / VL;
		const float TextureSize = Width * (ColumnHeaderPowerupEndX - ColumnHeaderPowerupX);

		DrawTexture(SelectedPowerup->HUDIcon.Texture, XOffset + (ScaledCellWidth * ColumnHeaderPowerupXDuringReadyUp) - (TextureSize / 2), YOffset, TextureSize, TextureSize * AspectRatio, U, V, UL, VL);
	}
	else
	{
		DrawText(NSLOCTEXT("UTScoreboard", "Dash", "-"), XOffset + (ScaledCellWidth * ColumnHeaderPowerupXDuringReadyUp), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
	}
}

void UUTFlagRunScoreboard::DrawBonusTeamStatsLine(FText StatsName, float DeltaTime, float XOffset, float& YPos, const FStatsFontInfo& StatsFontInfo, float ScoreWidth, bool bSkipEmpty)
{
	int32 HighlightIndex = 0;
	int32 RedTeamValue = UTGameState->Teams[0]->SecondaryScore;
	int32 BlueTeamValue = UTGameState->Teams[1]->SecondaryScore;
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

void UUTFlagRunScoreboard::DrawTeamStats(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight, const FStatsFontInfo& StatsFontInfo)
{
	DrawBonusTeamStatsLine(NSLOCTEXT("UTScoreboard", "TeamBonusTime", "Bonus Time (Tiebreak)"), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, false);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "TeamKills", "Kills"), UTGameState->Teams[0]->GetStatsValue(NAME_TeamKills), UTGameState->Teams[1]->GetStatsValue(NAME_TeamKills), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);

	float SectionSpacing = 0.6f * StatsFontInfo.TextHeight;
	YPos += SectionSpacing;

	// find top kills
	AUTPlayerState* TopKillerRed = FindTopTeamKillerFor(0);
	AUTPlayerState* TopKillerBlue = FindTopTeamKillerFor(1);

	DrawPlayerStatsLine(NSLOCTEXT("UTScoreboard", "TopFlagRunner", "Top Flag Runner"), UTGameState->Teams[0]->TopAttacker, UTGameState->Teams[1]->TopAttacker, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	DrawPlayerStatsLine(NSLOCTEXT("UTScoreboard", "TopKills", "Top Kills"), TopKillerRed, TopKillerBlue, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	YPos += SectionSpacing;

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "BeltPickups", "Shield Belt Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ShieldBeltCount), UTGameState->Teams[1]->GetStatsValue(NAME_ShieldBeltCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "VestPickups", "Armor Vest Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ArmorVestCount), UTGameState->Teams[1]->GetStatsValue(NAME_ArmorVestCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "PadPickups", "Thigh Pad Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ArmorPadsCount), UTGameState->Teams[1]->GetStatsValue(NAME_ArmorPadsCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);

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

void UUTFlagRunScoreboard::DrawScoringPlays(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight)
{
	Super::DrawScoringPlays(DeltaTime, YPos, XOffset, ScoreWidth, MaxHeight);

	if (TimeLineOffset < 1.f)
	{
		return;
	}
	AUTCTFRoundGameState* GS = GetWorld()->GetGameState<AUTCTFRoundGameState>();
	if (GS && !GS->HasMatchEnded() && ((GS->CTFRound == GS->NumRounds-2) || (GS->CTFRound == GS->NumRounds - 1)) && (GS->Teams.Num() > 1) && GS->Teams[0] && GS->Teams[1])
	{
		Canvas->SetLinearDrawColor(FLinearColor::White);
		FFontRenderInfo TextRenderInfo;
		TextRenderInfo.bEnableShadow = true;
		TextRenderInfo.bClipText = true;

		AUTTeamInfo* NextAttacker = GS->bRedToCap ? GS->Teams[1] : GS->Teams[0];
		AUTTeamInfo* NextDefender = GS->bRedToCap ? GS->Teams[0] : GS->Teams[1];
		FText AttackerNameText = (NextAttacker->TeamIndex == 0) ? RedTeamText : BlueTeamText;
		FText DefenderNameText = (NextDefender->TeamIndex == 0) ? RedTeamText : BlueTeamText;
		FFormatNamedArguments Args;
		Args.Add("AttackerName", AttackerNameText);
		Args.Add("DefenderName", DefenderNameText);
		int32 RequiredTime = NextDefender->SecondaryScore - NextAttacker->SecondaryScore;
		FText TimeStampLine = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), RequiredTime, false, true, false);
		Args.Add("TimeNeeded", TimeStampLine);
		FText BonusType = GS->BronzeBonusText;

		if (GS->CTFRound == GS->NumRounds - 2)
		{
			if (NextAttacker->Score > NextDefender->Score)
			{
				Canvas->SetLinearDrawColor(NextDefender->TeamIndex == 0 ? FLinearColor::Red : FLinearColor::Blue);
				FString DefenderName = FText::Format(NSLOCTEXT("UTFlagRun", "DefendersName", "{DefenderName}"), Args).ToString();
				float XL, YL;
				Canvas->StrLen(UTHUDOwner->SmallFont,DefenderName, XL, YL);
				Canvas->DrawText(UTHUDOwner->SmallFont, DefenderName, XOffset, YPos - 0.1f*YL*RenderScale, 1.1f*RenderScale, 1.1f*RenderScale, TextRenderInfo);
				XOffset += 1.1f*XL*RenderScale;
				Canvas->SetLinearDrawColor(FLinearColor::White);
				if (NextAttacker->Score - NextDefender->Score > 2)
				{
					// Defenders must stop attackers to have a chance
					Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "DefendersMustStop", " must hold on defense to have a chance."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
				}
				else
				{
					BonusType = (NextAttacker->Score - NextDefender->Score == 2) ? GS->BronzeBonusText : GS->SilverBonusText;
					Args.Add("BonusType", BonusType);
					Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "DefendersMustHold", " must hold {AttackerName} to {BonusType} to have a chance."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
				}
			}
			else if (NextDefender->Score > NextAttacker->Score)
			{
				Canvas->SetLinearDrawColor(NextAttacker->TeamIndex == 0 ? FLinearColor::Red : FLinearColor::Blue);
				FString AttackerName = FText::Format(NSLOCTEXT("UTFlagRun", "AttackerName", "{AttackerName}"), Args).ToString();
				float XL, YL;
				Canvas->StrLen(UTHUDOwner->SmallFont, AttackerName, XL, YL);
				Canvas->DrawText(UTHUDOwner->SmallFont, AttackerName, XOffset, YPos - 0.1f*YL*RenderScale, 1.1f*RenderScale, 1.1f*RenderScale, TextRenderInfo);
				XOffset += 1.1f*XL*RenderScale;
				Canvas->SetLinearDrawColor(FLinearColor::White);
				if (NextDefender->Score - NextAttacker->Score > 1)
				{
					// compare bonus times, see what level is implied and state for Attackers to have a chance
					if (RequiredTime < 60 * (NextDefender->Score - NextAttacker->Score - 1))
					{
						if (NextDefender->Score - NextAttacker->Score > 2)
						{
							BonusType = (NextDefender->Score - NextAttacker->Score == 4) ? GS->GoldBonusText : GS->SilverBonusText;
						}
						Args.Add("BonusType", BonusType);
						Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "AttackersMustScore", " must score {BonusType} to have a chance."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
					}
					else
					{
						// state required time and threshold
						if (RequiredTime >= 60)
						{
							BonusType = (RequiredTime >= 120) ? GS->GoldBonusText : GS->SilverBonusText;
						}
						Args.Add("BonusType", BonusType);
						Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "AttackersMustScoreTime", " must score {BonusType} with at least\n {TimeNeeded} remaining to have a chance."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
					}
				}
				else if (NextDefender->Score - NextAttacker->Score == 1)
				{
					// Attackers must score bronze
					Args.Add("BonusType", BonusType);
					Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "AttackersMustScore", " must score {BonusType} to have a chance."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
				}
				else
				{
					// WTF - unhandled condition
					Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "UnhandledCondition", "UNHANDLED WIN CONDITION"), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
				}
			}
		}
		else if (GS->CTFRound == GS->NumRounds - 1)
		{
			Canvas->SetLinearDrawColor(NextAttacker->TeamIndex == 0 ? FLinearColor::Red : FLinearColor::Blue);
			FString AttackerName = FText::Format(NSLOCTEXT("UTFlagRun", "AttackerName", "{AttackerName}"), Args).ToString();
			float XL, YL;
			Canvas->StrLen(UTHUDOwner->SmallFont, AttackerName, XL, YL);
			Canvas->DrawText(UTHUDOwner->SmallFont, AttackerName, XOffset, YPos-0.1f*YL*RenderScale, 1.1f*RenderScale, 1.1f*RenderScale, TextRenderInfo);
			XOffset += 1.1f*XL*RenderScale;
			Canvas->SetLinearDrawColor(FLinearColor::White);
			if (NextDefender->Score == NextAttacker->Score)
			{
				Args.Add("BonusType", BonusType);
				Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "AttackersMustScoreWin", " must score {BonusType} to win."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
			}
			else
			{
				if ((RequiredTime < 60 * (NextDefender->Score - NextAttacker->Score - 1)) || (RequiredTime > 60 * (NextDefender->Score - NextAttacker->Score)))
				{
					BonusType = (NextDefender->Score - NextAttacker->Score == 2) ? GS->GoldBonusText : GS->SilverBonusText;
					Args.Add("BonusType", BonusType);
					Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "AttackersMustScoreWin", " must score {BonusType} to win."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
				}
				else
				{
					if (RequiredTime >= 60)
					{
						BonusType = (RequiredTime >= 120) ? GS->GoldBonusText : GS->SilverBonusText;
					}
					Args.Add("BonusType", BonusType);
					Canvas->DrawText(UTHUDOwner->SmallFont, FText::Format(NSLOCTEXT("UTFlagRun", "AttackersMustScoreTimeWin", " must score {BonusType} with\n at least {TimeNeeded} remaining to win."), Args).ToString(), XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
				}
			}
		}
	}
}
