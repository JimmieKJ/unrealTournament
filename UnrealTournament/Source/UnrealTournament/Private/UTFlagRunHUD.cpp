// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTFlagRunHUD.h"
#include "UTCTFGameState.h"
#include "UTFlagRunGameState.h"
#include "UTCTFGameMode.h"
#include "UTCTFScoreboard.h"
#include "Slate/UIWindows/SUTPowerupSelectWindow.h"
#include "UTFlagRunScoreboard.h"
#include "UTFlagRunMessage.h"
#include "UTCTFRoleMessage.h"
#include "UTRallyPoint.h"

AUTFlagRunHUD::AUTFlagRunHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDrawMinimap = false;

	ConstructorHelpers::FObjectFinder<UTexture2D> PlayerStartTextureObject(TEXT("/Game/RestrictedAssets/UI/MiniMap/minimap_atlas.minimap_atlas"));
	PlayerStartIcon.U = 128;
	PlayerStartIcon.V = 192;
	PlayerStartIcon.UL = 64;
	PlayerStartIcon.VL = 64;
	PlayerStartIcon.Texture = PlayerStartTextureObject.Object;

	DefendersMustStop = NSLOCTEXT("UTFlagRun", "DefendersMustStop", " must hold on defense to have a chance.");
	DefendersMustHold = NSLOCTEXT("UTFlagRun", "DefendersMustHold", " must hold attackers to");
	AttackersMustScore = NSLOCTEXT("UTFlagRun", "AttackersMustScore", " to have a chance.");
	UnhandledCondition = NSLOCTEXT("UTFlagRun", "UnhandledCondition", "UNHANDLED WIN CONDITION");
	AttackersMustScoreWin = NSLOCTEXT("UTFlagRun", "AttackersMustScoreWin", " to win.");
	AttackersMustScoreTime = NSLOCTEXT("UTFlagRun", "AttackersMustScoreTime", " with over {TimeNeeded}s bonus to have a chance.");
	AttackersMustScoreTimeWin = NSLOCTEXT("UTFlagRun", "AttackersMustScoreTimeWin", " with over {TimeNeeded}s bonus to win.");

	MustScoreText = NSLOCTEXT("UTTeamScoreboard", "MustScore", " must score ");
	RedTeamText = NSLOCTEXT("UTTeamScoreboard", "RedTeam", "RED");
	BlueTeamText = NSLOCTEXT("UTTeamScoreboard", "BlueTeam", "BLUE");
	GoldBonusText = NSLOCTEXT("FlagRun", "GoldBonusText", "\u2605 \u2605 \u2605");
	SilverBonusText = NSLOCTEXT("FlagRun", "SilverBonusText", "\u2605 \u2605");
	BronzeBonusText = NSLOCTEXT("FlagRun", "BronzeBonusText", "\u2605");
}

void AUTFlagRunHUD::BeginPlay()
{
	Super::BeginPlay();

	for (TActorIterator<AUTRallyPoint> It(GetWorld()); It; ++It)
	{
		AUTRallyPoint* RallyPoint = *It;
		if (RallyPoint)
		{
			AddPostRenderedActor(RallyPoint);
		}
	}
}

void AUTFlagRunHUD::NotifyMatchStateChange()
{
	AUTFlagRunGameState* GS = Cast<AUTFlagRunGameState>(GetWorld()->GetGameState());
	if (GS && GS->GetMatchState() == MatchState::InProgress && GS->FlagRunMessageTeam && UTPlayerOwner)
	{
		UTPlayerOwner->ClientReceiveLocalizedMessage(UUTFlagRunMessage::StaticClass(), GS->FlagRunMessageSwitch, nullptr, nullptr, GS->FlagRunMessageTeam);
		WinConditionMessageTime = 5.f;
		ScoreMessageText = MustScoreText;
	}
	Super::NotifyMatchStateChange();
}

void AUTFlagRunHUD::DrawHUD()
{
	Super::DrawHUD();

	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
//	GS->FlagRunMessageSwitch = 5309; // fixmesteve remove
//	GS->FlagRunMessageTeam = GS->Teams[0];
//	ScoreMessageText = MustScoreText; 
	bShowScoresWhileDead = bShowScoresWhileDead && GS && GS->IsMatchInProgress() && !GS->IsMatchIntermission() && UTPlayerOwner && !UTPlayerOwner->GetPawn() && !UTPlayerOwner->IsInState(NAME_Spectating);
	bool bScoreboardIsUp = bShowScores || bForceScores || bShowScoresWhileDead;
	if (!bScoreboardIsUp && GS && GS->GetMatchState() == MatchState::InProgress)
	{
		if (GS->FlagRunMessageTeam && UTPlayerOwner && (WinConditionMessageTime > 0.f))
		{
			DrawWinConditions(MediumFont, 0.f, 0.2f*Canvas->ClipY, Canvas->ClipX, 1.f, true);
			WinConditionMessageTime -= GetWorld()->DeltaTimeSeconds;
		}
		float XAdjust = 0.05f * Canvas->ClipX * GetHUDWidgetScaleOverride();
		float XOffsetRed = 0.5f * Canvas->ClipX - XAdjust;
		float XOffsetBlue = 0.49f * Canvas->ClipX + XAdjust;
		float YOffset = 0.03f * Canvas->ClipY * GetHUDWidgetScaleOverride();
		FFontRenderInfo TextRenderInfo;

		// draw pips for players alive on each team @TODO move to widget
		TextRenderInfo.bEnableShadow = true;
		int32 OldRedCount = RedPlayerCount;
		int32 OldBlueCount = BluePlayerCount;
		RedPlayerCount = 0;
		BluePlayerCount = 0;
		float BasePipSize = 0.025f * Canvas->ClipX * GetHUDWidgetScaleOverride();
		XAdjust = 128.0f * GetHUDWidgetScaleOverride(); 
		XOffsetRed = 0.5f * Canvas->ClipX - XAdjust - BasePipSize;
		XOffsetBlue = 0.5f * Canvas->ClipX + XAdjust;
		YOffset = 0.1f * Canvas->ClipY * GetHUDWidgetScaleOverride();
		float XOffsetText = 0.f;
		TArray<AUTPlayerState*> LivePlayers;
		AUTPlayerState* HUDPS = GetScorerPlayerState();
		for (APlayerState* PS : GS->PlayerArray)
		{
			AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
			if (UTPS != NULL && UTPS->Team != NULL && !UTPS->bOnlySpectator  && !UTPS->bIsInactive)
			{
				UTPS->SelectionOrder = (UTPS == HUDPS) ? 0 : 1;
				LivePlayers.Add(UTPS);
			}
		}
		LivePlayers.Sort([](AUTPlayerState& A, AUTPlayerState& B) { return A.SelectionOrder < B.SelectionOrder; });
		for (AUTPlayerState* UTPS : LivePlayers)
		{
			bool bIsAttacker = (GS->bRedToCap == (UTPS->Team->TeamIndex == 0));
			float OwnerPipScaling = (UTPS == GetScorerPlayerState()) ? 1.5f : 1.f;
			float PipSize = BasePipSize * OwnerPipScaling;
			float SkullPipSize = 0.6f * BasePipSize * OwnerPipScaling;
			if (bIsAttacker ? GS->bAttackerLivesLimited : GS->bDefenderLivesLimited)
			{
				if (!UTPS->bOutOfLives)
				{
					bool bLastLife = (UTPS->RemainingLives == 1);
					if (UTPS->Team->TeamIndex == 0)
					{
						RedPlayerCount++;
						Canvas->SetLinearDrawColor(bLastLife ? FLinearColor::White : FLinearColor::Red, 0.7f);
						Canvas->DrawTile(PlayerStartIcon.Texture, XOffsetRed, YOffset, PipSize, PipSize, PlayerStartIcon.U, PlayerStartIcon.V, PlayerStartIcon.UL, PlayerStartIcon.VL, BLEND_Translucent);
						XOffsetText = XOffsetRed;
						XOffsetRed -= 1.1f*PipSize;
					}
					else
					{
						BluePlayerCount++;
						Canvas->SetLinearDrawColor(bLastLife ? FLinearColor::White : FLinearColor::Blue, 0.7f);
						Canvas->DrawTile(PlayerStartIcon.Texture, XOffsetBlue, YOffset, PipSize, PipSize, PlayerStartIcon.U, PlayerStartIcon.V, PlayerStartIcon.UL, PlayerStartIcon.VL, BLEND_Translucent);
						XOffsetText = XOffsetBlue;
						XOffsetBlue += 1.1f*PipSize;
					}
					if (!bLastLife)
					{
						Canvas->SetLinearDrawColor(FLinearColor::White, 1.f);
						Canvas->DrawText(TinyFont, FText::AsNumber(UTPS->RemainingLives), XOffsetText + 0.4f*PipSize, YOffset + 0.2f*PipSize, 0.75f, 0.75f, TextRenderInfo);
					}
				}
			}
		}
		if (OldRedCount > RedPlayerCount)
		{
			RedDeathTime = GetWorld()->GetTimeSeconds();
		}
		if (OldBlueCount > BluePlayerCount)
		{
			BlueDeathTime = GetWorld()->GetTimeSeconds();
		}

		float TimeSinceRedDeath = GetWorld()->GetTimeSeconds() - RedDeathTime;
		if (TimeSinceRedDeath < 0.5f)
		{
			Canvas->SetLinearDrawColor(FLinearColor::Red, 0.5f - TimeSinceRedDeath);
			float ScaledSize = 1.f + 2.f*TimeSinceRedDeath;
			Canvas->DrawTile(PlayerStartIcon.Texture, XOffsetRed - 0.5f*(ScaledSize - 1.f)*BasePipSize, YOffset - 0.5f*(ScaledSize - 1.f)*BasePipSize, BasePipSize, BasePipSize, PlayerStartIcon.U, PlayerStartIcon.V, PlayerStartIcon.UL, PlayerStartIcon.VL, BLEND_Translucent);
		}

		float TimeSinceBlueDeath = GetWorld()->GetTimeSeconds() - BlueDeathTime;
		if (TimeSinceBlueDeath < 0.5f)
		{
			Canvas->SetLinearDrawColor(FLinearColor::Blue, 0.5f - TimeSinceBlueDeath);
			float ScaledSize = 1.f + 2.f*TimeSinceBlueDeath;
			Canvas->DrawTile(PlayerStartIcon.Texture, XOffsetBlue - 0.5f*(ScaledSize - 1.f)*BasePipSize, YOffset - 0.5f*(ScaledSize - 1.f)*BasePipSize, BasePipSize, BasePipSize, PlayerStartIcon.U, PlayerStartIcon.V, PlayerStartIcon.UL, PlayerStartIcon.VL, BLEND_Translucent);
		}
	}
}

void AUTFlagRunHUD::DrawWinConditions(UFont* InFont, float XOffset, float YPos, float ScoreWidth, float RenderScale, bool bCenterMessage)
{
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	if (GS && GS->FlagRunMessageTeam != nullptr)
	{
		FFontRenderInfo TextRenderInfo;
		TextRenderInfo.bEnableShadow = true;
		TextRenderInfo.bClipText = true;
		float ScoreX = XOffset;

		AUTTeamInfo* SubjectTeam = GS->FlagRunMessageTeam;
		FText EmphasisText = (SubjectTeam && (SubjectTeam->TeamIndex == 0)) ? RedTeamText : BlueTeamText;
		FLinearColor EmphasisColor = (SubjectTeam && (SubjectTeam->TeamIndex == 0)) ? FLinearColor::Red : FLinearColor::Blue;

		float YL, EmphasisXL;
		Canvas->StrLen(InFont, EmphasisText.ToString(), EmphasisXL, YL);
		float BonusXL = 0.f;
		float MustScoreXL = 0.f;
		FText BonusType = BronzeBonusText;
		FLinearColor BonusColor = FLinearColor(0.48f, 0.25f, 0.18f);

		int32 Switch = GS->FlagRunMessageSwitch;
		int32 TimeNeeded = 0;
		if (Switch >= 100)
		{
			TimeNeeded = Switch / 100;
			Switch = Switch - 100 * TimeNeeded;
		}
		FText PostfixText = FText::GetEmpty();
		switch (Switch)
		{
		case 1: PostfixText = DefendersMustStop; break;
		case 2: PostfixText = DefendersMustHold; break;
		case 3: PostfixText = DefendersMustHold; BonusType = SilverBonusText; break;
		case 4: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTime : AttackersMustScore; break;
		case 5: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTime : AttackersMustScore; BonusType = SilverBonusText; BonusColor = FLinearColor(0.5f, 0.5f, 0.75f); break;
		case 6: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTime : AttackersMustScore; BonusType = GoldBonusText; BonusColor = FLinearColor(1.f, 0.9f, 0.15f); break;
		case 7: PostfixText = UnhandledCondition; break;
		case 8: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTimeWin : AttackersMustScoreWin; break;
		case 9: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTimeWin : AttackersMustScoreWin; BonusType = SilverBonusText; BonusColor = FLinearColor(0.5f, 0.5f, 0.75f); break;
		case 10: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTimeWin : AttackersMustScoreWin; BonusType = GoldBonusText; BonusColor = FLinearColor(1.f, 0.9f, 0.15f); break;
		}	

		if (Switch > 1)
		{
			if (Switch > 3)
			{
				Canvas->StrLen(InFont, MustScoreText.ToString(), MustScoreXL, YL);
			}
			Canvas->StrLen(InFont, BonusType.ToString(), BonusXL, YL);
		}
		FFormatNamedArguments Args;
		Args.Add("TimeNeeded", TimeNeeded);
		PostfixText = FText::Format(PostfixText, Args);
		float PostXL;
		Canvas->StrLen(InFont, PostfixText.ToString(), PostXL, YL);

		if (bCenterMessage)
		{
			ScoreX = XOffset + 0.5f * (ScoreWidth - RenderScale * (EmphasisXL + PostXL + MustScoreXL + BonusXL));
		}

		Canvas->SetLinearDrawColor(GS->FlagRunMessageTeam->TeamIndex == 0 ? FLinearColor::Red : FLinearColor::Blue);
		Canvas->DrawText(InFont, EmphasisText, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
		ScoreX += EmphasisXL*RenderScale;

		if (Switch > 1)
		{
			if (Switch < 4)
			{
				Canvas->SetLinearDrawColor(FLinearColor::White);
				Canvas->DrawText(InFont, PostfixText, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
				ScoreX += PostXL*RenderScale;
				PostfixText = FText::GetEmpty();
			}
			else
			{
				Canvas->SetLinearDrawColor(FLinearColor::White);
				Canvas->DrawText(InFont, MustScoreText, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
				ScoreX += MustScoreXL*RenderScale;
			}
			Canvas->SetLinearDrawColor(BonusColor);
			Canvas->DrawText(InFont, BonusType, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
			ScoreX += BonusXL*RenderScale;
		}
		Canvas->SetLinearDrawColor(FLinearColor::White);
		Canvas->DrawText(InFont, PostfixText, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
	}
}

