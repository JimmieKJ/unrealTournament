// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_WeaponInfo.h"

UUTHUDWidget_WeaponInfo::UUTHUDWidget_WeaponInfo(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Position=FVector2D(-5.0f, -5.0f);
	Size=FVector2D(0.0f,0.0f);
	ScreenPosition=FVector2D(1.0f, 1.0f);
	Origin=FVector2D(0.0f,0.0f);

	AmmoFlashColor = FLinearColor::White;
	WeaponChangeFlashColor = FLinearColor::White;
	AmmoFlashTime = 0.5;
}

void UUTHUDWidget_WeaponInfo::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);

	LastWeapon = NULL;
	LastAmmoAmount = 0;
	AmmoText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_WeaponInfo::GetAmmoAmount_Implementation);
}


void UUTHUDWidget_WeaponInfo::Draw_Implementation(float DeltaTime)
{
	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	UUTHUDWidget_WeaponInfo* DefObj = GetClass()->GetDefaultObject<UUTHUDWidget_WeaponInfo>();

	if (UTC)	
	{
		bool bCalcFlash = true;
		AUTWeapon* CurrentWeapon = UTC->GetWeapon();
		if (CurrentWeapon)
		{
			if (CurrentWeapon != LastWeapon)
			{
				FlashTimer = AmmoFlashTime;
				AmmoText.RenderColor = WeaponChangeFlashColor;
				LastAmmoAmount = CurrentWeapon->Ammo;
				bCalcFlash = false;
			}
			
			LastWeapon = CurrentWeapon;
		}

		if (LastWeapon)
		{
			if (LastAmmoAmount < LastWeapon->Ammo)
			{
				FlashTimer = AmmoFlashTime;
				AmmoText.RenderColor = AmmoFlashColor;
				bCalcFlash = false;
			}

			LastAmmoAmount = LastWeapon->Ammo;
		}


		if (FlashTimer > 0)
		{
			if (bCalcFlash)
			{
				FlashTimer = FlashTimer - DeltaTime;
				AmmoText.RenderColor = FMath::CInterpTo(AmmoText.RenderColor, DefObj->AmmoText.RenderColor, DeltaTime, (1.0 / (AmmoFlashTime > 0 ? AmmoFlashTime : 1.0)));
			}
		}
		else
		{
			AmmoText.RenderColor = DefObj->AmmoText.RenderColor;
		}
	}

	Super::Draw_Implementation(DeltaTime);
}

FText UUTHUDWidget_WeaponInfo::GetAmmoAmount_Implementation()
{
	return FText::AsNumber(LastAmmoAmount);
}

