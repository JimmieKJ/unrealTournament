// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_Paperdoll.h"
#include "UTJumpBoots.h"
#include "UTArmor.h"

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
	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	if (UTC != NULL && !UTC->IsDead())
	{
		int32 Health = UTC->Health;
		if (Health != LastHealth)
		{
			HealthFlashOpacity = 1.0f;
		}

		FLinearColor BGColor = ApplyHUDColor(FLinearColor::White);

		LastHealth = Health;
		DrawTexture(PaperDollTexture, 0,0, 205.0f, 111.0f, 10.0f, 53.0f, 205.0f, 111.0f, 1.0f, BGColor);
		DrawTexture(PaperDollTexture, 0,0, 64.0f, 111.0, 1.0f, 223.0f, 64.0f, 111.0f, 1.0f, BGColor);

		int32 ArmorAmt = 0;
		bool bHasShieldBelt = false;
		bool bHasThighPads = false;
		bool bHasChest = false;
		bool bHasHelmet = false;
		int JumpBootsCharges = 0;
		for (TInventoryIterator<> It(UTC); It; ++It)
		{
			AUTArmor* Armor = Cast<AUTArmor>(*It);
			if (Armor != NULL)
			{
				ArmorAmt += Armor->ArmorAmount;

				if (Armor->ArmorType == FName(TEXT("ShieldBelt"))) bHasShieldBelt = true;
				if (Armor->ArmorType == FName(TEXT("ThighPads"))) bHasThighPads = true;
				if (Armor->ArmorType == FName(TEXT("FlakVest"))) bHasChest = true;
				if (Armor->ArmorType == FName(TEXT("Helmet"))) bHasHelmet = true;
			}
			else if (Cast<AUTJumpBoots>(*It) != NULL)
			{
				JumpBootsCharges = Cast<AUTJumpBoots>(*It)->NumJumps;
			}
		}

		if (bHasShieldBelt)
		{
			DrawTexture(PaperDollTexture, 0,0, 64.0f, 111.0, 67.0f, 223.0f, 64.0f, 111.0f, 1.0f);
		}

		if (bHasThighPads)
		{
			DrawTexture(PaperDollTexture, 12,53, 40.0f, 28.0, 135.0f, 263.0f, 40.0f, 28.0f, 1.0f);
		}

		if (bHasChest)
		{
			DrawTexture(PaperDollTexture, 10,16, 44.0f, 25.0, 133.0f, 221.0f, 44.0f, 25.0f, 1.0f);
		}

		if (bHasHelmet)
		{
			DrawTexture(PaperDollTexture, 20,-2, 23.0f, 25.0, 192.0f, 265.0f, 23.0f, 25.0f, 1.0f);
		}

		if (JumpBootsCharges)
		{
			DrawTexture(PaperDollTexture, 5,84, 54.0f, 25.0, 222.0f, 263.0f, 54.0f, 25.0f, 1.0f);
			DrawText(FText::AsNumber(JumpBootsCharges), 32, 90, UTHUDOwner->GetFontFromSizeIndex(0), 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Center);
		}

		DrawTexture(PaperDollTexture, 58.0f, 26.0f, 26.0f, 33.0f, 233.0f, 67.0f, 26.0f, 33.0f);
		UTHUDOwner->DrawNumber(ArmorAmt, RenderPosition.X + 85 * RenderScale, RenderPosition.Y + 26 * RenderScale, FLinearColor::Yellow, 0.5, RenderScale * 0.5);

		DrawTexture(PaperDollTexture, 54.0f, 70.0f, 35.0f, 35.0f, 225.0f, 104.0f, 35.0f,35.0f);
		UTHUDOwner->DrawNumber(UTC->Health, RenderPosition.X + 85 * RenderScale , RenderPosition.Y + 70 * RenderScale, FLinearColor::Yellow, 0.5 + (0.5 * HealthFlashOpacity),RenderScale * 0.75);
	
		if (HealthFlashOpacity > 0.0f)
		{
			HealthFlashOpacity = FMath::Max<float>(0.0f, HealthFlashOpacity - DeltaTime * 1.25);
		}
	}
}

