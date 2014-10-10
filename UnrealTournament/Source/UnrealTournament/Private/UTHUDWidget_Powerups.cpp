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
		AUTCharacter* UTCharacter = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
		if (UTCharacter != NULL)
		{
			int32 Count = 0;
			FVector2D TotalSize;
			for (TInventoryIterator<AUTTimedPowerup> It(UTCharacter); It; ++It)
			{
				TotalSize.Y += It->HUDIcon.VL;
				TotalSize.X = FMath::Max<float>(It->HUDIcon.UL, TotalSize.X);
				Count++;
			}

			FVector2D CurrentPos(0.f, TotalSize.X * -0.5);
			for (TInventoryIterator<AUTTimedPowerup> It(UTCharacter); It; ++It)
			{
				FVector2D ItemSize = FVector2D(It->HUDIcon.UL, It->HUDIcon.VL);
				It->DrawInventoryHUD(this, CurrentPos, ItemSize);
				CurrentPos.Y += ItemSize.Y;
			}
		}
	}
}