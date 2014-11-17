// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_CTFSpectator.h"
#include "UTCTFGameState.h"

UUTHUDWidget_CTFSpectator::UUTHUDWidget_CTFSpectator(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Position=FVector2D(0,0);
	Size=FVector2D(0.0f,0.0f);
	ScreenPosition=FVector2D(0.0f, 0.85f);
	Origin=FVector2D(0.0f,0.0f);
}

void UUTHUDWidget_CTFSpectator::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);

	AUTCTFGameState* GameState = UTHUDOwner->GetWorld()->GetGameState<AUTCTFGameState>();
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
				FText Msg = FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator","RepsawnWaitMessage","You can respawn in {RespawnTime}..."),Args);
				DrawSimpleMessage(Msg, DeltaTime);
			}
			else
			{
				DrawSimpleMessage(NSLOCTEXT("UUTHUDWidget_Spectator","RepsawnMessage","Press [FIRE] to respawn..."), DeltaTime);
			}
		}
	}
	else
	{
		// Look to see if we are waiting to play and if we must be ready.  If we aren't, just exit cause we don
		if (!GameState->bPlayerMustBeReady)
		{
			DrawSimpleMessage(NSLOCTEXT("UUTHUDWidget_Spectator","WaitForMatch","Waiting for Match to Begin"), DeltaTime);
		}
		else if (UTHUDOwner->UTPlayerOwner->UTPlayerState != NULL && UTHUDOwner->UTPlayerOwner->UTPlayerState->bReadyToPlay)
		{
			DrawSimpleMessage(NSLOCTEXT("UUTHUDWidget_Spectator","IsReady","You are ready to play"),DeltaTime);
		}
		else
		{
			DrawSimpleMessage(NSLOCTEXT("UUTHUDWidget_Spectator","IsReady","Press [FIRE] when you are ready to play..."),DeltaTime);
		}
	}
}

void UUTHUDWidget_CTFSpectator::DrawSimpleMessage(FText SimpleMessage, float DeltaTime)
{
	float XL, YL;
	Canvas->StrLen(UTHUDOwner->MediumFont, SimpleMessage.ToString(), XL, YL);
	DrawTexture(Canvas->DefaultTexture, 0,0,Canvas->ClipX, YL * RenderScale,0,0,1,1,1.0, FLinearColor(0.08,0.28,0.60,1.0));

	// Draw the Unreal Symbol

	float H = 69.0 * RenderScale;
	float W = H * (82.0/69.0);

	float LogoX = Canvas->ClipX * 0.005;

	DrawTexture(UTHUDOwner->OldHudTexture, LogoX, YL * 0.5f, W, H, 734,190, 82,70, 1.0f, FLinearColor::White, FVector2D(0.0f,0.5f));
	DrawText(SimpleMessage, LogoX + (W*1.1), (5*RenderScale) , UTHUDOwner->MediumFont, false, FVector2D(0,0), FLinearColor::Black, false, FLinearColor::Black, RenderScale);
}

