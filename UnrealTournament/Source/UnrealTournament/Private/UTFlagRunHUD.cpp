// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTFlagRunHUD.h"
#include "UTCTFGameState.h"
#include "UTCTFRoundGameState.h"
#include "UTCTFGameMode.h"
#include "UTCTFScoreboard.h"
#include "Slate/UIWindows/SUTPowerupSelectWindow.h"

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

	TimeToDelayMenuOpenForIntermission = 8.0f;
}

void AUTFlagRunHUD::DrawHUD()
{
	Super::DrawHUD();

	AUTCTFGameState* GS = GetWorld()->GetGameState<AUTCTFGameState>();
	bool bScoreboardIsUp = bShowScores || bForceScores;
	if (!bScoreboardIsUp && GS && GS->GetMatchState() == MatchState::InProgress && GS->bOneFlagGameMode)
	{
		float XAdjust = 0.05f * Canvas->ClipX * GetHUDWidgetScaleOverride();
		float XOffsetRed = 0.5f * Canvas->ClipX - XAdjust;
		float XOffsetBlue = 0.49f * Canvas->ClipX + XAdjust;
		float YOffset = 0.03f * Canvas->ClipY * GetHUDWidgetScaleOverride();
		FFontRenderInfo TextRenderInfo;

		// draw secondary scores
		if (false) //GS->Teams.Num() > 1 && GS->Teams[0] && GS->Teams[1] && ((GS->Teams[0]->SecondaryScore > 0) || (GS->Teams[1]->SecondaryScore > 0)))
		{
			FText BlueSecondaryScore = FText::AsNumber(GS->Teams[1]->SecondaryScore);
			Canvas->SetLinearDrawColor(FLinearColor::Black, 1.f);
			Canvas->DrawText(TinyFont, FText::AsNumber(GS->Teams[0]->SecondaryScore), XOffsetRed, YOffset, 0.8f, 0.8f, TextRenderInfo);
			Canvas->DrawText(TinyFont, BlueSecondaryScore, XOffsetBlue, YOffset, 0.8f, 0.8f, TextRenderInfo);
			Canvas->SetLinearDrawColor(FLinearColor::Yellow, 1.f);
			Canvas->DrawText(TinyFont, FText::AsNumber(GS->Teams[0]->SecondaryScore), XOffsetRed, YOffset, 0.75f, 0.75f, TextRenderInfo);
			Canvas->DrawText(TinyFont, BlueSecondaryScore, XOffsetBlue, YOffset, 0.75f, 0.75f, TextRenderInfo);
		}

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
					bool bLastLife = (UTPS->RemainingLives == 0);
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
			else
			{
				// @todo fixmesteve optimize - one pass through pawns, set the value of cachedcharacter, show those with none or dead
				AUTCharacter* Character = UTPS->GetUTCharacter();
				Canvas->SetLinearDrawColor(FLinearColor::White, 0.7f);
				if (!Character || Character->IsDead())
				{
					if (UTPS->Team->TeamIndex == 0)
					{
						Canvas->DrawTile(HUDAtlas, XOffsetRed, YOffset, SkullPipSize, SkullPipSize, 725, 0, 28, 36, BLEND_Translucent);
						XOffsetText = XOffsetRed;
						XOffsetRed -= 1.8f*SkullPipSize;
					}
					else
					{
						Canvas->DrawTile(HUDAtlas, XOffsetBlue, YOffset, SkullPipSize, SkullPipSize, 725, 0, 28, 36, BLEND_Translucent);
						XOffsetText = XOffsetBlue;
						XOffsetBlue += 1.8f*SkullPipSize;
					}
					Canvas->SetLinearDrawColor(FLinearColor::White, 1.f);
					Canvas->DrawText(TinyFont, FText::AsNumber(int32(UTPS->RespawnTime + 1.f)), XOffsetText + 0.4f*SkullPipSize, YOffset, 0.75f, 0.75f, TextRenderInfo);
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

	HandlePowerups();
}


void AUTFlagRunHUD::HandlePowerups()
{
#if !UE_SERVER
	AUTCTFRoundGameState* GS = GetWorld()->GetGameState<AUTCTFRoundGameState>();

	AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTPlayerOwner->PlayerState);

	// wait for replication to populate the playerstate and team info
	if (UTPS && UTPS->Team)
	{
		if (GS && ((GS->GetMatchState() == MatchState::WaitingToStart) || (GS->GetMatchState() == MatchState::MatchIntermission)))
		{
			const bool bIsOnDefense = GS->IsTeamOnDefenseNextRound(UTPS->Team->TeamIndex);

			if (!PowerupSelectWindow.IsValid() && UTPlayerOwner)
			{
				SAssignNew(PowerupSelectWindow, SUTPowerupSelectWindow, UTPlayerOwner->GetUTLocalPlayer(), bIsOnDefense);
				UTPlayerOwner->GetUTLocalPlayer()->OpenWindow(PowerupSelectWindow);

				bConstructedPowerupWindowForDefense = bIsOnDefense;
			}
			//if we switch teams nuke the old window and make a new one
			else if (bIsOnDefense != bConstructedPowerupWindowForDefense)
			{
				UTPlayerOwner->GetUTLocalPlayer()->CloseWindow(PowerupSelectWindow);

				SAssignNew(PowerupSelectWindow, SUTPowerupSelectWindow, UTPlayerOwner->GetUTLocalPlayer(), bIsOnDefense);
				UTPlayerOwner->GetUTLocalPlayer()->OpenWindow(PowerupSelectWindow);

				bConstructedPowerupWindowForDefense = bIsOnDefense;
			}
			else
			{
				//We need to force the powerup select window open
				if (!bAlreadyForcedWindowOpening)
				{
					bAlreadyForcedWindowOpening = true;

					//if they have already opened it manually, let it go
					if (!UTPS->bIsPowerupSelectWindowOpen)
					{
						//Intermission we want a delay before forcing it open to give the scoring info time to display
						if (GS->GetMatchState() == MatchState::MatchIntermission)
						{
							GetWorldTimerManager().SetTimer(MenuOpenDelayTimerHandle, this, &AUTFlagRunHUD::OpenPowerupSelectMenu, TimeToDelayMenuOpenForIntermission, false);
						}
						else
						{
							if (GetWorldTimerManager().IsTimerActive(MenuOpenDelayTimerHandle))
							{
								GetWorldTimerManager().ClearTimer(MenuOpenDelayTimerHandle);
							}

							OpenPowerupSelectMenu();
						}
					}
				}
			}
		}
		else if ((PowerupSelectWindow.IsValid()) && (PowerupSelectWindow->GetWindowState() == EUIWindowState::Active))
		{
			UTPS->bIsPowerupSelectWindowOpen = false;
			bAlreadyForcedWindowOpening = false;
			UTPlayerOwner->GetUTLocalPlayer()->CloseWindow(PowerupSelectWindow);
		}
	}
#endif
}

void AUTFlagRunHUD::OpenPowerupSelectMenu()
{
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTPlayerOwner->PlayerState);
	if (UTPS)
	{
		UTPS->bIsPowerupSelectWindowOpen = true;
		bAlreadyForcedWindowOpening = true;
	}
}

EInputMode::Type AUTFlagRunHUD::GetInputMode_Implementation() const
{
#if !UE_SERVER
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTPlayerOwner->PlayerState);

	AUTCTFGameState* GS = GetWorld()->GetGameState<AUTCTFGameState>();
	if (PowerupSelectWindow.IsValid() && UTPS && UTPS->bIsPowerupSelectWindowOpen && GS && ((GS->GetMatchState() == MatchState::WaitingToStart) || (GS->GetMatchState() == MatchState::MatchIntermission)))
	{
		return EInputMode::EIM_GameAndUI;
	}

#endif

	return EInputMode::EIM_GameOnly;
}