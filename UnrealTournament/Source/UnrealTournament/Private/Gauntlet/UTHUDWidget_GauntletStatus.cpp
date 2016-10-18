// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_GauntletStatus.h"
#include "UTGauntletGameState.h"


UUTHUDWidget_GauntletStatus::UUTHUDWidget_GauntletStatus(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DesignedResolution=1080.0f;
	Position=FVector2D(0.0f, 0.0f);
	Size=FVector2D(0.0f,0.0f);
	ScreenPosition=FVector2D(0.5f, 0.0f);
	Origin=FVector2D(0.0f,0.0f);
}


void UUTHUDWidget_GauntletStatus::Draw_Implementation(float DeltaTime)
{
	float RemainingTime = UTGameState ? UTGameState->GetClockTime() : 0.f;
	ClockText.Text = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), RemainingTime, false);

	RenderObj_TextureAt(BackgroundSlate, 0.0f, 0.0f, 110.0f, 56.0f);
	RenderObj_TextAt(ClockText, 0.0f, 8.0f);

	AUTGauntletGameState* GauntletGameState = GetWorld()->GetGameState<AUTGauntletGameState>();
	if (GauntletGameState != nullptr)
	{
		int32 FlagTeam = GauntletGameState->Flag != nullptr ? GauntletGameState->Flag->GetTeamNum() : 255;
		if (FlagTeam < 0 || FlagTeam > 1 || !GauntletGameState->Teams.IsValidIndex(FlagTeam))
		{
			FlagIcon.RenderColor = FLinearColor(0.0f,0.45f,0.125f,1.0f);
		}
		else
		{
			FlagIcon.RenderColor = GauntletGameState->Teams[FlagTeam]->TeamColor;
		}

		RenderObj_TextureAt(FlagIcon, 0.0f, 46.0f, 66.0f, 63.0f);

		if (GauntletGameState->RemainingPickupDelay > 0)
		{
			LockIcon.RenderColor = FLinearColor::Black;
			RenderObj_TextureAt(LockIcon, 0.0f, 56.0f, 30.0f, 42.0f);
			LockIcon.RenderColor = FLinearColor::Yellow;
			RenderObj_TextureAt(LockIcon, 0.0f, 58.0f, 26.0f, 38.0f);
		}

		float X = 120.0f;

		for (int32 i=0; i < 3; i++)
		{
			int32 Target = 2 - i;

			// Red team first
			if (GauntletGameState->Teams[0]->Score > Target)
			{
				StarIcon.RenderColor = GauntletGameState->Teams[0]->TeamColor;
				RenderObj_TextureAt(StarIcon, (X * -1), 16.0f, 50.0f, 48.0f);
			}
			else
			{
				EmptyStarIcon.RenderColor = GauntletGameState->Teams[0]->TeamColor;
				RenderObj_TextureAt(EmptyStarIcon, (X * -1), 16.0f, 50.0f, 48.0f);
			}

			// Now blue

			if (GauntletGameState->Teams[1]->Score > Target)
			{
				StarIcon.RenderColor = GauntletGameState->Teams[1]->TeamColor;
				RenderObj_TextureAt(StarIcon, X, 16.0f, 50.0f, 48.0f);
			}
			else
			{
				EmptyStarIcon.RenderColor = GauntletGameState->Teams[1]->TeamColor;
				RenderObj_TextureAt(EmptyStarIcon, X, 16.0f, 50.0f, 48.0f);
			}
			X += 50.0f;
		}
	}
}