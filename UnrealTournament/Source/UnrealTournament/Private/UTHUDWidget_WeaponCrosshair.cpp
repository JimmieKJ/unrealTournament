// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_WeaponInfo.h"

UUTHUDWidget_WeaponCrosshair::UUTHUDWidget_WeaponCrosshair(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	Size=FVector2D(0.0f,0.0f);
	ScreenPosition=FVector2D(0.5f, 0.5f);
}

void UUTHUDWidget_WeaponCrosshair::Draw_Implementation(float DeltaTime)
{
	if (UTHUDOwner != NULL && UTHUDOwner->UTPlayerOwner != NULL)
	{
		AUTCharacter* UTCharacter = UTHUDOwner->UTPlayerOwner->GetUTCharacter();
		if (UTCharacter != NULL && UTCharacter->GetWeapon() != NULL)
		{
			UTCharacter->GetWeapon()->DrawWeaponCrosshair(this, UTCharacter, DeltaTime);
		}
	}
}

