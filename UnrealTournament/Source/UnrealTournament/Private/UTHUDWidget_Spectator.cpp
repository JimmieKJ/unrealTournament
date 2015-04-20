// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_Spectator.h"

UUTHUDWidget_Spectator::UUTHUDWidget_Spectator(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DesignedResolution = 1080;
	Position=FVector2D(0,0);
	Size=FVector2D(1920.0f,108.0f);
	ScreenPosition=FVector2D(0.0f, 0.85f);
	Origin=FVector2D(0.0f,0.0f);

	static ConstructorHelpers::FObjectFinder<UTexture2D> Tex(TEXT("Texture2D'/Game/RestrictedAssets/UI/Textures/UTScoreboard01.UTScoreboard01'"));
	TextureAtlas = Tex.Object;
}

bool UUTHUDWidget_Spectator::ShouldDraw_Implementation(bool bShowScores)
{
	return (UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->UTPlayerState 
			&& (UTHUDOwner->UTPlayerOwner->UTPlayerState->bOnlySpectator || (UTCharacterOwner == NULL && UTPlayerOwner->GetPawn() == NULL) || (UTCharacterOwner != NULL && UTCharacterOwner->IsDead()))
			&& (!bShowScores || !UTGameState->HasMatchStarted()));
}

void UUTHUDWidget_Spectator::DrawSimpleMessage(FText SimpleMessage, float DeltaTime, bool bShortMessage)
{
	if (SimpleMessage.IsEmpty())
	{
		return;
	}
	float BackgroundWidth = 1920.f;
	float TextPosition = 360.f;
	if (bShortMessage && UTHUDOwner->LargeFont)
	{
		TextPosition = 32.f;
		float YL = 0.0f;
		Canvas->StrLen(UTHUDOwner->LargeFont, SimpleMessage.ToString(), BackgroundWidth, YL);
		BackgroundWidth += 64.f;
	}
	// Draw the Background
	DrawTexture(TextureAtlas, 0, 0, BackgroundWidth, 108.0f, 4, 2, 124, 128, 1.0);
	if (!bShortMessage)
	{
		// Draw the Logo
		DrawTexture(TextureAtlas, 20, 54, 301, 98, 162, 14, 301, 98.0, 1.0f, FLinearColor::White, FVector2D(0.0, 0.5));

		// Draw the Spacer Bar
		DrawTexture(TextureAtlas, 341, 54, 4, 99, 488, 13, 4, 99, 1.0f, FLinearColor::White, FVector2D(0.0, 0.5));
	}
	DrawText(SimpleMessage, TextPosition, 50.f, UTHUDOwner->LargeFont, 1.f, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
}

void UUTHUDWidget_Spectator::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);

	if (TextureAtlas && UTGameState)
	{
		bool bShortMessage = false;
		FText SpectatorMessage;
		if (!UTGameState->HasMatchStarted())	
		{
			// Look to see if we are waiting to play and if we must be ready.  If we aren't, just exit cause we don
			AUTPlayerState* UTPS = UTHUDOwner->UTPlayerOwner->UTPlayerState;
			if (UTGameState->IsMatchInCountdown())
			{
				SpectatorMessage = (UTPS && UTPS->RespawnChoiceA && UTPS->RespawnChoiceB)
									? NSLOCTEXT("UUTHUDWidget_Spectator", "Choose Start", "Choose your start position")
									: NSLOCTEXT("UUTHUDWidget_Spectator", "MatchStarting", "Match is about to start");
			}
			else if (UTGameState->PlayersNeeded > 0)
			{
				SpectatorMessage = NSLOCTEXT("UUTHUDWidget_Spectator", "WaitingForPlayers", "Waiting for players to join.");
			}
			else if (UTPS && UTPS->bReadyToPlay)
			{
				SpectatorMessage = (UTGameState->bTeamGame && UTGameState->bAllowTeamSwitches)
									? NSLOCTEXT("UUTHUDWidget_Spectator", "IsReadyTeam", "You are ready, press [ALTFIRE] to change teams.")
									: NSLOCTEXT("UUTHUDWidget_Spectator", "IsReady", "You are ready to play.");
			}
			else if (UTPS && UTPS->bOnlySpectator)
			{
				SpectatorMessage = NSLOCTEXT("UUTHUDWidget_Spectator", "WaitingForReady", "Waiting for players to ready up.");
			}
			else
			{
				SpectatorMessage = (UTGameState->bTeamGame && UTGameState->bAllowTeamSwitches)
									? NSLOCTEXT("UUTHUDWidget_Spectator", "GetReadyTeam", "Press [FIRE] to ready up, [ALTFIRE] to change teams.")
									: NSLOCTEXT("UUTHUDWidget_Spectator", "GetReady", "Press [FIRE] when you are ready.");
			}
		}
		else if (!UTGameState->HasMatchEnded())
		{
			if (UTGameState->IsMatchAtHalftime())
			{
				FFormatNamedArguments Args;
				Args.Add("Time", FText::AsNumber(UTGameState->RemainingTime));
				SpectatorMessage = FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator", "HalfTime", "HALFTIME - Game resumes in {Time}"), Args);
			}
			else if (UTHUDOwner->UTPlayerOwner->UTPlayerState->bOnlySpectator)
			{
				AActor* ViewActor = UTHUDOwner->UTPlayerOwner->GetViewTarget();
				AUTCharacter* ViewCharacter = Cast<AUTCharacter>(ViewActor);
				if (ViewCharacter && ViewCharacter->PlayerState)
				{
					FFormatNamedArguments Args;
					Args.Add("PlayerName", FText::AsCultureInvariant(ViewCharacter->PlayerState->PlayerName));
					bShortMessage = true;
					SpectatorMessage = FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator", "SpectatorPlayerWatching", "{PlayerName}"), Args);
				}
			}
			else if (UTGameState->IsMatchInOvertime() && (UTGameState->bOnlyTheStrongSurvive || UTGameState->IsMatchInSuddenDeath()))
			{
				SpectatorMessage = NSLOCTEXT("UUTHUDWidget_Spectator", "SpectatorCameraChange", "Press [FIRE] to change viewpoint...");
			}
			else 
			{
				if (UTHUDOwner->UTPlayerOwner->UTPlayerState->RespawnTime > 0.0f)
				{
					FFormatNamedArguments Args;
					Args.Add("RespawnTime", FText::AsNumber(UTHUDOwner->UTPlayerOwner->UTPlayerState->RespawnTime + 1));
					SpectatorMessage = FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator","RespawnWaitMessage","You can respawn in {RespawnTime}..."),Args);
				}
				else
				{
					SpectatorMessage = (UTHUDOwner->UTPlayerOwner->UTPlayerState->RespawnChoiceA != nullptr)
											? NSLOCTEXT("UUTHUDWidget_Spectator", "ChooseRespawnMessage", "Choose a respawn point with [FIRE] or [ALT-FIRE]")
											: NSLOCTEXT("UUTHUDWidget_Spectator", "RespawnMessage", "Press [FIRE] to respawn...");
				}
			}
		}
		else
		{
			AActor* ViewActor = UTHUDOwner->UTPlayerOwner->GetViewTarget();
			AUTCharacter* ViewCharacter = Cast<AUTCharacter>(ViewActor);
			if (ViewCharacter && ViewCharacter->PlayerState)
			{
				FFormatNamedArguments Args;
				Args.Add("PlayerName", FText::AsCultureInvariant(ViewCharacter->PlayerState->PlayerName));
				bShortMessage = true;
				SpectatorMessage = FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator", "SpectatorPlayerWatching", "PLAYER {PlayerName}"), Args);
			}
		}
		DrawSimpleMessage(SpectatorMessage, DeltaTime, bShortMessage);
	}
}

