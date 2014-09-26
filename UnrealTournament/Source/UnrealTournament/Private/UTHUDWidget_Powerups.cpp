// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_Powerups.h"
#include "UTTimedPowerup.h"

UUTHUDWidget_Powerups::UUTHUDWidget_Powerups(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	Position = FVector2D(-5.0f, -5.0f);
	Size = FVector2D(0.0f, 0.0f);
	ScreenPosition = FVector2D(1.0f, 0.5f);
	MaxPowerupSize = FVector2D(1.0f, 0.333f);
}

void UUTHUDWidget_Powerups::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);
	if (UTHUDOwner != NULL && UTHUDOwner->UTPlayerOwner != NULL)
	{
		AUTCharacter* UTCharacter = UTHUDOwner->UTPlayerOwner->GetUTCharacter();
		if (UTCharacter != NULL)
		{
			int32 Count = 0;
			FVector2D TotalSize;
			for (AUTInventory* Inv = UTCharacter->GetInventory(); Inv != NULL; Inv = Inv->GetNext())
			{
				if (Inv->GetOwner() == nullptr)
				{
					break;
				}

				AUTTimedPowerup* PowerUp = Cast<AUTTimedPowerup>(Inv);
				if (PowerUp != NULL)
				{
					TotalSize.Y += PowerUp->HUDIcon.VL;
					TotalSize.X = FMath::Max<float>(PowerUp->HUDIcon.UL, TotalSize.X);
					Count++;
				}
			}

			FVector2D CurrentPos(0.f, TotalSize.X * -0.5);
			for (AUTInventory* Inv = UTCharacter->GetInventory(); Inv != NULL; Inv = Inv->GetNext())
			{
				if (Inv->GetOwner() == nullptr)
				{
					break;
				}

				AUTTimedPowerup* PowerUp = Cast<AUTTimedPowerup>(Inv);
				if (PowerUp != NULL)
				{
					FVector2D ItemSize = FVector2D(PowerUp->HUDIcon.UL, PowerUp->HUDIcon.VL);
					Inv->DrawInventoryHUD(this, CurrentPos, ItemSize);
					CurrentPos.Y += ItemSize.Y;
				}
			}
		}
	}
}