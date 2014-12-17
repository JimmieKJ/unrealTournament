// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_Paperdoll.h"
#include "UTJumpBoots.h"
#include "UTArmor.h"

UUTHUDWidget_Paperdoll::UUTHUDWidget_Paperdoll(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture> HudTexture(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));

	Position=FVector2D(5.0f, -5.0f);
	Size=FVector2D(205.0f,111.0f);
	ScreenPosition=FVector2D(0.0f, 1.0f);
	Origin=FVector2D(0.0f,1.0f);
}

void UUTHUDWidget_Paperdoll::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);
	HealthText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_Paperdoll::GetPlayerHealth_Implementation);
	ArmorText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_Paperdoll::GetPlayerArmor_Implementation);

	LastHealth = 100;
	LastArmor = 0;

	ArmorFlashTimer = 0.0f;
	HealthFlashTimer = 0.0f;

}

FText UUTHUDWidget_Paperdoll::GetPlayerHealth_Implementation()
{
	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	if (UTC != NULL && !UTC->IsDead())
	{
		return FText::AsNumber(UTC->Health);
	}

	return FText::AsNumber(0);
}

FText UUTHUDWidget_Paperdoll::GetPlayerArmor_Implementation()
{
	return FText::AsNumber(PlayerArmor);
}

void UUTHUDWidget_Paperdoll::ProcessArmor()
{
	PlayerArmor = 0;
	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	if (UTC != NULL && !UTC->IsDead())
	{
		bool bHasShieldBelt = false;
		bool bHasThighPads = false;
		bool bHasChest = false;
		bool bHasHelmet = false;
		bool bHasJumpBoots = false;
		for (TInventoryIterator<> It(UTC); It; ++It)
		{
			AUTArmor* Armor = Cast<AUTArmor>(*It);
			if (Armor != NULL)
			{
				PlayerArmor += Armor->ArmorAmount;

				if (Armor->ArmorType == FName(TEXT("ShieldBelt"))) bHasShieldBelt = true;
				if (Armor->ArmorType == FName(TEXT("ThighPads"))) bHasThighPads = true;
				if (Armor->ArmorType == FName(TEXT("FlakVest"))) bHasChest = true;
				if (Armor->ArmorType == FName(TEXT("Helmet"))) bHasHelmet = true;
			}
			else if (Cast<AUTJumpBoots>(*It) != NULL) bHasJumpBoots = true;
		} 

		PaperDoll_ShieldBeltOverlay.bHidden = !bHasShieldBelt;
		PaperDoll_ChestArmorOverlay.bHidden = !bHasChest;
		PaperDoll_HelmetOverlay.bHidden = !bHasHelmet;
		PaperDoll_ThighPadArmorOverlay.bHidden = !bHasThighPads;
		PaperDoll_BootsOverlay.bHidden = !bHasJumpBoots;
	}

}




void UUTHUDWidget_Paperdoll::Draw_Implementation(float DeltaTime)
{
	ProcessArmor();

	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	UUTHUDWidget_Paperdoll* DefObj = GetClass()->GetDefaultObject<UUTHUDWidget_Paperdoll>();

	if (UTC != NULL && !UTC->IsDead())
	{
		if (PlayerArmor != LastArmor)
		{
			ArmorText.RenderColor = (PlayerArmor > LastArmor) ? ArmorPositiveFlashColor : ArmorNegativeFlashColor;
			ArmorFlashTimer = ArmorFlashTime;		
			LastArmor = PlayerArmor;
		}
		else if (ArmorFlashTimer > 0.0f)
		{
			ArmorFlashTimer = ArmorFlashTimer - DeltaTime;
			ArmorText.RenderColor = FMath::CInterpTo(ArmorText.RenderColor, DefObj->ArmorText.RenderColor, DeltaTime, (1.0 / (ArmorFlashTime > 0 ? ArmorFlashTime : 1.0)));
		}
		else
		{
			ArmorText.RenderColor = DefObj->ArmorText.RenderColor;
		}

		if (UTC->Health != LastHealth)
		{
			HealthText.RenderColor = (UTC->Health > LastHealth) ? HealthPositiveFlashColor : HealthNegativeFlashColor;
			HealthFlashTimer = HealthFlashTime;		
			LastHealth = UTC->Health;
		}
		else if (HealthFlashTimer > 0.0f)
		{
			HealthFlashTimer = HealthFlashTimer - DeltaTime;
			HealthText.RenderColor = FMath::CInterpTo(HealthText.RenderColor, DefObj->HealthText.RenderColor, DeltaTime, (1.0 / (HealthFlashTime > 0 ? HealthFlashTime : 1.0)));
		}
		else
		{
			HealthText.RenderColor = DefObj->HealthText.RenderColor;
		}
	}

	Super::Draw_Implementation(DeltaTime);
}

