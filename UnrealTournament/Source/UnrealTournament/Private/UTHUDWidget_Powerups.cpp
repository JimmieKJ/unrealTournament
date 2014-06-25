// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_Powerups.h"
#include "UTTimedPowerup.h"

UUTHUDWidget_Powerups::UUTHUDWidget_Powerups(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	Position = FVector2D(-5.0f, -5.0f);
	Size = FVector2D(200.0f, 160.0f);
	ScreenPosition = FVector2D(1.0f, 0.5f);
	Origin = FVector2D(1.0f, 0.5f);
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
			// calculate size
			FVector2D RelativeSize(0.f, 0.f);
			int32 Count = 0;
			for (AUTInventory* Inv = UTCharacter->GetInventory(); Inv != NULL; Inv = Inv->GetNext())
			{
				if (Inv->IsA(AUTTimedPowerup::StaticClass()))
				{
					RelativeSize += MaxPowerupSize;
					Count++;
				}
			}
			if (RelativeSize.X > 1.0f || RelativeSize.Y > 1.0f)
			{
				RelativeSize /= FMath::Max<float>(RelativeSize.X, RelativeSize.Y);
			}
			RelativeSize /= Count;

			FVector2D CurrentPos(0.f, 0.f);
			FVector2D ItemSize(RelativeSize * GetRenderSize());
			for (AUTInventory* Inv = UTCharacter->GetInventory(); Inv != NULL; Inv = Inv->GetNext())
			{
				if (Inv->IsA(AUTTimedPowerup::StaticClass()))
				{
					Inv->DrawInventoryHUD(this, CurrentPos, ItemSize);
					// advance in rows or columns depending on settings
					if (RelativeSize.X >= RelativeSize.Y)
					{
						CurrentPos.Y += ItemSize.Y;
					}
					else
					{
						CurrentPos.X += ItemSize.X;
					}
				}
			}
		}
	}
}