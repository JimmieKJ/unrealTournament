// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_Paperdoll.h"

UUTHUDWidget_Paperdoll::UUTHUDWidget_Paperdoll(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	static ConstructorHelpers::FObjectFinder<UTexture> HudTexture(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));
	PaperDollTexture = HudTexture.Object;

	Position=FVector2D(5.0f, -5.0f);
	Size=FVector2D(205.0f,111.0f);
	ScreenPosition=FVector2D(0.0f, 1.0f);
	Origin=FVector2D(0.0f,1.0f);

	LastHealth = 100;

}

void UUTHUDWidget_Paperdoll::Draw_Implementation(float DeltaTime)
{
	AUTCharacter* UTC = UTHUDOwner->UTPlayerOwner->GetUTCharacter();
	if (UTC)
	{
		int32 Health = UTC->Health;
		if (Health != LastHealth)
		{
			HealthFlashOpacity = 1.0f;
		}

		FLinearColor BGColor = FLinearColor::White;
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTC->PlayerState);
		if (PS != NULL && PS->Team != NULL)
		{
			BGColor = PS->Team->TeamColor;
		}

		LastHealth = Health;
		DrawTexture(PaperDollTexture, 0,0, 205.0f, 111.0f, 10.0f, 53.0f, 205.0f, 111.0f, 1.0f, BGColor);
		DrawTexture(PaperDollTexture, 0,0, 64.0f, 111.0, 1.0f, 223.0f, 64.0f, 111.0f, 1.0f, BGColor);

		int32 ArmorAmt = 0;
		for (AUTInventory* Inv = UTC->GetInventory(); Inv != NULL; Inv = Inv->GetNext())
		{
			AUTArmor* Armor = Cast<AUTArmor>(Inv);
			if (Armor != NULL)
			{
				ArmorAmt += Armor->ArmorAmount;
			}
		}

		DrawTexture(PaperDollTexture, 58.0f, 26.0f, 26.0f, 33.0f, 233.0f, 67.0f, 26.0f, 33.0f);
		UTHUDOwner->TempDrawNumber(ArmorAmt, RenderPosition.X + 85 * RenderScale, RenderPosition.Y + 26 * RenderScale, FLinearColor::Yellow, 0.5, RenderScale * 0.5);

		DrawTexture(PaperDollTexture, 54.0f, 70.0f, 35.0f, 35.0f, 225.0f, 104.0f, 35.0f,35.0f);
		UTHUDOwner->TempDrawNumber(UTC->Health, RenderPosition.X + 85 * RenderScale , RenderPosition.Y + 70 * RenderScale, FLinearColor::Yellow, 0.5 + (0.5 * HealthFlashOpacity),RenderScale * 0.75);
	
		if (HealthFlashOpacity > 0.0f)
		{
			HealthFlashOpacity = FMath::Max<float>(0.0f, HealthFlashOpacity - DeltaTime * 1.25);
		}
	}
}

