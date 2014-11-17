// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_WeaponInfo.h"

UUTHUDWidget_WeaponInfo::UUTHUDWidget_WeaponInfo(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Position=FVector2D(-5.0f, -5.0f);
	Size=FVector2D(0.0f,0.0f);
	ScreenPosition=FVector2D(1.0f, 1.0f);
	Origin=FVector2D(0.0f,0.0f);

}

void UUTHUDWidget_WeaponInfo::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);
	if (UTHUDOwner != NULL && UTHUDOwner->UTPlayerOwner != NULL)
	{
		AUTCharacter* UTCharacter = UTHUDOwner->UTPlayerOwner->GetUTCharacter();
		if (UTCharacter != NULL && UTCharacter->GetWeapon() != NULL)
		{
			UTCharacter->GetWeapon()->DrawWeaponInfo(this, DeltaTime);
		}
	}
}

