// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_LivesRemaining.h"
#include "UTCTFGameState.h"

UUTHUDWidget_LivesRemaining::UUTHUDWidget_LivesRemaining(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Position = FVector2D(-5.0f, 5.0f);
	Size = FVector2D(114.0f, 43.0f);
	ScreenPosition = FVector2D(1.0f, 0.0f);
	Origin = FVector2D(1.0f, 0.0f);
}

void UUTHUDWidget_LivesRemaining::Draw_Implementation(float DeltaTime)
{
	AUTCTFGameState* GS = Cast<AUTCTFGameState>(UTGameState);
	bool bTeamLifePool = false;
	if (GS && GS->BlueLivesRemaining > 0)
	{
		bTeamLifePool = true;
		DrawTexture(UTHUDOwner->HUDAtlas, 0.f, 0.f, 40.f, 40.f, 725.f, 0.f, 28.f, 36.f, 1.f, FLinearColor::Blue);
		DrawText(FText::AsNumber(FMath::Max(0, GS->BlueLivesRemaining)), 60.f, 18.f, UTHUDOwner->MediumFont, 1.f, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
	}
	if (GS && GS->RedLivesRemaining > 0)
	{
		float YOffset = bTeamLifePool ? 68.f : 18.f;
		DrawTexture(UTHUDOwner->HUDAtlas, 0.f, YOffset, 40.f, 40.f, 725.f, 0.f, 28.f, 36.f, 1.f, FLinearColor::Red);
		DrawText(FText::AsNumber(FMath::Max(0, GS->RedLivesRemaining)), 60.f, YOffset, UTHUDOwner->MediumFont, 1.f, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		bTeamLifePool = true;
	}

	AUTPlayerState* PS = UTHUDOwner->GetScorerPlayerState();
	if (!bTeamLifePool && PS != NULL && !PS->bOnlySpectator && (PS->RemainingLives > 0))
	{
		DrawTexture(UTHUDOwner->HUDAtlas, 0.f, 0.f, 40.f, 40.f, 725.f, 0.f, 28.f, 36.f, 1.f, FLinearColor::White);
		DrawText(FText::AsNumber(FMath::Max(0,PS->RemainingLives)), 60.f, 18.f, UTHUDOwner->MediumFont, 1.f, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
	}
}