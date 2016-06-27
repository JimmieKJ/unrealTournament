// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	FlashTimer = 0.f;
	AmmoText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_WeaponInfo::GetAmmoAmount);
}

bool UUTHUDWidget_WeaponInfo::ShouldDraw_Implementation(bool bShowScores)
{
	if (!UTHUDOwner->GetQuickStatsHidden())
	{
		return false;
	}
	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
	return Super::ShouldDraw_Implementation(bShowScores) && (UTC && UTC->GetWeapon() && UTC->GetWeapon()->NeedsAmmoDisplay());
}

void UUTHUDWidget_WeaponInfo::Draw_Implementation(float DeltaTime)
{
	AUTCharacter* UTC = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());

	if (UTC)	
	{
		UUTHUDWidget_WeaponInfo* DefObj = GetClass()->GetDefaultObject<UUTHUDWidget_WeaponInfo>();
		AUTWeapon* CurrentWeapon = UTC->GetWeapon();
		if (CurrentWeapon)
		{
			if (CurrentWeapon != LastWeapon)
			{
				FlashTimer = AmmoFlashTime;
				AmmoText.RenderColor = WeaponChangeFlashColor;
				LastAmmoAmount = CurrentWeapon->Ammo;
			}
		
			LastWeapon = CurrentWeapon;
		}

		if (LastWeapon)
		{
			if (LastAmmoAmount < LastWeapon->Ammo)
			{
				FlashTimer = AmmoFlashTime;
				AmmoText.RenderColor = AmmoFlashColor;
				AmmoText.TextScale = 2.f;
			}
			LastAmmoAmount = LastWeapon->Ammo;
		}

		if (FlashTimer > 0.f)
		{
			FlashTimer = FlashTimer - DeltaTime;
			if (FlashTimer < 0.5f* AmmoFlashTime)
			{
				AmmoText.RenderColor = FMath::CInterpTo(AmmoText.RenderColor, DefObj->AmmoText.RenderColor, DeltaTime, (1.f / (AmmoFlashTime > 0.f ? 2.f*AmmoFlashTime : 1.f)));
			}
			AmmoText.TextScale = 1.f + FlashTimer / AmmoFlashTime;
		}
		else
		{
			AmmoText.RenderColor = DefObj->AmmoText.RenderColor;
			AmmoText.TextScale = 1.f;
		}
	}

	Super::Draw_Implementation(DeltaTime);
}

FText UUTHUDWidget_WeaponInfo::GetAmmoAmount_Implementation()
{
	return FText::AsNumber(LastAmmoAmount);
}

