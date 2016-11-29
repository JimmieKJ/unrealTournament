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
	PlayerStartIcon.U = 128.f;
	PlayerStartIcon.V = 192.f;
	PlayerStartIcon.UL = 64.f;
	PlayerStartIcon.VL = 64.f;
	PlayerStartIcon.Texture = PlayerStartTextureObject.Object;

	DefendersMustStop = NSLOCTEXT("UTFlagRun", "DefendersMustStop", " must hold on defense to have a chance.");
	DefendersMustHold = NSLOCTEXT("UTFlagRun", "DefendersMustHold", " must hold attackers to ");
	AttackersMustScore = NSLOCTEXT("UTFlagRun", "AttackersMustScore", " to have a chance.");
	UnhandledCondition = NSLOCTEXT("UTFlagRun", "UnhandledCondition", "UNHANDLED WIN CONDITION");
	AttackersMustScoreWin = NSLOCTEXT("UTFlagRun", "AttackersMustScoreWin", " to win.");
	AttackersMustScoreTime = NSLOCTEXT("UTFlagRun", "AttackersMustScoreTime", " with over {TimeNeeded}s bonus to have a chance.");
	AttackersMustScoreTimeWin = NSLOCTEXT("UTFlagRun", "AttackersMustScoreTimeWin", " with over {TimeNeeded}s bonus to win.");

	MustScoreText = NSLOCTEXT("UTTeamScoreboard", "MustScore", " must score ");
	RedTeamText = NSLOCTEXT("UTTeamScoreboard", "RedTeam", "RED");
	BlueTeamText = NSLOCTEXT("UTTeamScoreboard", "BlueTeam", "BLUE");

	RedTeamIcon.U = 257.f;
	RedTeamIcon.V = 940.f;
	RedTeamIcon.UL = 72.f;
	RedTeamIcon.VL = 72.f;
	RedTeamIcon.Texture = HUDAtlas;

	BlueTeamIcon.U = 333.f;
	BlueTeamIcon.V = 940.f;
	BlueTeamIcon.UL = 72.f;
	BlueTeamIcon.VL = 72.f;
	BlueTeamIcon.Texture = HUDAtlas;
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
		WinConditionMessageTime = 5.f;
		ScoreMessageText = MustScoreText;
	}
	Super::NotifyMatchStateChange();
}

bool AUTFlagRunHUD::ScoreboardIsUp()
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	return  (GS && (GS->IsMatchIntermission() || GS->HasMatchEnded())) || Super::ScoreboardIsUp();
}

void AUTFlagRunHUD::DrawHUD()
{
	Super::DrawHUD();

	AUTFlagRunGameState* GS = Cast<AUTFlagRunGameState>(GetWorld()->GetGameState());
	bool bScoreboardIsUp = ScoreboardIsUp();
	if (!bScoreboardIsUp && GS && GS->GetMatchState() == MatchState::InProgress)
	{
		if (GS->FlagRunMessageTeam && UTPlayerOwner && (WinConditionMessageTime > 0.f))
		{
			DrawWinConditions(MediumFont, 0.f, 0.2f*Canvas->ClipY, Canvas->ClipX, 1.f, true);
			WinConditionMessageTime -= GetWorld()->DeltaTimeSeconds;
		}
		FFontRenderInfo TextRenderInfo;

		// draw pips for players alive on each team @TODO move to widget
		TextRenderInfo.bEnableShadow = true;
		int32 OldRedCount = RedPlayerCount;
		int32 OldBlueCount = BluePlayerCount;
		RedPlayerCount = 0;
		BluePlayerCount = 0;
		float BasePipSize = 0.03f * Canvas->ClipX * GetHUDWidgetScaleOverride();
		float XAdjust = 0.1f * Canvas->ClipX * GetHUDWidgetScaleOverride();
		float XOffsetRed = 0.47f * Canvas->ClipX - XAdjust - BasePipSize;
		float XOffsetBlue = 0.53f * Canvas->ClipX + XAdjust;
		float YOffset = 0.005f * Canvas->ClipY * GetHUDWidgetScaleOverride();
		float XOffsetText = 0.f;
		TArray<AUTPlayerState*> LivePlayers;
		AUTPlayerState* HUDPS = GetScorerPlayerState();
		for (APlayerState* PS : GS->PlayerArray)
		{
			AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
			if (UTPS != NULL && UTPS->Team != NULL && !UTPS->bOnlySpectator && !UTPS->bIsInactive)
			{
				UTPS->SelectionOrder = (UTPS == HUDPS) ? -1 : UTPS->PlayerId;
				LivePlayers.Add(UTPS);
			}
		}
		LivePlayers.Sort([](AUTPlayerState& A, AUTPlayerState& B) { return A.SelectionOrder > B.SelectionOrder; });
		float XL, YL;
		Canvas->TextSize(SmallFont, "5", XL, YL);
		for (AUTPlayerState* UTPS : LivePlayers)
		{
			if (!UTPS->bOutOfLives)
			{
				bool bIsAttacker = (GS->bRedToCap == (UTPS->Team->TeamIndex == 0));
				float OwnerPipScaling = (UTPS == GetScorerPlayerState()) ? 1.5f : 1.f;
				float PipSize = BasePipSize * OwnerPipScaling;
				bool bLastLife = (UTPS->RemainingLives == 1);
				float LiveScaling = FMath::Clamp(((UTPS->RespawnTime > 0.f) && (UTPS->RespawnWaitTime > 0.f) && !UTPS->GetUTCharacter()) ? 1.f - UTPS->RespawnTime / UTPS->RespawnWaitTime : 1.f, 0.f ,1.f);
				if (UTPS->Team->TeamIndex == 0)
				{
					RedPlayerCount++;
					DrawPlayerIcon(RedTeamIcon, bLastLife ? FLinearColor::White : FLinearColor::Red, LiveScaling, XOffsetRed, YOffset, PipSize);
					XOffsetText = XOffsetRed;
					XOffsetRed -= 1.1f*PipSize;
				}
				else
				{
					BluePlayerCount++;
					DrawPlayerIcon(BlueTeamIcon, bLastLife ? FLinearColor::White : FLinearColor::Blue, LiveScaling, XOffsetBlue, YOffset, PipSize);
					XOffsetText = XOffsetBlue;
					XOffsetBlue += 1.1f*PipSize;
				}
				if (!bIsAttacker)
				{
					Canvas->SetLinearDrawColor(bLastLife ? FLinearColor::Yellow : FLinearColor::White);
					Canvas->DrawText(SmallFont, FText::AsNumber(UTPS->RemainingLives), XOffsetText + 0.5f*(PipSize - XL), YOffset + 0.95f*PipSize - YL, 1.f, 1.f, TextRenderInfo);
				}
			}
		}
	}
}

void AUTFlagRunHUD::DrawPlayerIcon(FCanvasIcon PlayerIcon, FLinearColor DrawColor, float LiveScaling, float XOffset, float YOffset, float PipSize)
{
	FLinearColor BackColor = FLinearColor::Black;
	BackColor.A = 0.5f;
	Canvas->SetLinearDrawColor(BackColor);
	Canvas->DrawTile(Canvas->DefaultTexture, XOffset, YOffset, LiveScaling*PipSize, PipSize, 0, 0, 1, 1, BLEND_Translucent);

	Canvas->SetLinearDrawColor(DrawColor);
	Canvas->DrawTile(PlayerIcon.Texture, XOffset, YOffset, LiveScaling*PipSize, PipSize, PlayerIcon.U, PlayerIcon.V, PlayerIcon.UL*LiveScaling, PlayerIcon.VL, BLEND_Translucent);
	if (LiveScaling < 1.f)
	{
		Canvas->SetLinearDrawColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.3f));
		Canvas->DrawTile(Canvas->DefaultTexture, XOffset + LiveScaling*PipSize, YOffset, PipSize - LiveScaling *PipSize, PipSize, 0, 0, 1, 1, BLEND_Translucent);
		Canvas->SetLinearDrawColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.f));
		Canvas->DrawTile(PlayerIcon.Texture, XOffset + LiveScaling*PipSize, YOffset, PipSize - LiveScaling *PipSize, PipSize, PlayerIcon.U + PlayerIcon.UL*LiveScaling, PlayerIcon.V, PlayerIcon.UL * (1.f - LiveScaling), PlayerIcon.VL, BLEND_Translucent);
	}
}

void AUTFlagRunHUD::DrawWinConditions(UFont* InFont, float XOffset, float YPos, float ScoreWidth, float RenderScale, bool bCenterMessage)
{

	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	if (GS && GS->HasMatchEnded())
	{
		Super::DrawWinConditions(InFont, XOffset, YPos, ScoreWidth, RenderScale, bCenterMessage);
		return;
	}
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
		FText BonusType = GS->BronzeBonusText;
		FLinearColor BonusColor = GS->BronzeBonusColor;

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
		case 3: PostfixText = DefendersMustHold; BonusType = GS->SilverBonusText; break;
		case 4: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTime : AttackersMustScore; break;
		case 5: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTime : AttackersMustScore; BonusType = GS->SilverBonusText; BonusColor = GS->SilverBonusColor; break;
		case 6: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTime : AttackersMustScore; BonusType = GS->GoldBonusText; BonusColor = GS->GoldBonusColor; break;
		case 7: PostfixText = UnhandledCondition; break;
		case 8: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTimeWin : AttackersMustScoreWin; break;
		case 9: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTimeWin : AttackersMustScoreWin; BonusType = GS->SilverBonusText; BonusColor = GS->SilverBonusColor; break;
		case 10: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTimeWin : AttackersMustScoreWin; BonusType = GS->GoldBonusText; BonusColor = GS->GoldBonusColor; break;
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

