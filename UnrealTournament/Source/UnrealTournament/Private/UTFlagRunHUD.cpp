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
	AttackersMustScoreShort = NSLOCTEXT("UTFlagRun", "AttackersMustScoreShort", " with over {TimeNeeded}s");

	MustScoreText = NSLOCTEXT("UTTeamScoreboard", "MustScore", " must score ");
	RedTeamText = NSLOCTEXT("UTTeamScoreboard", "RedTeam", "RED");
	BlueTeamText = NSLOCTEXT("UTTeamScoreboard", "BlueTeam", "BLUE");

	RedTeamIcon.U = 5.f;
	RedTeamIcon.V = 5.f;
	RedTeamIcon.UL = 224.f;
	RedTeamIcon.VL = 310.f;
	RedTeamIcon.Texture = CharacterPortraitAtlas;

	BlueTeamIcon.U = 237.f;
	BlueTeamIcon.V = 5.f;
	BlueTeamIcon.UL = 224.f;
	BlueTeamIcon.VL = 310.f;
	BlueTeamIcon.Texture = CharacterPortraitAtlas;

	BlueTeamOverlay.U = 237.0f;
	BlueTeamOverlay.V = 330.0f;
	BlueTeamOverlay.UL = 224.0f;
	BlueTeamOverlay.VL = 310.0f;
	BlueTeamOverlay.Texture = CharacterPortraitAtlas;

	RedTeamOverlay.U = 5.0f;
	RedTeamOverlay.V = 330.0f;
	RedTeamOverlay.UL = 224.0f;
	RedTeamOverlay.VL = 310.0f;
	RedTeamOverlay.Texture = CharacterPortraitAtlas;


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
		if (GS->FlagRunMessageTeam && UTPlayerOwner)
		{
			bUseShortWinMessage = true;
			DrawWinConditions(TinyFont, 0.f, 0.07f*Canvas->ClipY, Canvas->ClipX, 1.f, true);
			bUseShortWinMessage = false;
		}

		int32 OldRedCount = RedPlayerCount;
		int32 OldBlueCount = BluePlayerCount;
		RedPlayerCount = 0;
		BluePlayerCount = 0;

		const float RenderScale = float(Canvas->SizeY) / 1080.0f;

		float TeammateScale = VerifyProfileSettings() ? CachedProfileSettings->HUDTeammateScaleOverride : UUTProfileSettings::StaticClass()->GetDefaultObject<UUTProfileSettings>()->HUDTeammateScaleOverride;

		float BasePipSize = (32 + (64 * TeammateScale)) * GetHUDWidgetScaleOverride() * RenderScale;  // 96 - 32px in size
		float XAdjust = BasePipSize * 1.1;
		float XOffsetRed = 0.4f * Canvas->ClipX - XAdjust - BasePipSize;
		float XOffsetBlue = 0.6f * Canvas->ClipX + XAdjust;
		float YOffset = 0.005f * Canvas->ClipY * GetHUDWidgetScaleOverride() * RenderScale;
		float XOffsetText = 0.f;

		TArray<AUTPlayerState*> LivePlayers;
		AUTPlayerState* HUDPS = GetScorerPlayerState();
		for (APlayerState* PS : GS->PlayerArray)
		{
			AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
			if (UTPS != NULL && UTPS->Team != NULL && !UTPS->bOnlySpectator && !UTPS->bIsInactive)
			{
				UTPS->SelectionOrder = (UTPS == HUDPS) ? -1 : UTPS->SpectatingIDTeam;
				LivePlayers.Add(UTPS);
			}
		}
		LivePlayers.Sort([](AUTPlayerState& A, AUTPlayerState& B) { return A.SelectionOrder > B.SelectionOrder; });

		for (AUTPlayerState* UTPS : LivePlayers)
		{
			if (!UTPS->bOutOfLives)
			{
				bool bIsAttacker = (GS->bRedToCap == (UTPS->Team->TeamIndex == 0));
				float OwnerPipScaling = (UTPS == GetScorerPlayerState()) ? 1.5f : 1.f;
				float PipSize = BasePipSize * OwnerPipScaling;
				float LiveScaling = FMath::Clamp(((UTPS->RespawnTime > 0.f) && (UTPS->RespawnWaitTime > 0.f) && !UTPS->GetUTCharacter()) ? 1.f - UTPS->RespawnTime / UTPS->RespawnWaitTime : 1.f, 0.f ,1.f);

				if (UTPS->Team->TeamIndex == 0)
				{
					RedPlayerCount++;
					DrawPlayerIcon(UTPS, LiveScaling, XOffsetRed, YOffset, PipSize, bIsAttacker);
					XOffsetRed -= 1.1f*PipSize;
				}
				else
				{
					BluePlayerCount++;
					DrawPlayerIcon(UTPS, LiveScaling, XOffsetBlue, YOffset, PipSize, bIsAttacker);
					XOffsetBlue += 1.1f*PipSize;
				}
			}
		}
	}
}

void AUTFlagRunHUD::DrawPlayerIcon(AUTPlayerState* PlayerState, float LiveScaling, float XOffset, float YOffset, float PipSize, bool bIsAttacker)
{
	FLinearColor BackColor = FLinearColor::Black;
	BackColor.A = 0.5f;

	TSubclassOf<AUTCharacterContent> CharacterData = PlayerState->GetSelectedCharacter();
	if (CharacterData)
	{
		AUTCharacterContent* DefaultCharData = CharacterData->GetDefaultObject<AUTCharacterContent>();
		if (DefaultCharData && DefaultCharData->DefaultCharacterPortrait.Texture)
		{
			Canvas->SetLinearDrawColor(FLinearColor::White);

			float PipHeight = PipSize * (320.0f/224.0f);

			// Draw the background.
			FCanvasIcon* BGIcon = PlayerState->GetTeamNum() == 1 ? &BlueTeamIcon : &RedTeamIcon;
			Canvas->DrawTile(BGIcon->Texture, XOffset, YOffset, PipSize, PipHeight, BGIcon->U, BGIcon->V, BGIcon->UL, BGIcon->VL);

			if (LiveScaling < 1.f)
			{	
				Canvas->SetLinearDrawColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.f));
			}

			BGIcon = &DefaultCharData->DefaultCharacterPortrait;
			if (PlayerState->GetTeamNum() == 1)
			{
				Canvas->DrawTile(BGIcon->Texture, XOffset, YOffset, PipSize, PipHeight, BGIcon->U + BGIcon->UL, BGIcon->V, BGIcon->UL * -1.0f, BGIcon->VL);
			}
			else
			{
				Canvas->DrawTile(BGIcon->Texture, XOffset, YOffset, PipSize, PipHeight, BGIcon->U, BGIcon->V, BGIcon->UL, BGIcon->VL);
			}


			if (LiveScaling < 1.f)
			{	
				Canvas->SetLinearDrawColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
				Canvas->DrawTile(Canvas->DefaultTexture, XOffset + LiveScaling*PipSize, YOffset, PipSize - LiveScaling * PipSize, PipHeight, 0, 0, 1, 1, BLEND_Translucent);
			}


			Canvas->SetLinearDrawColor(FLinearColor::White);

			BGIcon = PlayerState->GetTeamNum() == 1 ? &BlueTeamOverlay : &RedTeamOverlay;
			Canvas->DrawTile(BGIcon->Texture, XOffset, YOffset, PipSize, PipHeight, BGIcon->U, BGIcon->V, BGIcon->UL, BGIcon->VL);

			if (!bIsAttacker)
			{			
				const float FontRenderScale = float(Canvas->SizeY) / 1080.0f;
				FFontRenderInfo TextRenderInfo;

				// draw pips for players alive on each team @TODO move to widget
				TextRenderInfo.bEnableShadow = true;

				FString LivesRemaining = FString::Printf(TEXT("%i"),PlayerState->RemainingLives);
				float XL,YL;
				Canvas->StrLen(SmallFont, LivesRemaining, XL, YL);

				Canvas->SetLinearDrawColor(PlayerState->RemainingLives == 1 ? FLinearColor::Yellow : FLinearColor::White);
				Canvas->DrawText(SmallFont, FText::FromString(LivesRemaining), XOffset + (PipSize * 0.5f) - (XL * 0.5f), YOffset + (PipSize * (320.0f/224.0f)), FontRenderScale, FontRenderScale, TextRenderInfo);
			}
		}

	}
}

float AUTFlagRunHUD::DrawWinConditions(UFont* InFont, float XOffset, float YPos, float ScoreWidth, float RenderScale, bool bCenterMessage, bool bSkipDrawing)
{
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	if (GS && GS->HasMatchEnded())
	{
		return Super::DrawWinConditions(InFont, XOffset, YPos, ScoreWidth, RenderScale, bCenterMessage);
	}
	if (GS && GS->FlagRunMessageTeam != nullptr)
	{
		FFontRenderInfo TextRenderInfo;
		TextRenderInfo.bEnableShadow = true;
		TextRenderInfo.bClipText = true;
		float ScoreX = XOffset;

		FText EmphasisText = (GS->FlagRunMessageTeam->TeamIndex == 0) ? RedTeamText : BlueTeamText;
		FLinearColor EmphasisColor = (GS->FlagRunMessageTeam->TeamIndex == 0) ? FLinearColor::Red : FLinearColor::Blue;

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
		if (bUseShortWinMessage && Switch < 4)
		{
			return 0.f;
		}
		FText PostfixText = FText::GetEmpty();
		switch (Switch)
		{
		case 1: PostfixText = DefendersMustStop; break;
		case 2: PostfixText = DefendersMustHold; break;
		case 3: PostfixText = DefendersMustHold; BonusType = GS->SilverBonusText;  BonusColor = GS->SilverBonusColor; break;
		case 4: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTime : AttackersMustScore; break;
		case 5: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTime : AttackersMustScore; BonusType = GS->SilverBonusText; BonusColor = GS->SilverBonusColor; break;
		case 6: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTime : AttackersMustScore; BonusType = GS->GoldBonusText; BonusColor = GS->GoldBonusColor; break;
		case 7: PostfixText = UnhandledCondition; break;
		case 8: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTimeWin : AttackersMustScoreWin; break;
		case 9: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTimeWin : AttackersMustScoreWin; BonusType = GS->SilverBonusText; BonusColor = GS->SilverBonusColor; break;
		case 10: PostfixText = (TimeNeeded > 0) ? AttackersMustScoreTimeWin : AttackersMustScoreWin; BonusType = GS->GoldBonusText; BonusColor = GS->GoldBonusColor; break;
		}	
		if (bUseShortWinMessage)
		{
			PostfixText = (TimeNeeded > 0) ? AttackersMustScoreShort : FText::GetEmpty();
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
		if (!bSkipDrawing)
		{
			Canvas->SetLinearDrawColor(EmphasisColor);
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
		return RenderScale * (EmphasisXL + PostXL + MustScoreXL + BonusXL);
	}
	return 0.f;
}

