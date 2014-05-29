// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_Paperdoll.h"

UUTHUDWidget_Paperdoll::UUTHUDWidget_Paperdoll(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	static ConstructorHelpers::FObjectFinder<UTexture> HudTexture(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));
	PaperDollTexture = HudTexture.Object;

	Position=FVector2D(5.0f, -5.0f);
	Size=FVector2D(400.0f,166.0f);
	ScreenPosition=FVector2D(0.0f, 1.0f);
	Origin=FVector2D(0.0f,1.0f);

	LastHealth = 100;

}

void UUTHUDWidget_Paperdoll::Draw(float DeltaTime)
{
	if (UTHUDOwner->UTCharacterOwner)
	{
		int32 Health = UTHUDOwner->UTCharacterOwner->Health;
		if (Health != LastHealth)
		{
			HealthFlashOpacity = 1.0f;
		}

		LastHealth = Health;

		DrawTexture(PaperDollTexture, 0,0, 97.5f, 166.5f, 0.0f, 223.0f, 65.0f, 111.0f);
		DrawTexture(PaperDollTexture, 100.0f, 92.0f, 35.0f, 35.0f, 225.0f, 104.0f, 35.0f,35.0f,0.5f);
		DrawTexture(PaperDollTexture, 105.0f, 132.0f, 26.0f, 33.0f, 233.0f, 67.0f, 26.0f, 33.0f,0.5f);
		UTHUDOwner->TempDrawNumber(UTHUDOwner->UTCharacterOwner->Health, RenderPosition.X + 140 * RenderScale , RenderPosition.Y + 92 * RenderScale, FLinearColor::Yellow, HealthFlashOpacity,RenderScale * 0.75);
		UTHUDOwner->TempDrawNumber(0, RenderPosition.X + 135 * RenderScale , RenderPosition.Y + 135 * RenderScale, FLinearColor::Yellow, 0.0, RenderScale * 0.5);
	
		if (HealthFlashOpacity > 0.0f)
		{
			HealthFlashOpacity = FMath::Max<float>(0.0f, HealthFlashOpacity - DeltaTime * 1.25);
		}
	}
}

