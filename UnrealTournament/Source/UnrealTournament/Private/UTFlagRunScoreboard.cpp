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
#include "UTCountDownMessage.h"
#include "Engine/DemoNetDriver.h"
#include "UTCTFRewardMessage.h"
#include "UTCTFGameMessage.h"
#include "UTShowdownGameMessage.h"
#include "UTDemoRecSpectator.h"
#include "UTAnnouncer.h"
#include "UTLineUpHelper.h"
#include "UTVictoryMessage.h"

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
	DefendLines.Add(NSLOCTEXT("UTScoreboard", "DefenseLine2", "* You have 5 lives.  The attackers do not have a life limit."));
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
	AttackLines.Add(NSLOCTEXT("UTScoreboard", "AttackLine2", "* Defenders have 5 lives.  You do not have a life limit."));
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

	static ConstructorHelpers::FObjectFinder<USoundBase> StarPound(TEXT("SoundCue'/Game/RestrictedAssets/Audio/Gameplay/A_Gameplay_JumpPadJump01.A_Gameplay_JumpPadJump01'"));
	StarPoundSound = StarPound.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> StarWoosh(TEXT("SoundCue'/Game/RestrictedAssets/Audio/UI/StarWoosh_Cue.StarWoosh_Cue'"));
	StarWooshSound = StarWoosh.Object;

	BlueTeamName = NSLOCTEXT("CTFRewardMessage", "BlueTeamName", "BLUE TEAM");
	RedTeamName = NSLOCTEXT("CTFRewardMessage", "RedTeamName", "RED TEAM");
	TeamScorePrefix = NSLOCTEXT("CTFRewardMessage", "TeamScorePrefix", "");
	TeamScorePostfix = NSLOCTEXT("CTFRewardMessage", "TeamScorePostfix", " Scores!");
	TeamWinsPrefix = NSLOCTEXT("UTVictoryMessage", "TeamWinsPrefix", "");
	TeamWinsPostfix = NSLOCTEXT("UTVictoryMessage", "TeamWinsPostfix", " Wins The Match!");
	StarText = NSLOCTEXT("CTFRewardMessage", "StarText", "\u2605");
	DefenseScorePrefix = NSLOCTEXT("CTFRewardMessage", "DefenseScoreBonusPrefix", "");
	DefenseScorePostfix = NSLOCTEXT("CTFRewardMessage", "DefenseScoreBonusPostfix", " Successfully Defends!");
	DeliveredPrefix = NSLOCTEXT("FlagRunScoreboard", "DeliveredBy", "Delivered by ");
	ScoreInfoDuration = 8.f;
	PendingTiebreak = 0;
	bHasAnnouncedWin = false;
}

bool UUTFlagRunScoreboard::IsBeforeFirstRound()
{
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	return !GS || ((GS->GetScoringPlays().Num() == 0) && (!GS->HasMatchStarted() || GS->IsMatchIntermission()));
}

bool UUTFlagRunScoreboard::ShowScorePanel()
{
	if (UTHUDOwner && UTHUDOwner->bShowScores)
	{
		return true;
	}
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	AUTFlagRunGame* DefaultGame = GS && GS->GameModeClass ? GS->GameModeClass->GetDefaultObject<AUTFlagRunGame>() : nullptr;
	if (!DefaultGame || (GS->GetMatchState() == MatchState::CountdownToBegin) || (GS->GetMatchState() == MatchState::PlayerIntro))
	{
		return false;
	}
	return (GS->IsMatchIntermission() || GS->HasMatchEnded()) ? (!IsBeforeFirstRound() && (GetWorld()->GetTimeSeconds() - ScoreReceivedTime > ScoreInfoDuration) && (EndIntermissionTime < GetWorld()->GetTimeSeconds())) : true;
}

bool UUTFlagRunScoreboard::ShowScoringInfo()
{
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	AUTFlagRunGame* DefaultGame = GS && GS->GameModeClass ? GS->GameModeClass->GetDefaultObject<AUTFlagRunGame>() : nullptr;
	if (!DefaultGame)
	{
		return false;
	}
	return (GS->IsMatchIntermission() || GS->HasMatchEnded()) && !IsBeforeFirstRound() && (GetWorld()->GetTimeSeconds() - ScoreReceivedTime < ScoreInfoDuration);
}

void UUTFlagRunScoreboard::DrawTeamPanel(float RenderDelta, float& YOffset)
{
	if (UTGameState == NULL || UTGameState->Teams.Num() < 2 || UTGameState->Teams[0] == NULL || UTGameState->Teams[1] == NULL) return;

	int32 RealScore = 0;
	if (ScoringTeam)
	{
		RealScore = ScoringTeam->Score;
		ScoringTeam->Score -= PendingScore;
	}
	Super::DrawTeamPanel(RenderDelta, YOffset);
	if (ScoringTeam)
	{
		ScoringTeam->Score = RealScore;
	}

	// Tiebreak display
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	if (GS && (GS->Teams.Num() >1) && GS->Teams[0] && GS->Teams[1] && ((GS->TiebreakValue != 0) || (PendingTiebreak != 0)))
	{
		float Width = 0.125f*Canvas->ClipX;
		float Height = Width * 21.f / 150.f;
		float BackgroundY = YOffset - 2.f*Height;
		int32 CurrentTiebreakValue = GS->TiebreakValue - PendingTiebreak;
		float ChargePct = Width*FMath::Clamp(CurrentTiebreakValue, -100, 100) / 200.f;
		FLinearColor TiebreakColor = (CurrentTiebreakValue > 0) ? GS->Teams[0]->TeamColor : FLinearColor::Blue; 
		float ChargeOffset = (ChargePct < 0.f) ? 0.f : ChargePct;
		DrawTexture(UTHUDOwner->HUDAtlas, 0.5f*Canvas->ClipX - ChargeOffset, BackgroundY, FMath::Abs(ChargePct), Height, 127.f, 641, 150.f, 21.f, 1.f, TiebreakColor, FVector2D(0.f, 0.5f));

		DrawTexture(UTHUDOwner->HUDAtlas, 0.5f*Canvas->ClipX, BackgroundY, Width, Height, 127, 612, 150, 21.f, 1.f, FLinearColor::White, FVector2D(0.5f, 0.5f));

		DrawText(NSLOCTEXT("FlagRun", "Tiebreak", "TIEBREAKER"), 0.5f*Canvas->ClipX, BackgroundY + Height, UTHUDOwner->TinyFont, FVector2D(1.f,1.f), FLinearColor::Black, FLinearColor::Black, RenderScale, 1.f, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Center);

		if (CurrentTiebreakValue != 0)
		{
			FFormatNamedArguments Args;
			FText Title = (CurrentTiebreakValue > 0) ? NSLOCTEXT("FlagRun", "RedTiebreak", "RED +{Score}") : NSLOCTEXT("FlagRun", "BlueTiebreak", "BLUE +{Score}");
			Args.Add("Score", FText::AsNumber(FMath::Abs(CurrentTiebreakValue)));
			FText FormattedTitle = FText::Format(Title, Args);
			DrawText(FormattedTitle, 0.5f*Canvas->ClipX, BackgroundY - Height - 2.f*RenderScale, UTHUDOwner->TinyFont, FVector2D(1.f, 1.f), FLinearColor::Black, FLinearColor::Black, RenderScale, 1.f, TiebreakColor, ETextHorzPos::Center, ETextVertPos::Center);
		}
	}
}

void UUTFlagRunScoreboard::AnnounceRoundScore(AUTTeamInfo* InScoringTeam, APlayerState* InScoringPlayer, uint8 InRoundBonus, uint8 InReason)
{
	ScoringTeam = InScoringTeam;
	ScoringPlayer = InScoringPlayer;
	RoundBonus = InRoundBonus;
	Reason = InReason;
	ScoreReceivedTime = GetWorld()->GetTimeSeconds();
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	AUTFlagRunGame* DefaultGame = GS && GS->GameModeClass ? GS->GameModeClass->GetDefaultObject<AUTFlagRunGame>() : nullptr;
	PendingScore = 1;
	PendingTiebreak = 0;
	if (DefaultGame && ScoringTeam)
	{
		if (RoundBonus >= DefaultGame->GoldBonusTime)
		{
			PendingScore = 3;
			PendingTiebreak = FMath::Min(RoundBonus - DefaultGame->GoldBonusTime, 60);
		}
		else if (RoundBonus >= DefaultGame->SilverBonusTime)
		{
			PendingScore = 2;
			PendingTiebreak = FMath::Min(RoundBonus - DefaultGame->SilverBonusTime, 59);
		}
		else if (RoundBonus > 0)
		{
			PendingTiebreak = FMath::Min(int32(RoundBonus), 59);
		}
		if (ScoringTeam->TeamIndex == 1)
		{
			PendingTiebreak *= -1;
		}
	}
	if (UTPlayerOwner)
	{
		if (UTHUDOwner && UTHUDOwner->AnnouncementWidget)
		{
			UTHUDOwner->AnnouncementWidget->AgeMessages(1000.f);
		}
		if (UTPlayerOwner->Announcer)
		{
			UTPlayerOwner->Announcer->ClearAnnouncements();
		}
		if (Reason == 0)
		{
			UTPlayerOwner->ClientReceiveLocalizedMessage(UUTCTFGameMessage::StaticClass(), 2, ScoringPlayer, nullptr, ScoringTeam);
		}
		else if (ScoringTeam)
		{
			UTPlayerOwner->ClientReceiveLocalizedMessage(UUTShowdownGameMessage::StaticClass(), 3 + ScoringTeam->TeamIndex);
		}
	}
}

void UUTFlagRunScoreboard::DrawFramedBackground(float XOffset, float YOffset, float Width, float Height)
{
	float FrameWidth = 8.f * RenderScale;
	Canvas->SetLinearDrawColor(FLinearColor::Black);
	DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset - FrameWidth, YOffset - FrameWidth, Width + 2.f*FrameWidth, Height + 2.f*FrameWidth, 149, 138, 32, 32, 0.75f, FLinearColor::Black);
	Canvas->SetLinearDrawColor(FLinearColor::White);
	float BackAlpha = 0.3f;
	DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset, YOffset, Width, Height, 149, 138, 32, 32, BackAlpha, FLinearColor::White);
}

float UUTFlagRunScoreboard::DrawWinAnnouncement(float DeltaTime, UFont* InFont)
{
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	if (!GS || !GS->WinningTeam)
	{
		return 0.f;
	}
	if (!bHasAnnouncedWin && UTPlayerOwner && UTPlayerOwner->UTPlayerState)
	{
		UTPlayerOwner->ClientReceiveLocalizedMessage(UUTVictoryMessage::StaticClass(), 0, GS->WinnerPlayerState, UTPlayerOwner->UTPlayerState, GS->WinningTeam);
		bHasAnnouncedWin = true;
	}

	FText EmphasisText = (GS->WinningTeam && (GS->WinningTeam->TeamIndex == 0)) ? RedTeamName : BlueTeamName;
	float YL, EmphasisXL;
	Canvas->StrLen(InFont, EmphasisText.ToString(), EmphasisXL, YL);
	float YPos = (LastScorePanelYOffset > 0.f) ? LastScorePanelYOffset - 2.f : 0.25f*Canvas->ClipY;

	float PostXL;
	Canvas->StrLen(InFont, TeamWinsPostfix.ToString(), PostXL, YL);

	float ScoreWidth = RenderScale * (1.2f*(PostXL + EmphasisXL));
	float ScoreHeight = 1.2f*YL;
	float XOffset = 0.5f*(Canvas->ClipX - ScoreWidth);
	DrawFramedBackground(XOffset, YPos, ScoreWidth, RenderScale * ScoreHeight);

	// Draw winning team string
	FFontRenderInfo TextRenderInfo;
	TextRenderInfo.bEnableShadow = true;
	TextRenderInfo.bClipText = true;
	FLinearColor EmphasisColor = (GS->WinningTeam && (GS->WinningTeam->TeamIndex == 0)) ? FLinearColor::Red : FLinearColor::Blue;
	float ScoreX = 0.5f * (Canvas->ClipX - RenderScale * (EmphasisXL + PostXL));

	Canvas->SetLinearDrawColor(GS->WinningTeam->TeamColor);
	Canvas->DrawText(InFont, EmphasisText, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
	ScoreX += EmphasisXL*RenderScale;

	Canvas->SetLinearDrawColor(FLinearColor::White);
	Canvas->DrawText(InFont, TeamWinsPostfix, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
	return RenderScale*ScoreHeight;
}


void UUTFlagRunScoreboard::DrawScoreAnnouncement(float DeltaTime)
{
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	AUTFlagRunGame* DefaultGame = GS && GS->GameModeClass ? GS->GameModeClass->GetDefaultObject<AUTFlagRunGame>() : nullptr;
	if (!DefaultGame || !ScoringTeam)
	{
		return;
	}

	float PoundStart = 1.6f;
	float PoundInterval = 0.5f;
	float WooshStart = 4.1f;
	float WooshTime = 0.3f;
	float WooshInterval = 0.5f;
	float CurrentTime = GetWorld()->GetTimeSeconds() - ScoreReceivedTime;

	int32 NumStars = 1;
	FLinearColor BonusColor = GS->BronzeBonusColor;
	if (Reason == 0)
	{
		if (RoundBonus >= DefaultGame->GoldBonusTime)
		{
			NumStars = 3;
			BonusColor = GS->GoldBonusColor;
		}
		else if (RoundBonus >= DefaultGame->SilverBonusTime)
		{
			NumStars = 2;
			BonusColor = GS->SilverBonusColor;
		}
	}

	if (GS && GS->HasMatchEnded() && (CurrentTime >= WooshStart + NumStars*WooshInterval + WooshTime + 1.f + FMath::Min(FMath::Abs(float(PendingTiebreak)), 20.f) * 0.05f + FMath::Max(0.f, FMath::Abs(float(PendingTiebreak)) - 20.f) * 0.025f))
	{
		DrawWinAnnouncement(DeltaTime, UTHUDOwner->HugeFont);
		return;
	}
	UFont* InFont = UTHUDOwner->LargeFont;
	FText EmphasisText = (ScoringTeam && (ScoringTeam->TeamIndex == 0)) ? RedTeamName : BlueTeamName;
	float YL, EmphasisXL;
	Canvas->StrLen(InFont, EmphasisText.ToString(), EmphasisXL, YL);
	float YPos = (LastScorePanelYOffset > 0.f) ? LastScorePanelYOffset - 2.f : 0.25f*Canvas->ClipY;
	float StarXL, StarYL;
	UFont* StarFont = UTHUDOwner->HugeFont;
	Canvas->StrLen(StarFont, StarText.ToString(), StarXL, StarYL);

	FText ScorePrefix = (Reason == 0) ? TeamScorePrefix : DefenseScorePrefix;
	FText ScorePostfix = (Reason == 0) ? TeamScorePostfix : DefenseScorePostfix;
	float PostXL;
	Canvas->StrLen(InFont, ScorePostfix.ToString(), PostXL, YL);

	float ScoreWidth = RenderScale * FMath::Max(6.f*StarXL, 1.2f*(PostXL+EmphasisXL));
	float ScoreHeight = ScoringPlayer ? StarYL + 1.7f*YL : StarYL + YL;
	float XOffset = 0.5f*(Canvas->ClipX - ScoreWidth);
	DrawFramedBackground(XOffset, YPos, ScoreWidth, RenderScale * ScoreHeight);

	// Draw scoring team string
	FFontRenderInfo TextRenderInfo;
	TextRenderInfo.bEnableShadow = true;
	TextRenderInfo.bClipText = true;
	FLinearColor EmphasisColor = (ScoringTeam && (ScoringTeam->TeamIndex == 0)) ? FLinearColor::Red : FLinearColor::Blue;
	float ScoreX = 0.5f * (Canvas->ClipX - RenderScale * (EmphasisXL + PostXL));

	Canvas->SetLinearDrawColor(ScoringTeam->TeamColor);
	Canvas->DrawText(InFont, EmphasisText, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
	ScoreX += EmphasisXL*RenderScale;

	Canvas->SetLinearDrawColor(FLinearColor::White);
	Canvas->DrawText(InFont, ScorePostfix, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);

	YPos += YL*RenderScale;

	ScoreX = 0.5f * (Canvas->ClipX - 1.5f * RenderScale * StarXL * (float(NumStars) - 0.5f));
	Canvas->SetLinearDrawColor(BonusColor);

	// Draw stars
	for (int32 i = 0; i < NumStars; i++)
	{
		if (CurrentTime >= PoundStart + i*PoundInterval)
		{
			if (CurrentTime - DeltaTime < PoundStart + i*PoundInterval)
			{
				UTHUDOwner->UTPlayerOwner->ClientPlaySound(StarPoundSound);
			}
			float StarXPos = ScoreX;
			float StarYPos = YPos - 0.31f*YL*RenderScale;
			if (CurrentTime >= WooshStart + i*WooshInterval)
			{
				if (CurrentTime - DeltaTime < WooshStart + i*WooshInterval)
				{
					UTHUDOwner->UTPlayerOwner->ClientPlaySound(StarWooshSound);
				}
				else if ((CurrentTime >= WooshStart + i*WooshInterval + WooshTime) && (CurrentTime - DeltaTime < WooshStart + i*WooshInterval + WooshTime))
				{
					PendingScore--;
					if (ScoringTeam->TeamIndex == 0)
					{
						RedScoreScaling = 1.5f;
					}
					else
					{
						BlueScoreScaling = 1.5f;
					}
				}
				float WooshPct = FMath::Clamp((CurrentTime - (WooshStart + i*WooshInterval)) / WooshTime, 0.f, 1.f);
				float XOffsetDest = (ScoringTeam->TeamIndex == 0) ? 0.37f * Canvas->ClipX : 0.60f * Canvas->ClipX;
				StarXPos = StarXPos + FMath::Min(1.1f*WooshPct, 1.f)*(XOffsetDest - StarXPos);
				StarYPos *= (1.f - WooshPct);
			}
			if (StarYPos > 0.05f*Canvas->ClipY)
			{
				float EffectDuration = 0.5f*PoundInterval;
				float StarEffectEndTime = PoundStart + i*PoundInterval + EffectDuration;
				if (CurrentTime < StarEffectEndTime)
				{
					TextRenderInfo.bEnableShadow = false;
					float StarEffectScale = 1.05f + 4.f*(EffectDuration - StarEffectEndTime + CurrentTime);
					BonusColor.A = 0.25f + StarEffectEndTime - CurrentTime;
					Canvas->SetLinearDrawColor(BonusColor);
					Canvas->DrawText(StarFont, StarText, StarXPos - 0.5f*RenderScale*(StarEffectScale - 1.f) * StarXL, StarYPos + 16.f*RenderScale - 0.5f*RenderScale*(StarEffectScale - 0.5f) * StarYL, StarEffectScale*RenderScale, StarEffectScale*RenderScale, TextRenderInfo);
				}
				else
				{
					TextRenderInfo.bEnableShadow = true;
				}
				BonusColor.A = 1.f;
				Canvas->SetLinearDrawColor(BonusColor);
				Canvas->DrawText(StarFont, StarText, StarXPos, StarYPos, RenderScale, RenderScale, TextRenderInfo);
			}
			ScoreX += 1.5f*StarXL*RenderScale;
		}
	}

	// Draw bonus time
	if (ScoringTeam && (CurrentTime >= PoundStart + NumStars*PoundInterval) && (PendingTiebreak != 0))
	{
		float BonusXL, BonusYL;
		UFont* BonusFont = UTHUDOwner->MediumFont;
		FFormatNamedArguments Args;
		Args.Add("Bonus", FText::AsNumber(FMath::Abs(PendingTiebreak)));
		FText BonusText = FText::Format(NSLOCTEXT("FlagRun", "BonusTime", "Bonus Time +{Bonus}"), Args);
		Canvas->StrLen(BonusFont, BonusText.ToString(), BonusXL, BonusYL);
		if (CurrentTime >= WooshStart + NumStars*WooshInterval)
		{
			float Interval = FMath::Max(float(FMath::Abs(PendingTiebreak)), 20.f);
			if (int32(Interval*(CurrentTime - DeltaTime)) != int32(Interval*CurrentTime))
			{
				UTHUDOwner->UTPlayerOwner->ClientPlaySound(LineDisplaySound);
				if (PendingTiebreak > 0)
				{
					PendingTiebreak--;
				}
				else if (PendingTiebreak < 0)
				{
					PendingTiebreak++;
				}
			}
			FVector LineEndPoint(0.5f*Canvas->ClipX, 0.06f*Canvas->ClipY, 0.f);
			FVector LineStartPoint(0.545f*Canvas->ClipX, YPos + 1.19f*YL*RenderScale, 0.f);
			FLinearColor LineColor = EmphasisColor;
			LineColor.A = 0.2f;
			FBatchedElements* BatchedElements = Canvas->Canvas->GetBatchedElements(FCanvas::ET_Line);
			FHitProxyId HitProxyId = Canvas->Canvas->GetHitProxyId();
			BatchedElements->AddTranslucentLine(LineEndPoint, LineStartPoint, LineColor, HitProxyId, 16.f);
		}
		Canvas->SetLinearDrawColor(FLinearColor::White);
		TextRenderInfo.bEnableShadow = true;
		Canvas->DrawText(BonusFont, BonusText, 0.5f*(Canvas->ClipX - RenderScale*BonusXL), YPos + 0.99f*YL*RenderScale, RenderScale, RenderScale, TextRenderInfo);
	}

	// Draw scoring player string
	if (ScoringPlayer)
	{
		YPos = YPos + StarYL*RenderScale;
		float DeliveredXL;
		Canvas->StrLen(UTHUDOwner->SmallFont, DeliveredPrefix.ToString(), DeliveredXL, YL);
		EmphasisText = FText::FromString(ScoringPlayer->PlayerName);
		Canvas->StrLen(UTHUDOwner->SmallFont, EmphasisText.ToString(), EmphasisXL, YL);

		ScoreX = 0.5f * (Canvas->ClipX - RenderScale * (EmphasisXL + DeliveredXL));
		Canvas->SetLinearDrawColor(FLinearColor::White);
		Canvas->DrawText(UTHUDOwner->SmallFont, DeliveredPrefix, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
		ScoreX += DeliveredXL*RenderScale;
		Canvas->SetLinearDrawColor(ScoringTeam->TeamColor);
		Canvas->DrawText(UTHUDOwner->SmallFont, EmphasisText, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
	}
}

void UUTFlagRunScoreboard::DrawScorePanel(float RenderDelta, float& YOffset)
{
	LastScorePanelYOffset = YOffset;
	if (ShowScoringInfo() && (!Cast<AUTDemoRecSpectator>(UTHUDOwner->UTPlayerOwner) || !((AUTDemoRecSpectator *)(UTHUDOwner->UTPlayerOwner))->IsKillcamSpectator()))
	{
		DrawScoreAnnouncement(RenderDelta);
	}
	if (ShowScorePanel())
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
		if (DefaultGame && (GetWorld()->GetTimeSeconds() - ScoreReceivedTime < ScoreInfoDuration))
		{
			bNeedReplay = !IsBeforeFirstRound();
			return;
		}
		if (bNeedReplay && GS->ScoringPlayerState)
		{
			bNeedReplay = false;
			if (GetWorld()->DemoNetDriver)
			{
				AUTCharacter* PawnToFocus = GS->ScoringPlayerState->GetUTCharacter();
				FNetworkGUID FocusPawnGuid = GetWorld()->DemoNetDriver->GetGUIDForActor(PawnToFocus);
				UTPlayerOwner->OnKillcamStart(FocusPawnGuid, 8.0f + ScoreInfoDuration);
				GetWorld()->GetTimerManager().SetTimer(
					UTPlayerOwner->KillcamStopHandle,
					FTimerDelegate::CreateUObject(UTPlayerOwner, &AUTPlayerController::ClientStopKillcam),
					8.2f,
					false);
			}
			return;
		}

		const float MapSize = FMath::Min(Canvas->ClipX - 2.f*ScaledEdgeSize - 2.f*ScaledCellWidth, 0.9f*Canvas->ClipY - 120.f * RenderScale);
		float MapYPos = (LastScorePanelYOffset > 0.f) ? LastScorePanelYOffset - 2.f : 0.25f*Canvas->ClipY;
		FVector2D LeftCorner = FVector2D(MinimapCenter.X*Canvas->ClipX - 0.5f*MapSize, MapYPos);
		if ((EndIntermissionTime < GetWorld()->GetTimeSeconds()) && (GS->IntermissionTime < 9.f) && (GS->IntermissionTime > 0.f) && !bHasAnnouncedWin)
		{
			EndIntermissionTime = GetWorld()->GetTimeSeconds() + (GS->HasMatchEnded() ? 12.f : 9.1f);
			OldDisplayedParagraphs = 0;
			bFullListPlayed = false;
			bool bIsOnDefense = UTPS && UTPS->Team && GS->IsTeamOnDefenseNextRound(UTPS->Team->TeamIndex);
			int32 MessageIndex = IsBeforeFirstRound() ? 2001 : GS->CTFRound + 2001;
			UTPlayerOwner->ClientReceiveLocalizedMessage(UUTCountDownMessage::StaticClass(), MessageIndex);
			UTPlayerOwner->ClientReceiveLocalizedMessage(UUTCTFRoleMessage::StaticClass(), bIsOnDefense ? 2 : 1);
			if (GS->FlagRunMessageTeam)
			{
				UTPlayerOwner->ClientReceiveLocalizedMessage(UUTFlagRunMessage::StaticClass(), GS->FlagRunMessageSwitch, nullptr, nullptr, GS->FlagRunMessageTeam);
			}
		}
		if (UTPS && UTPS->Team && (EndIntermissionTime > GetWorld()->GetTimeSeconds()) && UTHUDOwner && UTHUDOwner->UTPlayerOwner)
		{
			if (IsBeforeFirstRound())
			{
				// draw round information
				const bool bIsOnDefense = GS->IsTeamOnDefenseNextRound(UTPS->Team->TeamIndex);
				int32 NumLines = bIsOnDefense ? DefendLines.Num() : AttackLines.Num();
				float Height = RenderScale * (80.f + 32.f*NumLines);
				float Width = 0.35f * Canvas->ClipX;
				DrawFramedBackground(0.5f * (Canvas->ClipX - Width), LeftCorner.Y, Width, Height);

				FText Title = bIsOnDefense ? DefendTitle : AttackTitle;
				float TextYPos = LeftCorner.Y + 16.f*RenderScale;
				FFormatNamedArguments Args;
				Args.Add("RoundNum", FText::AsNumber(1));
				Args.Add("NumRounds", FText::AsNumber(GS->NumRounds));
				FText FormattedTitle = FText::Format(Title, Args);
				DrawText(FormattedTitle, MinimapCenter.X*Canvas->ClipX, TextYPos, UTHUDOwner->MediumFont, RenderScale, 1.0f, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Center);
				float TextXPos = 0.5f*Canvas->ClipX - 0.45f*Width;
				TextYPos += 48.f*RenderScale;

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
			else if (DefaultGame && (GS->GetScoringPlays().Num() > 0))
			{
				float Height = 0.5f*Canvas->ClipY;
				float ScoreWidth = 0.8f*MapSize;
				float YPos = LeftCorner.Y;
				float PageBottom = YPos + 0.1f*Canvas->ClipY;
				DrawScoringSummary(RenderDelta, YPos, LeftCorner.X + 0.04f*MapSize, 0.9f*ScoreWidth, PageBottom);
				LeftCorner.Y += 0.15f*Canvas->ClipY;
			}
		}
		else if (GS->GetScoringPlays().Num() > 0)
		{
			// draw scoring plays
			float ScoreWidth = 0.8f*MapSize;
			float MaxHeight = (0.05f + GS->CTFRound * 0.058f) * Canvas->ClipY;
			float PageBottom = LeftCorner.Y + MaxHeight;
			FLinearColor PageColor = FLinearColor::Black;
			PageColor.A = 0.5f;
			float XOffset = 0.5f*(Canvas->ClipX - ScoreWidth);
			float YPos = LeftCorner.Y;

			if (GS->HasMatchEnded() && GS->WinningTeam)
			{
				YPos += DrawWinAnnouncement(RenderDelta, UTHUDOwner->LargeFont);
			}
			DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset, YPos, ScoreWidth, MaxHeight, 149, 138, 32, 32, 0.5f, PageColor);
			DrawScoringPlays(RenderDelta, YPos, XOffset, 0.9f*ScoreWidth, PageBottom);
		}
	}
	else if ((GS->GetMatchState() != MatchState::CountdownToBegin) && (GS->GetMatchState() != MatchState::PlayerIntro))
	{
		Super::DrawMinimap(RenderDelta);
	}
}

void UUTFlagRunScoreboard::DrawGamePanel(float RenderDelta, float& YOffset)
{
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	if (!GS || ((GS->GetMatchState() != MatchState::MatchIntermission) && !GS->HasMatchEnded() && (!GS->LineUpHelper || !GS->LineUpHelper->bIsActive)))
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
	if (!ShowScorePanel())
	{
		return;
	}
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
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	if (GS && GS->HasMatchEnded() && GS->WinningTeam)
	{
		DrawWinAnnouncement(DeltaTime, UTHUDOwner->LargeFont);
		return;
	}
	if ((TimeLineOffset < 1.f) || !GS || (GS->Teams.Num() < 2) || !GS->Teams[0] || !GS->Teams[1])
	{
		return;
	}

	FFontRenderInfo TextRenderInfo;
	TextRenderInfo.bEnableShadow = true;
	TextRenderInfo.bClipText = true;
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTPlayerOwner->PlayerState);

	FText Title = GS->IsTeamOnDefenseNextRound(UTPS->Team->TeamIndex) ? DefendTitle : AttackTitle;
	FFormatNamedArguments Args;
	Args.Add("RoundNum", FText::AsNumber(IsBeforeFirstRound() ? 1 : GS->CTFRound + 1));
	Args.Add("NumRounds", FText::AsNumber(GS->NumRounds));
	FText FormattedTitle = FText::Format(Title, Args);
	float TitleX, TitleY;

	Canvas->StrLen(UTHUDOwner->MediumFont, FormattedTitle.ToString(), TitleX, TitleY);

	AUTFlagRunHUD* FRHUD = Cast<AUTFlagRunHUD>(UTHUDOwner);
	float WinConditionWidth = FRHUD ? FRHUD->DrawWinConditions(UTHUDOwner->MediumFont, 0.f, YPos, Canvas->ClipX, RenderScale, true, true) : 0.f;

	FLinearColor DrawColor = FLinearColor::White;
	float CurrentScoreHeight = (WinConditionWidth > 0.f) ? 2.f*TitleY : TitleY;
	float FrameWidth = FMath::Max(1.1f*WinConditionWidth, 1.2f*RenderScale*TitleX);
	DrawFramedBackground(0.5f*(Canvas->ClipX - FrameWidth), YPos, FrameWidth, 1.2f*RenderScale*CurrentScoreHeight);

	// draw round information
	Canvas->DrawText(UTHUDOwner->MediumFont, FormattedTitle, 0.5f*(Canvas->ClipX - RenderScale*TitleX), YPos, RenderScale, RenderScale, TextRenderInfo);
	YPos += RenderScale*TitleY;

	if (FRHUD)
	{
		FRHUD->DrawWinConditions(UTHUDOwner->MediumFont, 0.f, YPos, Canvas->ClipX, RenderScale, true);
	}
}

void UUTFlagRunScoreboard::DrawScoringPlayInfo(const FCTFScoringPlay& Play, float CurrentScoreHeight, float SmallYL, float MedYL, float DeltaTime, float& YPos, float XOffset, float ScoreWidth, FFontRenderInfo TextRenderInfo, bool bIsSmallPlay)
{
	AUTCTFGameState* CTFState = Cast<AUTCTFGameState>(UTGameState);
	float BoxYPos = YPos;

	// draw scoring team icon
	int32 IconIndex = Play.Team->TeamIndex;
	IconIndex = FMath::Min(IconIndex, 1);
	float IconHeight = 0.9f*CurrentScoreHeight;
	float IconOffset = 0.12f*ScoreWidth;
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
	Canvas->DrawText(UTHUDOwner->SmallFont, TimeStampLine, XOffset + 0.03f*ScoreWidth, YPos + 0.5f*CurrentScoreHeight - 0.5f*SmallYL, RenderScale, RenderScale, TextRenderInfo);

	// scored by
	Canvas->SetLinearDrawColor(Play.Team->TeamColor);
	float NameSizeX, NameSizeY;
	Canvas->TextSize(UTHUDOwner->MediumFont, ScoredByLine, NameSizeX, NameSizeY, RenderScale, RenderScale);
	Canvas->DrawText(UTHUDOwner->MediumFont, ScoredByLine, XOffset + 0.24f*ScoreWidth, YPos + 0.5f*CurrentScoreHeight - 0.6f*MedYL, RenderScale * FMath::Min(1.f, ScoreWidth*0.45f / FMath::Max(NameSizeX, 1.f)), RenderScale, TextRenderInfo);

	int32 CurrentRoundBonus = FMath::Max(Play.RedBonus, Play.BlueBonus);
	FString BonusString = TEXT("\u2605");
	FLinearColor BonusColor = FLinearColor(0.48f, 0.25f, 0.18f);
	if ((CurrentRoundBonus > 0) && !Play.bDefenseWon)
	{
		if (CurrentRoundBonus >= 60)
		{
			BonusString = (CurrentRoundBonus >= 120) ? TEXT("\u2605 \u2605 \u2605") : TEXT("\u2605 \u2605");
			BonusColor = (CurrentRoundBonus >= 120) ? FLinearColor(1.f, 0.9f, 0.15f) : FLinearColor(0.7f, 0.7f, 0.75f);
		}
	}

	if ((CurrentRoundBonus > 0) && !Play.bDefenseWon && !bIsSmallPlay)
	{
		int32 NetBonus = CurrentRoundBonus;
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
		FString BonusLine = FString::Printf(TEXT("+%d"), NetBonus);
		float BonusX, BonusY;
		Canvas->TextSize(UTHUDOwner->MediumFont, BonusLine, BonusX, BonusY, 1.f, 1.f);
		Canvas->SetLinearDrawColor(FLinearColor::White);
		Canvas->DrawText(UTHUDOwner->TinyFont, BonusLine, XOffset + 1.035f*ScoreWidth, YPos + 0.5f*CurrentScoreHeight - 0.4f*BonusY*RenderScale, RenderScale, RenderScale, TextRenderInfo);
	}

	float ScoringOffsetX, ScoringOffsetY;
	Canvas->TextSize(UTHUDOwner->LargeFont, BonusString, ScoringOffsetX, ScoringOffsetY, 1.f, 1.f);
	float ScoreX = XOffset + 0.87f*ScoreWidth - 0.5f*ScoringOffsetX*RenderScale;

	Canvas->SetLinearDrawColor(BonusColor);
	Canvas->DrawText(UTHUDOwner->LargeFont, BonusString, ScoreX, YPos + 0.5f*CurrentScoreHeight - 0.6f*ScoringOffsetY*RenderScale, RenderScale, RenderScale, TextRenderInfo);
}

void UUTFlagRunScoreboard::DrawScoringPlays(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight)
{
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	if (!GS)
	{
		return;
	}

	int32 CurrentPeriod = -1;
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

	float ScoreHeight = MedYL;
	int32 TotalPlays = GS->GetScoringPlays().Num();

	Canvas->DrawText(UTHUDOwner->MediumFont, ScoringPlaysHeader, XOffset + (ScoreWidth - XL) * 0.5f, YPos, RenderScale, RenderScale, TextRenderInfo);
	YPos += 1.f * MedYL;
	if (GS->GetScoringPlays().Num() == 0)
	{
		float YL;
		Canvas->TextSize(UTHUDOwner->MediumFont, NoScoringText.ToString(), XL, YL, RenderScale, RenderScale);
		YPos += 0.2f * MedYL;
		DrawText(NoScoringText, XOffset + (ScoreWidth - XL) * 0.5f, YPos, UTHUDOwner->MediumFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, RenderScale, 1.f, FLinearColor::White, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), ETextHorzPos::Left, ETextVertPos::Top, TextRenderInfo);
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
	int32 DrawnPlays = 0;
	float PctOffset = 1.f + TimeFloor - TimeLineOffset;
	for (const FCTFScoringPlay& Play : GS->GetScoringPlays())
	{
		DrawnPlays++;
		if (DrawnPlays > NumPlays)
		{
			break;
		}
		if (Play.Team != NULL)
		{
			if ((GS->CTFRound == 0) && (Play.Period > CurrentPeriod))
			{
				CurrentPeriod++;
				if (Play.Period < 3)
				{
					YPos += 0.3f*SmallYL;
					DrawText(PeriodText[Play.Period], XOffset + 0.2f*ScoreWidth, YPos, UTHUDOwner->TinyFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, RenderScale, 1.f, FLinearColor::White, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), ETextHorzPos::Left, ETextVertPos::Center, TextRenderInfo);
					YPos += 0.7f * SmallYL;
				}
			}
			if (DrawnPlays % 2 == 1)
			{
				// draw group background
				FLinearColor DrawColor = FLinearColor::White;
				float CurrentScoreHeight = (DrawnPlays == NumPlays) ? (ScoreHeight + 8.f * RenderScale) : (2.f * ScoreHeight + 16.f * RenderScale);
				float BackAlpha = ((DrawnPlays == NumPlays) && (NumPlays == TimeFloor + 1)) ? FMath::Max(0.5f, PctOffset) : 0.3f;
				DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset, YPos - 4.f*RenderScale, 1.11f*ScoreWidth, CurrentScoreHeight, 149, 138, 32, 32, BackAlpha, DrawColor);
			}
			// draw background
			FLinearColor DrawColor = FLinearColor::White;
			float BackAlpha = ((DrawnPlays == NumPlays) && (NumPlays == TimeFloor + 1)) ? FMath::Max(0.5f, PctOffset) : 0.5f;
			DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset + 0.01f*ScoreWidth, YPos, 1.09f*ScoreWidth, ScoreHeight, 149, 138, 32, 32, BackAlpha, DrawColor);

			float BoxYPos = YPos;
			DrawScoringPlayInfo(Play, ScoreHeight, SmallYL, MedYL, DeltaTime, YPos, XOffset, ScoreWidth, TextRenderInfo, false);
			YPos = (bGroupRoundPairs && (DrawnPlays % 2 == 0)) ? BoxYPos + ScoreHeight + 20.f*RenderScale : BoxYPos + ScoreHeight + 8.f*RenderScale;
		}
	}
}



