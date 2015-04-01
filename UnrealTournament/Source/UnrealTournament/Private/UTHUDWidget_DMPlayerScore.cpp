// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_DMPlayerScore.h"

UUTHUDWidget_DMPlayerScore::UUTHUDWidget_DMPlayerScore(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture> Tex(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));
	HudTexture = Tex.Object;

	Position=FVector2D(-5.0f, 5.0f);
	Size=FVector2D(114.0f,43.0f);
	ScreenPosition=FVector2D(1.0f, 0.0f);
	Origin=FVector2D(1.0f,0.0f);

	LastScore = 0;
}

void UUTHUDWidget_DMPlayerScore::Draw_Implementation(float DeltaTime)
{
	if (UTHUDOwner->UTPlayerOwner != NULL)
	{
		AUTPlayerState* PS = UTHUDOwner->GetViewedPlayerState();
		if (PS != NULL && !PS->bOnlySpectator)
		{
			float Score = int32(PS->Score);
			float Deaths = PS->Deaths;

			if (Score != LastScore)
			{
				ScoreFlashOpacity = 1.0f;
			}
			LastScore = Score;

			DrawTexture(HudTexture, 0.0f, 0.0f, 114.0f, 43.0f, 375.0f, 396.0f, 114.0f, 43.0f, 1.0f, ApplyHUDColor(FLinearColor::White));
			UTHUDOwner->DrawNumber(Score, RenderPosition.X + 22 * RenderScale, RenderPosition.Y + 7 * RenderScale, FLinearColor::Yellow, ScoreFlashOpacity, RenderScale * 0.5f, 3.0f);
			DrawText(FText::Format(NSLOCTEXT("UTHUD", "Deaths", "{0} Deaths"), FText::AsNumber(Deaths)), 114.0f, 46.0f, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Right);

			if (ScoreFlashOpacity > 0.0f)
			{
				ScoreFlashOpacity = FMath::Max<float>(0.0f, ScoreFlashOpacity - DeltaTime * 1.25f);
			}
		}
	}
}