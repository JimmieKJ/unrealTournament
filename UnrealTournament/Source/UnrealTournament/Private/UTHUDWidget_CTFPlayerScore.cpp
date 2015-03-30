// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_CTFPlayerScore.h"

UUTHUDWidget_CTFPlayerScore::UUTHUDWidget_CTFPlayerScore(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	Size=FVector2D(181.0f,43.0f);
	ScreenPosition=FVector2D(1.0f, 0.0f);
	Origin=FVector2D(1.0f,0.0f);
}

void UUTHUDWidget_CTFPlayerScore::Draw_Implementation(float DeltaTime)
{
	if ( UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->UTPlayerState)
	{
		int32 Score = int32(UTHUDOwner->UTPlayerOwner->UTPlayerState->Score);

		if (Score != LastScore) ScoreFlashOpacity = 1.0f;
		LastScore = Score;

		DrawTexture(HudTexture, 0.0f,0.0f, 181.0f,43.0f, 491.0f,396.0f,181.0f,43.0f,1.0f,ApplyHUDColor(FLinearColor::White));

		FNumberFormattingOptions NumberOpts;
		NumberOpts.MaximumIntegralDigits=6;
		NumberOpts.MinimumIntegralDigits=6;
		FText ScoreText = FText::AsNumber(Score, &NumberOpts);

		DrawText(ScoreText, 90, 26, UTHUDOwner->NumberFont, 0.5 + (0.1 * ScoreFlashOpacity), 1.0, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Center);
		if (ScoreFlashOpacity > 0.0f) ScoreFlashOpacity = FMath::Max<float>(0.0f, ScoreFlashOpacity - DeltaTime * 1.25f);
	}
}