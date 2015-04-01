// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_CTFSpectator.h"
#include "UTCTFGameState.h"

UUTHUDWidget_CTFSpectator::UUTHUDWidget_CTFSpectator(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DesignedResolution = 1080;
	Position = FVector2D(0, 0);
	Size = FVector2D(1920.0f, 108.0f);
	ScreenPosition = FVector2D(0.0f, 0.85f);
	Origin = FVector2D(0.0f, 0.0f);

	static ConstructorHelpers::FObjectFinder<UTexture2D> Tex(TEXT("Texture2D'/Game/RestrictedAssets/UI/Textures/UTScoreboard01.UTScoreboard01'"));
	TextureAtlas = Tex.Object;
}

bool UUTHUDWidget_CTFSpectator::ShouldDraw_Implementation(bool bShowScores)
{
	return (UTGameState != NULL && !UTGameState->HasMatchEnded() && UTHUDOwner->UTPlayerOwner != NULL && 
				UTHUDOwner->UTPlayerOwner->UTPlayerState != NULL && ( (UTCharacterOwner == NULL && UTPlayerOwner->GetPawn() == NULL) || ( UTCharacterOwner != NULL && UTCharacterOwner->IsDead() ) ) 
				&& (!bShowScores || !UTGameState->HasMatchStarted()));
}


void UUTHUDWidget_CTFSpectator::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);

	AUTCTFGameState* GameState = Cast<AUTCTFGameState>(UTGameState);
	if (GameState == NULL) return;

	if (GameState->HasMatchEnded())
	{
		DrawSimpleMessage(NSLOCTEXT("UUTHUDWidget_Spectator","GameOver","Game Over -- Waiting for Next Match."), DeltaTime);
	}
	else if (GameState->HasMatchStarted())
	{
		if (GameState->IsMatchAtHalftime())
		{
			FFormatNamedArguments Args;
			uint32 WaitTime = GameState->RemainingTime;
			Args.Add("Time", FText::AsNumber(WaitTime));
			FText Msg = FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator","HalfTime","HALFTIME - Take a break - Game restarts in {Time}"),Args);
			DrawSimpleMessage(Msg, DeltaTime);

		}
		else if (GameState->IsMatchInSuddenDeath())
		{
			DrawSimpleMessage(NSLOCTEXT("UUTHUDWidget_Spectator","SuddenDeath","Waiting for Winner.  Press [FIRE] to change your view."), DeltaTime);
		}
		else if (UTHUDOwner->UTPlayerOwner != NULL && UTHUDOwner->UTPlayerOwner->UTPlayerState != NULL && UTHUDOwner->UTPlayerOwner->GetPawn() == NULL)
		{
			if (UTHUDOwner->UTPlayerOwner->UTPlayerState->RespawnTime > 0.0f)
			{
				FFormatNamedArguments Args;
				uint32 WaitTime = uint32(UTHUDOwner->UTPlayerOwner->UTPlayerState->RespawnTime) + 1;
				Args.Add("RespawnTime", FText::AsNumber(WaitTime));
				FText Msg = FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator","RespawnWaitMessage","You can respawn in {RespawnTime}..."),Args);
				DrawSimpleMessage(Msg, DeltaTime);
			}
			else
			{
				DrawSimpleMessage(NSLOCTEXT("UUTHUDWidget_Spectator","RespawnMessage","Press [FIRE] to respawn..."), DeltaTime);
			}
		}
	}
	else
	{
		AUTPlayerState* UTPS = UTHUDOwner->UTPlayerOwner->UTPlayerState;
		if (UTGameState->IsMatchInCountdown())
		{
			if (UTPS != NULL && UTPS->RespawnChoiceA && UTPS->RespawnChoiceB)
			{
				DrawSimpleMessage(NSLOCTEXT("UUTHUDWidget_Spectator", "Choose Start", "Choose your start position"), DeltaTime);
			}
			else
			{
				DrawSimpleMessage(NSLOCTEXT("UUTHUDWidget_Spectator", "MatchStarting", "Match is about to start"), DeltaTime);
			}
		}
		else if (UTGameState->PlayersNeeded > 0)
		{
			DrawSimpleMessage(NSLOCTEXT("UUTHUDWidget_Spectator", "WaitingForPlayers", "Waiting for players to join."), DeltaTime);
		}
		else if (UTPS != NULL && UTPS->bReadyToPlay)
		{
			if (UTGameState->bTeamGame && UTGameState->bAllowTeamSwitches)
			{
				DrawSimpleMessage(NSLOCTEXT("UUTHUDWidget_Spectator", "IsReadyTeam", "You are ready to play, [ALTFIRE] to change teams."), DeltaTime);
			}
			else
			{
				DrawSimpleMessage(NSLOCTEXT("UUTHUDWidget_Spectator", "IsReady", "You are ready to play."), DeltaTime);
			}
		}
		else
		{
			if (UTGameState->bTeamGame && UTGameState->bAllowTeamSwitches)
			{
				DrawSimpleMessage(NSLOCTEXT("UUTHUDWidget_Spectator", "GetReadyTeam", "Press [FIRE] when you are ready, [ALTFIRE] to change teams."), DeltaTime);
			}
			else
			{
				DrawSimpleMessage(NSLOCTEXT("UUTHUDWidget_Spectator", "GetReady", "Press [FIRE] when you are ready."), DeltaTime);
			}
		}
	}
}



void UUTHUDWidget_CTFSpectator::DrawSimpleMessage(FText SimpleMessage, float DeltaTime)
{
	// Draw the Background
	DrawTexture(TextureAtlas, 0, 0, 1920.0, 108.0f, 4, 2, 124, 128, 1.0);

	// Draw the Logo
	DrawTexture(TextureAtlas, 20, 54, 301, 98, 162, 14, 301, 98.0, 1.0f, FLinearColor::White, FVector2D(0.0, 0.5));

	// Draw the Spacer Bar
	DrawTexture(TextureAtlas, 341, 54, 4, 99, 488, 13, 4, 99, 1.0f, FLinearColor::White, FVector2D(0.0, 0.5));
	DrawText(SimpleMessage, 360, 50, UTHUDOwner->LargeFont , 1.0, 1.0, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
}
