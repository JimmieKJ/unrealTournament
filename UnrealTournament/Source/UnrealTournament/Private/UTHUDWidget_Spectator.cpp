// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_Spectator.h"

UUTHUDWidget_Spectator::UUTHUDWidget_Spectator(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	Position=FVector2D(0,0);
	Size=FVector2D(0.0f,0.0f);
	ScreenPosition=FVector2D(0.0f, 0.85f);
	Origin=FVector2D(0.0f,0.0f);
}

void UUTHUDWidget_Spectator::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);

	AUTGameState* UTGameState = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
	
	if (UTGameState != NULL && !UTGameState->HasMatchEnded() && UTHUDOwner->UTPlayerOwner != NULL && UTHUDOwner->UTPlayerOwner->GetUTCharacter() == NULL)
	{
		FText SpectatorMessage;
		if (!UTGameState->HasMatchStarted())	
		{
			// Look to see if we are waiting to play and if we must be ready.  If we aren't, just exit cause we don

			if (!UTGameState->bPlayerMustBeReady)
			{
				return;
			}

			if (UTHUDOwner->UTPlayerOwner->UTPlayerState != NULL && UTHUDOwner->UTPlayerOwner->UTPlayerState->bReadyToPlay)
			{
				SpectatorMessage = NSLOCTEXT("UUTHUDWidget_Spectator","IsReady","You are ready to play");
			}
			else
			{
				SpectatorMessage = NSLOCTEXT("UUTHUDWidget_Spectator","IsReady","Press [FIRE] when you are ready to play...");
			}
		}
		else if (!UTGameState->HasMatchEnded())
		{
			if (UTHUDOwner->UTPlayerOwner->UTPlayerState->RespawnTime > 0.0f)
			{
				FFormatNamedArguments Args;
				uint32 WaitTime = uint32(UTHUDOwner->UTPlayerOwner->UTPlayerState->RespawnTime) + 1;
				Args.Add("RespawnTime", FText::AsNumber(WaitTime));
				SpectatorMessage = FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator","RepsawnWaitMessage","You can respawn in {RespawnTime}..."),Args);
			}
			else
			{
				SpectatorMessage = NSLOCTEXT("UUTHUDWidget_Spectator","RepsawnMessage","Press [FIRE] to respawn...");
			}
		}


		float XL, YL;
		Canvas->StrLen(UTHUDOwner->MediumFont, SpectatorMessage.ToString(), XL, YL);
		DrawTexture(Canvas->DefaultTexture, 0,0,Canvas->ClipX, YL * RenderScale,0,0,1,1,1.0, FLinearColor(0.08,0.28,0.60,1.0));

		// Draw the Unreal Symbol

		float H = 69.0 * RenderScale;
		float W = H * (82.0/69.0);

		DrawTexture(UTHUDOwner->OldHudTexture, Canvas->ClipX * 0.05, YL * 0.5f, W, H, 734,190, 82,70, 1.0f, FLinearColor::White, FVector2D(0.0f,0.5f));
		DrawText(SpectatorMessage, Canvas->ClipX * 0.05 + W*1.1, 0 , UTHUDOwner->MediumFont, false, FVector2D(0,0), FLinearColor::Black, false, FLinearColor::Black, RenderScale);
	}
}

