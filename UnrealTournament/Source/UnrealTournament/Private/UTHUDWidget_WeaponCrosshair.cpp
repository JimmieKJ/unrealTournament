// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_WeaponCrosshair.h"

UUTHUDWidget_WeaponCrosshair::UUTHUDWidget_WeaponCrosshair(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Size=FVector2D(0.0f,0.0f);
	ScreenPosition=FVector2D(0.5f, 0.5f);
}

void UUTHUDWidget_WeaponCrosshair::Draw_Implementation(float DeltaTime)
{
	if (UTHUDOwner != NULL && UTHUDOwner->UTPlayerOwner != NULL)
	{
		AUTCharacter* UTCharacter = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
		if (UTCharacter)
		{
			if (UTCharacter->GetWeapon() != NULL)
			{
				UTCharacter->GetWeapon()->DrawWeaponCrosshair(this, DeltaTime);
			}
			else
			{
				// fall back crosshair
				UTexture2D* CrosshairTexture = UTHUDOwner->DefaultCrosshairTex;
				if (CrosshairTexture != NULL)
				{
					float W = CrosshairTexture->GetSurfaceWidth();
					float H = CrosshairTexture->GetSurfaceHeight();
					float CrosshairScale = UTHUDOwner->GetCrosshairScale();

					DrawTexture(CrosshairTexture, 0, 0, W * CrosshairScale, H * CrosshairScale, 0.0, 0.0, 16, 16, 1.0, UTHUDOwner->GetCrosshairColor(FLinearColor::White), FVector2D(0.5f, 0.5f));
				}
			}
		}
	}
}

