// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTFlagRunScoreboard.h"
#include "UTFlagRunGameState.h"
#include "UTCTFScoring.h"
#include "UTFlagRunMessage.h"
#include "StatNames.h"
#include "UTFlagRunHUD.h"
#include "UTCTFRoleMessage.h"
#include "UTFlagRunGame.h"
#include "UTHUDWidgetAnnouncements.h"

UUTFlagRunScoreboard::UUTFlagRunScoreboard(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	CH_Powerup = NSLOCTEXT("UTFlagRunScoreboard", "ColumnHeader_Powerups", "");
	ColumnHeaderPowerupX = 0.715f;
	ColumnHeaderPowerupEndX = ColumnHeaderPowerupX + 0.0575f;
	ColumnHeaderPowerupXDuringReadyUp = 0.82f;
	bGroupRoundPairs = true;
	bUseRoundKills = true;

	DefendTitle = NSLOCTEXT("UTScoreboard", "Defending", "Round {RoundNum} of {NumRounds} - DEFENDING");
	AttackTitle = NSLOCTEXT("UTScoreboard", "Attacking", "Round {RoundNum} of {NumRounds} - ATTACKING");

	DefendLines.Add(NSLOCTEXT("UTScoreboard", "DefenseLine1", "* You are defending.  Your goal is to keep the enemy from"));
	DefendLines.Add(NSLOCTEXT("UTScoreboard", "DefenseLine1b", "  bringing the flag to your base, and not run out of lives."));
	DefendLines.Add(FText::GetEmpty());
	DefendLines.Add(NSLOCTEXT("UTScoreboard", "DefenseLine2", "* You have five lives.  The attackers do not have a life limit."));
	DefendLines.Add(FText::GetEmpty());
	DefendLines.Add(NSLOCTEXT("UTScoreboard", "DefenseLine3", "* Attackers can earn 1 to 3 stars depending on how quickly"));
	DefendLines.Add(NSLOCTEXT("UTScoreboard", "DefenseLine3b", "  they score."));
	DefendLines.Add(FText::GetEmpty());
	DefendLines.Add(NSLOCTEXT("UTScoreboard", "DefenseLine4", "* Defenders earn 1 star for preventing the attackers from"));
	DefendLines.Add(NSLOCTEXT("UTScoreboard", "DefenseLine4b", "  scoring within the round time limit."));
	DefendLines.Add(FText::GetEmpty());
	DefendLines.Add(NSLOCTEXT("UTScoreboard", "DefenseLine5", "* When the flag carrier powers up a Rally Point, teammates"));
	DefendLines.Add(NSLOCTEXT("UTScoreboard", "DefenseLine5b", "  can teleport to it by pressing the rally button."));

	AttackLines.Add(NSLOCTEXT("UTScoreboard", "AttackLine1", "* You are attacking.  Your goal is to deliver your flag as"));
	AttackLines.Add(NSLOCTEXT("UTScoreboard", "AttackLine1b", "  fast as possible to the enemy base, or exhaust their lives."));
	AttackLines.Add(FText::GetEmpty());
	AttackLines.Add(NSLOCTEXT("UTScoreboard", "AttackLine2", "* Defenders have five lives.  You do not have a life limit."));
	AttackLines.Add(FText::GetEmpty());
	AttackLines.Add(NSLOCTEXT("UTScoreboard", "DefenseLine3", "* Attackers can earn 1 to 3 stars depending on how quickly"));
	AttackLines.Add(NSLOCTEXT("UTScoreboard", "DefenseLine3b", "  they score."));
	AttackLines.Add(FText::GetEmpty());
	AttackLines.Add(NSLOCTEXT("UTScoreboard", "DefenseLine4", "* Defenders earn 1 star for preventing the attackers from"));
	AttackLines.Add(NSLOCTEXT("UTScoreboard", "DefenseLine4b", "  scoring within the round time limit."));
	AttackLines.Add(FText::GetEmpty());
	AttackLines.Add(NSLOCTEXT("UTScoreboard", "DefenseLine5", "* When the flag carrier powers up a Rally Point, teammates"));
	AttackLines.Add(NSLOCTEXT("UTScoreboard", "DefenseLine5b", "  can teleport to it by pressing the rally button."));

	static ConstructorHelpers::FObjectFinder<USoundBase> PressedSelect(TEXT("SoundCue'/Game/RestrictedAssets/Audio/UI/A_UI_BigSelect02_Cue.A_UI_BigSelect02_Cue'")); 
	LineDisplaySound = PressedSelect.Object;
}

bool UUTFlagRunScoreboard::IsBeforeFirstRound()
{
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	return !GS || (GS->GetScoringPlays().Num() == 0);
}

void UUTFlagRunScoreboard::DrawScorePanel(float RenderDelta, float& YOffset)
{
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	AUTFlagRunGame* DefaultGame = GS && GS->GameModeClass ? GS->GameModeClass->GetDefaultObject<AUTFlagRunGame>() : nullptr;
	if (DefaultGame && (GS->IsMatchIntermission() && ((GS->IntermissionTime > DefaultGame->IntermissionDuration - GS->HalftimeScoreDelay) || IsBeforeFirstRound())) 
		|| (GS->GetMatchState() == MatchState::CountdownToBegin) || (GS->GetMatchState() == MatchState::PlayerIntro))
	{
		LastScorePanelYOffset = 0.f;
		if (UTHUDOwner && UTHUDOwner->AnnouncementWidget)
		{
			UTHUDOwner->AnnouncementWidget->PreDraw(0.f, UTHUDOwner, Canvas, CanvasCenter);
			UTHUDOwner->AnnouncementWidget->Draw(RenderDelta);
			UTHUDOwner->AnnouncementWidget->PostDraw(0.f);
		}
	}
	else
	{
		Super::DrawScorePanel(RenderDelta, YOffset);
	}
}

void UUTFlagRunScoreboard::DrawMinimap(float RenderDelta)
{
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTPlayerOwner->PlayerState);
	AUTFlagRunGame* DefaultGame = GS && GS->GameModeClass ? GS->GameModeClass->GetDefaultObject<AUTFlagRunGame>() : nullptr;
	if (GS && ((GS->GetMatchState() == MatchState::MatchIntermission) || GS->HasMatchEnded()))
	{
		if (DefaultGame && (GS->IntermissionTime > DefaultGame->IntermissionDuration - GS->HalftimeScoreDelay))
		{
			return;
		}
		const float MapSize = (UTGameState && UTGameState->bTeamGame) ? FMath::Min(Canvas->ClipX - 2.f*ScaledEdgeSize - 2.f*ScaledCellWidth, 0.9f*Canvas->ClipY - 120.f * RenderScale)
			: FMath::Min(0.5f*Canvas->ClipX, 0.9f*Canvas->ClipY - 120.f * RenderScale);
		float MapYPos = (LastScorePanelYOffset > 0.f) ? LastScorePanelYOffset - 2.f : 0.25f*Canvas->ClipY;
		FVector2D LeftCorner = FVector2D(MinimapCenter.X*Canvas->ClipX - 0.5f*MapSize, MapYPos);
		if (GS->GetScoringPlays().Num() > 0)
		{
			if (DefaultGame)
			{
				float Height = 0.5f*Canvas->ClipY;
				float ScoreWidth = 0.92f*MapSize;
				float YPos = LeftCorner.Y;
				float PageBottom = YPos + 0.1f*Canvas->ClipY;
				DrawScoringSummary(RenderDelta, YPos, LeftCorner.X + 0.04f*MapSize, 0.9f*ScoreWidth, PageBottom);
				LeftCorner.Y += 0.15f*Canvas->ClipY;
			}
		}
		if ((EndIntermissionTime < GetWorld()->GetTimeSeconds()) && (GS->IntermissionTime < 9.f) && (GS->IntermissionTime > 0.f))
		{
			EndIntermissionTime = GetWorld()->GetTimeSeconds() + 9.1f;
			OldDisplayedParagraphs = 0;
			bFullListPlayed = false;
			bool bIsOnDefense = UTPS && UTPS->Team && GS->IsTeamOnDefenseNextRound(UTPS->Team->TeamIndex);
			UTPlayerOwner->ClientReceiveLocalizedMessage(UUTCTFRoleMessage::StaticClass(), bIsOnDefense ? 2 : 1);
		}

		if (UTPS && UTPS->Team && (EndIntermissionTime > GetWorld()->GetTimeSeconds()) && UTHUDOwner && UTHUDOwner->UTPlayerOwner)
		{
			// draw round information
			const bool bIsOnDefense = GS->IsTeamOnDefenseNextRound(UTPS->Team->TeamIndex);
			int32 NumLines = bIsOnDefense ? DefendLines.Num() : AttackLines.Num();
			bool bIsAfterFirstRound = !IsBeforeFirstRound();
			float Height = bIsAfterFirstRound ? RenderScale * 80.f : RenderScale * (80.f + 32.f*NumLines);
			DrawTexture(UTHUDOwner->ScoreboardAtlas, LeftCorner.X + 0.04f*MapSize, LeftCorner.Y, 0.92f*MapSize, Height, 149, 138, 32, 32, 0.75f, FLinearColor::Black);

			FText Title = bIsOnDefense ? DefendTitle : AttackTitle;
			float TextYPos = LeftCorner.Y + 16.f*RenderScale;
			FFormatNamedArguments Args;
			Args.Add("RoundNum", FText::AsNumber(bIsAfterFirstRound ? GS->CTFRound+1 : 1));
			Args.Add("NumRounds", FText::AsNumber(GS->NumRounds));
			FText FormattedTitle = FText::Format(Title, Args);
			DrawText(FormattedTitle, MinimapCenter.X*Canvas->ClipX, TextYPos, UTHUDOwner->MediumFont, RenderScale, 1.0f, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Center);
			float TextXPos = MinimapCenter.X*Canvas->ClipX - 0.45f*MapSize;
			TextYPos += 48.f*RenderScale;

			if (!bIsAfterFirstRound)
			{
				int32 DisplayedParagraphs = 9 - int32(EndIntermissionTime - GetWorld()->GetTimeSeconds());
				int32 CountedParagraphs = 0;
				int32 LastParStart = 0;
				bool bFullList = true;
				for (int32 LineIndex = 0; LineIndex < NumLines; LineIndex++)
				{
					FText NextLine = bIsOnDefense ? DefendLines[LineIndex] : AttackLines[LineIndex];
					if (NextLine.IsEmpty())
					{
						CountedParagraphs++;
						if (CountedParagraphs >= DisplayedParagraphs)
						{
							NumLines = LineIndex;
							bFullList = false;
							break;
						}
						LastParStart = NumLines;
					}
				}
				if ((DisplayedParagraphs != OldDisplayedParagraphs) && !bFullListPlayed)
				{
					// play beep
					UTHUDOwner->UTPlayerOwner->ClientPlaySound(LineDisplaySound);
					bFullListPlayed = bFullList;
				}
				OldDisplayedParagraphs = DisplayedParagraphs;
				for (int32 LineIndex = 0; LineIndex < NumLines; LineIndex++)
				{
					FText NextLine = bIsOnDefense ? DefendLines[LineIndex] : AttackLines[LineIndex];
					DrawText(NextLine, TextXPos, TextYPos, UTHUDOwner->SmallFont, RenderScale, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
					TextYPos += 32.f*RenderScale;
				}
			}
		}
		else if (GS->GetScoringPlays().Num() > 0)
		{
			// draw scoring plays - FIXMESTEVE show summary always
			float ScoreWidth = 0.92f*MapSize;
			float PageBottom = LeftCorner.Y + 0.5f*Canvas->ClipY;
			if (GS && (GS->CTFRound < 4))
			{
				PageBottom -= (GS->CTFRound < 2) ? 0.1f*Canvas->ClipY : 0.05f*Canvas->ClipY;
			}
			float MaxHeight = 0.45f*Canvas->ClipY;
			FLinearColor PageColor = FLinearColor::Black;
			PageColor.A = 0.5f;
			float XOffset = LeftCorner.X + 0.04f*MapSize;
			float YPos = LeftCorner.Y;
			DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset, YPos, ScoreWidth, MaxHeight, 149, 138, 32, 32, 0.5f, PageColor);
			DrawScoringPlays(RenderDelta, YPos, XOffset, 0.9f*ScoreWidth, PageBottom);
		}
	}
	else 
	{
		Super::DrawMinimap(RenderDelta);
	}
}

void UUTFlagRunScoreboard::DrawGamePanel(float RenderDelta, float& YOffset)
{
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	if (!GS || ((GS->GetMatchState() != MatchState::MatchIntermission) && !GS->HasMatchEnded()))
	{
		Super::DrawGamePanel(RenderDelta, YOffset);
	}
	else
	{
		YOffset -= 8.f*RenderScale;
	}
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
			AUTFlagRunGameState* CTFState = Cast<AUTFlagRunGameState>(UTGameState);
			if (CTFState && ((CTFState->bRedToCap == (i==0)) ? CTFState->bAttackerLivesLimited : CTFState->bDefenderLivesLimited) && (!CTFState->IsMatchIntermission() || (CTFState->OffenseKills > 0) || (CTFState->DefenseKills > 0)))
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
	if (PlayerState)
	{
		FText TotalKills = FText::AsNumber(PlayerState->RoundKillAssists + PlayerState->RoundKills);
		DrawText(TotalKills, XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnY, UTHUDOwner->SmallFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
		float XL, YL;
		Canvas->TextSize(UTHUDOwner->SmallFont, TotalKills.ToString(), XL, YL, RenderScale, RenderScale);
		FFormatNamedArguments Args;
		Args.Add("Kills", FText::AsNumber(PlayerState->RoundKills));
		FText CurrentScoreText = FText::Format(NSLOCTEXT("UTFlagRun", "PlayerScoreText", "({Kills})"), Args);
		DrawText(CurrentScoreText, XOffset + (Width * ColumnHeaderScoreX) + XL + 8.f*RenderScale, YOffset + ColumnY, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	}
	AUTFlagRunGameState* CTFState = Cast<AUTFlagRunGameState>(UTGameState);
	if (CTFState)
	{
		AUTInventory* SelectedPowerup = PlayerState && PlayerState->BoostClass ? PlayerState->BoostClass->GetDefaultObject<AUTInventory>() : nullptr;
		const float LivesXOffset = (Width * 0.5f*(ColumnHeaderPowerupX + ColumnHeaderPingX));

		//Only display powerups from your team
		if (SelectedPowerup && ShouldShowPowerupForPlayer(PlayerState))
		{
			const float U = SelectedPowerup->HUDIcon.U;
			const float V = SelectedPowerup->HUDIcon.V;
			const float UL = SelectedPowerup->HUDIcon.UL;
			const float VL = SelectedPowerup->HUDIcon.VL;
				
			const float AspectRatio = UL / VL;
			const float TextureSize = (Width * (ColumnHeaderPowerupEndX - ColumnHeaderPowerupX));

			//Draw a Red Dash if they have used their powerup, or an icon if they have not yet used it
			const bool bIsAbleToEarnMorePowerups = (CTFState->IsTeamOnOffense(PlayerState->GetTeamNum()) ? CTFState->bIsOffenseAbleToGainPowerup : CTFState->bIsDefenseAbleToGainPowerup);
				
			//Draw Dash
			if (!bIsAbleToEarnMorePowerups && !CTFState->IsMatchIntermission() && (PlayerState->GetRemainingBoosts() <= 0))
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
		else
		{
			//DrawText(NSLOCTEXT("UTScoreboard", "Dash", "-"), XOffset + (Width * ColumnHeaderPowerupX), YOffset + ColumnY, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
		}

		if (PlayerState->bHasLifeLimit && (!CTFState->IsMatchIntermission() || (CTFState->OffenseKills > 0) || (CTFState->DefenseKills > 0)))
		{
			if (PlayerState->RemainingLives > 0)
			{
				DrawText(FText::AsNumber(PlayerState->RemainingLives), XOffset + LivesXOffset, YOffset + ColumnY, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
			}
			else
			{
				// draw skull here
				DrawTexture(UTHUDOwner->HUDAtlas, XOffset + LivesXOffset - 0.5f*CellHeight*RenderScale, YOffset + ColumnY - 0.34f*CellHeight*RenderScale, 0.75f*CellHeight*RenderScale, 0.75f*CellHeight*RenderScale, 725, 0, 28, 36, 1.f, FLinearColor::White);
			}
		}
	}
}

bool UUTFlagRunScoreboard::ShouldShowPowerupForPlayer(AUTPlayerState* PlayerState)
{
	return (UTGameState->IsMatchInProgress() && !UTGameState->IsMatchIntermission()) || (UTPlayerOwner->GetTeamNum() == PlayerState->GetTeamNum());
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

void UUTFlagRunScoreboard::DrawTeamStats(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight, const FStatsFontInfo& StatsFontInfo)
{
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
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "LargeArmorPickups", "Large Armor Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ArmorVestCount), UTGameState->Teams[1]->GetStatsValue(NAME_ArmorVestCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "MediumArmorPickups", "Medium Armor Pickups"), UTGameState->Teams[0]->GetStatsValue(NAME_ArmorPadsCount), UTGameState->Teams[1]->GetStatsValue(NAME_ArmorPadsCount), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);

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
			DrawStatsLine(NSLOCTEXT("UTScoreboard", "SmallArmorPickups", "Small Armor Pickups"), TeamStat0, TeamStat1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
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

bool UUTFlagRunScoreboard::ShouldDrawScoringStats()
{
	AUTCTFGameState* CTFState = Cast<AUTCTFGameState>(UTGameState);
	return Super::ShouldDrawScoringStats() && CTFState && (CTFState->GetScoringPlays().Num() > 0);
}

void UUTFlagRunScoreboard::DrawStatsRight(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom)
{
}

void UUTFlagRunScoreboard::DrawStatsLeft(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom)
{
}

void UUTFlagRunScoreboard::DrawScoringSummary(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight)
{
	if (TimeLineOffset < 1.f)
	{
		return;
	}
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	if (!GS || GS->HasMatchEnded() || (GS->Teams.Num() < 2) || !GS->Teams[0] || !GS->Teams[1])
	{
		return;
	}
	FFontRenderInfo TextRenderInfo;
	TextRenderInfo.bEnableShadow = true;
	TextRenderInfo.bClipText = true;
	bool bDrawBlueFirst = false;
	FText ScorePreamble = NSLOCTEXT("UTFlagRun", "ScoreTied", "Score Tied ");
	if (GS->Teams[0]->Score > GS->Teams[1]->Score)
	{
		ScorePreamble = NSLOCTEXT("UTFlagRun", "RedLeads", "Red Team leads  ");
	}
	else if (GS->Teams[1]->Score > GS->Teams[0]->Score)
	{
		ScorePreamble = NSLOCTEXT("UTFlagRun", "BlueLeads", "Blue Team leads  ");
		bDrawBlueFirst = true;
	}
	FFormatNamedArguments Args;
	Args.Add("Preamble", ScorePreamble);
	Args.Add("RedScore", FText::AsNumber(GS->Teams[0]->Score));
	Args.Add("BlueScore", FText::AsNumber(GS->Teams[1]->Score));

	FText CurrentScoreText = FText::Format(NSLOCTEXT("UTFlagRun", "ScoreText", "{Preamble} {RedScore} - {BlueScore}"), Args);
	float ScoringOffsetX, ScoringOffsetY;
	Canvas->TextSize(UTHUDOwner->MediumFont, CurrentScoreText.ToString(), ScoringOffsetX, ScoringOffsetY, RenderScale, RenderScale);

	FLinearColor DrawColor = FLinearColor::White;
	float CurrentScoreHeight = (GS->CTFRound >= GS->NumRounds - 2) ? 3.f*ScoringOffsetY : 2.f*ScoringOffsetY;
	Canvas->SetLinearDrawColor(FLinearColor::Black);
	float FrameWidth = 8.f * RenderScale;
	DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset - FrameWidth, YPos - FrameWidth, 1.11f*ScoreWidth + 2.f*FrameWidth, CurrentScoreHeight+2.f*FrameWidth, 149, 138, 32, 32, 0.75f, FLinearColor::Black);
	Canvas->SetLinearDrawColor(FLinearColor::White);
	float BackAlpha = 0.3f;
	DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset, YPos, 1.11f*ScoreWidth, CurrentScoreHeight, 149, 138, 32, 32, BackAlpha, DrawColor);

	float SingleXL, SingleYL;
	float ScoreX = XOffset + 0.5f*(ScoreWidth - ScoringOffsetX);
	FString PreambleString = ScorePreamble.ToString();
	Canvas->TextSize(UTHUDOwner->MediumFont, PreambleString, SingleXL, SingleYL, RenderScale, RenderScale);
	YPos -= 0.1f * SingleYL;
	Canvas->DrawText(UTHUDOwner->MediumFont, PreambleString, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
	Canvas->SetLinearDrawColor(FLinearColor::White);
	ScoreX += SingleXL;

	int32 TeamIndex = bDrawBlueFirst ? 1 : 0;
	Canvas->SetLinearDrawColor(GS->Teams[TeamIndex]->TeamColor);
	FString SingleScorePart = FString::Printf(TEXT("%i"), GS->Teams[TeamIndex]->Score);
	Canvas->TextSize(UTHUDOwner->MediumFont, SingleScorePart, SingleXL, SingleYL, RenderScale, RenderScale);
	Canvas->DrawText(UTHUDOwner->MediumFont, SingleScorePart, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
	Canvas->SetLinearDrawColor(FLinearColor::White);
	ScoreX += SingleXL;
	Canvas->TextSize(UTHUDOwner->MediumFont, " - ", SingleXL, SingleYL, RenderScale, RenderScale);
	Canvas->DrawText(UTHUDOwner->MediumFont, " - ", ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
	ScoreX += SingleXL;
	TeamIndex = bDrawBlueFirst ? 0 : 1;
	Canvas->SetLinearDrawColor(GS->Teams[TeamIndex]->TeamColor);
	SingleScorePart = FString::Printf(TEXT("%i"), GS->Teams[TeamIndex]->Score);
	Canvas->DrawText(UTHUDOwner->MediumFont, SingleScorePart, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
	YPos += (GS->CTFRound == GS->NumRounds - 2) ? 0.9f*SingleYL : 0.8f*SingleYL;

	bool bRedLeadsTiebreak = (GS->TiebreakValue > 0);
	FText TiebreakPreamble = NSLOCTEXT("UTFlagRun", "TiebreakPreamble", "Tiebreak ");
	Args.Add("TBPre", TiebreakPreamble);
	Args.Add("Bonus", FText::AsNumber(FMath::Abs(GS->TiebreakValue)));
	Args.Add("Team", bRedLeadsTiebreak ? RedTeamText : BlueTeamText);
	FText TiebreakBonusPattern = NSLOCTEXT("UTFlagRun", "TiebreakPattern", "{TBPre}{Team} +{Bonus}");
	FText TiebreakBonusText = FText::Format(TiebreakBonusPattern, Args);
	Canvas->TextSize(UTHUDOwner->SmallFont, TiebreakBonusText.ToString(), ScoringOffsetX, ScoringOffsetY, RenderScale, RenderScale);
	ScoreX = XOffset + 0.5f*(ScoreWidth - ScoringOffsetX);

	Canvas->SetLinearDrawColor(FLinearColor::White);
	PreambleString = TiebreakPreamble.ToString();
	Canvas->TextSize(UTHUDOwner->SmallFont, PreambleString, SingleXL, SingleYL, RenderScale, RenderScale);
	Canvas->DrawText(UTHUDOwner->SmallFont, PreambleString, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
	ScoreX += SingleXL;

	Canvas->SetLinearDrawColor(bRedLeadsTiebreak ? GS->Teams[0]->TeamColor : GS->Teams[1]->TeamColor);
	FString TeamString = bRedLeadsTiebreak ? RedTeamText.ToString() : BlueTeamText.ToString();
	Canvas->TextSize(UTHUDOwner->SmallFont, TeamString, SingleXL, SingleYL, RenderScale, RenderScale);
	Canvas->DrawText(UTHUDOwner->SmallFont, TeamString, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
	ScoreX += SingleXL;

	Canvas->SetLinearDrawColor(FLinearColor::White);
	FText TiebreakValue = FText::Format(NSLOCTEXT("UTFlagRun", "TiebreakValue", " +{Bonus}"), Args);
	Canvas->DrawText(UTHUDOwner->SmallFont, TiebreakValue, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);

	YPos += (GS->CTFRound == GS->NumRounds - 2) ? 0.9f*SingleYL : 0.8f*SingleYL;

	AUTFlagRunHUD* FRHUD = Cast<AUTFlagRunHUD>(UTHUDOwner);
	if (FRHUD)
	{
		FRHUD->DrawWinConditions(UTHUDOwner->SmallFont, XOffset, YPos, ScoreWidth, RenderScale, true);
	}
}

int32 UUTFlagRunScoreboard::GetSmallPlaysCount(int32 NumPlays) const
{
	return  (NumPlays > 6) ? NumPlays : 0;
}

void UUTFlagRunScoreboard::DrawScoringPlayInfo(const FCTFScoringPlay& Play, float CurrentScoreHeight, float SmallYL, float MedYL, float DeltaTime, float& YPos, float XOffset, float ScoreWidth, FFontRenderInfo TextRenderInfo, bool bIsSmallPlay)
{
	AUTCTFGameState* CTFState = Cast<AUTCTFGameState>(UTGameState);
	float BoxYPos = YPos;

	// draw scoring team icon
	int32 IconIndex = Play.Team->TeamIndex;
	IconIndex = FMath::Min(IconIndex, 1);
	float IconHeight = 0.9f*CurrentScoreHeight;
	float IconOffset = 0.13f*ScoreWidth;
	DrawTexture(UTHUDOwner->HUDAtlas, XOffset + IconOffset, YPos + 0.1f*CurrentScoreHeight, IconHeight, IconHeight, UTHUDOwner->TeamIconUV[IconIndex].X, UTHUDOwner->TeamIconUV[IconIndex].Y, 72, 72, 1.f, Play.Team->TeamColor);

	FString ScoredByLine;
	if (Play.bDefenseWon)
	{
		ScoredByLine = FString::Printf(TEXT("Defense Won"));
	}
	else if (Play.bAnnihilation)
	{
		ScoredByLine = FString::Printf(TEXT("Annihilation"));
	}
	else
	{
		ScoredByLine = Play.ScoredBy.GetPlayerName();
	}

	// time of game
	FString TimeStampLine = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), Play.RemainingTime, false, true, false).ToString();
	Canvas->SetLinearDrawColor(FLinearColor::White);
	Canvas->DrawText(UTHUDOwner->SmallFont, TimeStampLine, XOffset + 0.02f*ScoreWidth, YPos + 0.5f*CurrentScoreHeight - 0.5f*SmallYL, RenderScale, RenderScale, TextRenderInfo);

	// scored by
	Canvas->SetLinearDrawColor(Play.Team->TeamColor);
	float NameSizeX, NameSizeY;
	Canvas->TextSize(UTHUDOwner->MediumFont, ScoredByLine, NameSizeX, NameSizeY, RenderScale, RenderScale);
	Canvas->DrawText(UTHUDOwner->MediumFont, ScoredByLine, XOffset + 0.32f*ScoreWidth, YPos + 0.5f*CurrentScoreHeight - 0.6f*MedYL, RenderScale * FMath::Min(1.f, ScoreWidth*0.35f / FMath::Max(NameSizeX, 1.f)), RenderScale, TextRenderInfo);

	int32 RoundBonus = FMath::Max(Play.RedBonus, Play.BlueBonus);
	FString BonusString = TEXT("\u2605");
	FLinearColor BonusColor = FLinearColor(0.48f, 0.25f, 0.18f);
	if ((RoundBonus > 0) && !Play.bDefenseWon)
	{
		if (RoundBonus >= 60)
		{
			BonusString = (RoundBonus >= 120) ? TEXT("\u2605 \u2605 \u2605") : TEXT("\u2605 \u2605");
			BonusColor = (RoundBonus >= 120) ? FLinearColor(1.f, 0.9f, 0.15f) : FLinearColor(0.7f, 0.7f, 0.75f);
		}
	}
	float ScoringOffsetX, ScoringOffsetY;
	Canvas->TextSize(UTHUDOwner->MediumFont, BonusString, ScoringOffsetX, ScoringOffsetY, RenderScale, RenderScale);
	float ScoreX = XOffset + ScoreWidth - ScoringOffsetX;

	Canvas->SetLinearDrawColor(BonusColor);
	Canvas->DrawText(UTHUDOwner->MediumFont, BonusString, ScoreX, YPos + 0.5f*CurrentScoreHeight - 0.6f*MedYL, RenderScale, RenderScale, TextRenderInfo);

	if ((RoundBonus > 0) && !Play.bDefenseWon && !bIsSmallPlay)
	{
		YPos += 0.82f* MedYL;
		int32 NetBonus = RoundBonus;
		if (NetBonus == 180)
		{
			NetBonus = 60;
		}
		else
		{
			while (NetBonus > 59)
			{
				NetBonus -= 60;
			}
		}
		FString BonusLine = FString::Printf(TEXT("Bonus Time: %d"), NetBonus);
		Canvas->TextSize(UTHUDOwner->MediumFont, BonusLine, ScoringOffsetX, ScoringOffsetY, RenderScale, RenderScale);
		ScoreX = XOffset + ScoreWidth - 0.5f*ScoringOffsetX;
		Canvas->SetLinearDrawColor(FLinearColor::White);
		Canvas->DrawText(UTHUDOwner->TinyFont, BonusLine, ScoreX, YPos, 0.75f*RenderScale, RenderScale, TextRenderInfo);
	}
}

