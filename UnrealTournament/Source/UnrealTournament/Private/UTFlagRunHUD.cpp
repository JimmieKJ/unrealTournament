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
}

void AUTFlagRunHUD::NotifyMatchStateChange()
{
	AUTFlagRunGameState* GS = Cast<AUTFlagRunGameState>(GetWorld()->GetGameState());
	if (!GS)
	{
		UE_LOG(UT, Warning, TEXT("NO FRGS!!!!!"));
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("Got flag run state change to %s with team %s"), *GS->GetMatchState().ToString(), GS->FlagRunMessageTeam ? *GS->FlagRunMessageTeam->GetName() : TEXT("NONE"));
	}
	if (GS && GS->GetMatchState() == MatchState::InProgress && GS->FlagRunMessageTeam && UTPlayerOwner)
	{
		UE_LOG(UT, Warning, TEXT("PLAY THE MESSAGE"));
		UTPlayerOwner->ClientReceiveLocalizedMessage(UUTFlagRunMessage::StaticClass(), GS->FlagRunMessageSwitch, nullptr, nullptr, GS->FlagRunMessageTeam);
	}
	Super::NotifyMatchStateChange();
}

void AUTFlagRunHUD::DrawHUD()
{
	Super::DrawHUD();

	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	bShowScoresWhileDead = bShowScoresWhileDead && GS && GS->IsMatchInProgress() && !GS->IsMatchIntermission() && UTPlayerOwner && !UTPlayerOwner->GetPawn() && !UTPlayerOwner->IsInState(NAME_Spectating);
	bool bScoreboardIsUp = bShowScores || bForceScores || bShowScoresWhileDead;
	if (!bScoreboardIsUp && GS && GS->GetMatchState() == MatchState::InProgress)
	{
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


