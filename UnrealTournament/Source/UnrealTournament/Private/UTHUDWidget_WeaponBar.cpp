// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_WeaponBar.h"
#include "UTWeapon.h"

UUTHUDWidget_WeaponBar::UUTHUDWidget_WeaponBar(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	static ConstructorHelpers::FObjectFinder<UTexture> HudTexture(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));
	OldHudTexture = HudTexture.Object;

	static ConstructorHelpers::FObjectFinder<UTexture> WeaponTexture(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseB.UI_HUD_BaseB'"));
	OldWeaponTexture = WeaponTexture.Object;
	
	
	Position=FVector2D(0.0f, -5.0f);
	Size=FVector2D(0,0);
	ScreenPosition=FVector2D(0.5f, 1.0f);
	Origin=FVector2D(0.0f,0.0f);

	WeaponScaleSpeed = 10;
	BouncedWeapon = -1;

	BounceWeaponScale=1.5;
	SelectedWeaponScale=1.35;

	CellWidth = 72;		
	CellHeight = 48;	

	AmmoBarOffset = FVector2D(18.0f,-13.0f);
	AmmoBarSize = FVector2D(34.0f, 4.0f);
	SlotOffset = FVector2D(9.0f, -15.0f);
	for (int i=0;i<10;i++) CurrentWeaponScale[i] = 1.0f;

	MaxIconSize = FVector2D(80, 56);

}

void UUTHUDWidget_WeaponBar::Draw_Implementation(float DeltaTime)
{
	AUTCharacter* UTC = UTHUDOwner->UTPlayerOwner->GetUTCharacter();
	if (UTC)
	{

		FLinearColor HudColor = ApplyHUDColor(FLinearColor::White);

		AUTWeapon *WeaponList[10];
		for (int i=0;i<10;i++) WeaponList[i] = NULL;

		int32 FirstWeaponIndex = 11; 
		int32 LastWeaponIndex = -1;
		int32 CellCount = 0;

		// Get the weapon list.
		for (AUTInventory* Inv = UTC->GetInventory(); Inv != NULL; Inv = Inv->GetNext())
		{
			AUTWeapon* Weapon = Cast<AUTWeapon>(Inv);
			if (Weapon != NULL && Weapon->Group > 0 && Weapon->Group < 11)
			{
				int32 WeaponGroup = Weapon->Group-1;

				// Count if needed
				if (WeaponList[WeaponGroup] == NULL) CellCount++;

				// Store off - NOTE: if a weapon already exists in that group.. it will get blown out.  This implementation
				// doesn't support stacking.

				WeaponList[WeaponGroup] = Weapon;								

				// Move the first and last if needed
				if (WeaponGroup < FirstWeaponIndex) FirstWeaponIndex = WeaponGroup;
				if (WeaponGroup > LastWeaponIndex) LastWeaponIndex = WeaponGroup;
			}
		}

		if (CellCount <= 0) return;	// Quick out if we do not have any weapons

		AUTWeapon* PendingWeapon = UTC->GetPendingWeapon();
		AUTWeapon* Weapon = UTC->GetWeapon();

		// Figure out the selected weapon
		int32 SelectedWeaponIndex = (PendingWeapon != NULL ? PendingWeapon->Group : ( Weapon != NULL ? Weapon->Group : -1));
		if (SelectedWeaponIndex > 0) SelectedWeaponIndex--;
		
		float Delta = WeaponScaleSpeed * DeltaTime;
		float BarWidth = 0;
		float BarHeight = 0;

		int32 PrevWeaponIndex = -1;
		int32 WeaponIndex = -1;
		int32 NextWeaponIndex = -1;

		float DesiredWeaponScale[10];

		for (int i=FirstWeaponIndex; i<= LastWeaponIndex; i++)
		{
			// Sanity
			if (WeaponList[i] != NULL)
			{
				if (SelectedWeaponIndex == i)	// This is the selected weapon
				{
					PrevWeaponIndex = WeaponIndex;
					if (BouncedWeapon == i)
					{
						DesiredWeaponScale[i] = SelectedWeaponScale;
					}
					else
					{
						DesiredWeaponScale[i] = BounceWeaponScale;
						if (CurrentWeaponScale[i] > DesiredWeaponScale[i])
						{
							BouncedWeapon = i;
						}
					}
				}
				else
				{
					if (WeaponIndex == SelectedWeaponIndex)
					{
						NextWeaponIndex = i;
					}
					DesiredWeaponScale[i] = 1.0f;
				}
			
				if (CurrentWeaponScale[i] != DesiredWeaponScale[i])
				{
					if (DesiredWeaponScale[i] > CurrentWeaponScale[i])
					{
						CurrentWeaponScale[i] = FMath::Min<float>(CurrentWeaponScale[i]+Delta , DesiredWeaponScale[i]);
					}
					else
					{
						CurrentWeaponScale[i] = FMath::Max<float>(CurrentWeaponScale[i]-Delta , DesiredWeaponScale[i]);
					}
				}

				BarWidth += CellWidth * CurrentWeaponScale[i];
				BarHeight = FMath::Max<float>(BarHeight, CellHeight * CurrentWeaponScale[i]);
				WeaponIndex = i;
			}
		}

		// Draw the bar

		float X = BarWidth * -0.5f;
		for (int i=FirstWeaponIndex; i<= LastWeaponIndex; i++)
		{
			if (WeaponList[i] != NULL)
			{
				float NegativeScale = CurrentWeaponScale[i] * -1;

				if (i == SelectedWeaponIndex)
				{
					DrawTexture(OldHudTexture, X, CellHeight * NegativeScale, CellWidth * CurrentWeaponScale[i], CellHeight * CurrentWeaponScale[i], 530,248,69,49,1.0, HudColor);
					DrawTexture(OldHudTexture, X, CellHeight * NegativeScale, CellWidth * CurrentWeaponScale[i], CellHeight * CurrentWeaponScale[i], 459,148,69,49,1.0, HudColor);
					DrawTexture(OldHudTexture, X, CellHeight * NegativeScale, CellWidth * CurrentWeaponScale[i], CellHeight * CurrentWeaponScale[i], 459,248,69,49,1.0, HudColor);
					
					// Draw ammo bar ticks...

					//FVector2D BarOffset = AmmoBarOffset * CurrentWeaponScale[i];
					//FVector2D BarSize = AmmoBarSize * CurrentWeaponScale[i];

					//DrawTexture(OldHudTexture, X + BarOffset.X, BarOffset.Y, BarSize.X, BarSize.Y, 407,479, AmmoBarSize.X, AmmoBarSize.Y);
				}
				else
				{
					DrawTexture(OldHudTexture, X, CellHeight * NegativeScale, CellWidth * CurrentWeaponScale[i], CellHeight * CurrentWeaponScale[i], 459,148,69,49,0.6f,HudColor);
					if (i == PrevWeaponIndex)
					{
						DrawTexture(OldHudTexture, X, CellHeight * NegativeScale, CellWidth * CurrentWeaponScale[i], CellHeight * CurrentWeaponScale[i], 530,97,69,49,0.6f,HudColor);
					}
						
					if (i == NextWeaponIndex)
					{
						DrawTexture(OldHudTexture, X, CellHeight * NegativeScale, CellWidth * CurrentWeaponScale[i], CellHeight * CurrentWeaponScale[i], 530,148,69,49,0.6f,HudColor);
					}
			
				}

				X += CellWidth * CurrentWeaponScale[i];
			}
		}

		// Draw the Weapon Icons
		X = BarWidth * -0.5f;
		for (int i=FirstWeaponIndex; i<= LastWeaponIndex; i++)
		{
			if (WeaponList[i] != NULL)
			{
				if (WeaponList[i]->IconCoordinates.UL > 0 && WeaponList[i]->IconCoordinates.VL > 0)
				{
					float IconX = X + (CellWidth * CurrentWeaponScale[i]) * 0.5;
					float IconY = (CellHeight * CurrentWeaponScale[i]) * -0.5;

					float IconWidth = WeaponList[i]->IconCoordinates.UL;
					float IconHeight = WeaponList[i]->IconCoordinates.VL;

					if (IconWidth > MaxIconSize.X || IconHeight > MaxIconSize.Y)
					{
						if (IconWidth - MaxIconSize.X > IconHeight - MaxIconSize.Y)
						{
							IconHeight = MaxIconSize.X * (IconHeight / IconWidth);
							IconWidth = MaxIconSize.X;
						}
						else
						{
							IconWidth = MaxIconSize.Y * (IconWidth / IconHeight);
							IconHeight = MaxIconSize.Y;
						}
					}

					IconWidth *= CurrentWeaponScale[i];
					IconHeight *= CurrentWeaponScale[i];

					DrawTexture(OldWeaponTexture, IconX, IconY, IconWidth, IconHeight
										,WeaponList[i]->IconCoordinates.U, WeaponList[i]->IconCoordinates.V, WeaponList[i]->IconCoordinates.UL, WeaponList[i]->IconCoordinates.VL 
										,1.0, FLinearColor::White, FVector2D(0.5f,0.5f),15, FVector2D(0.5,0.5));
				}

				X += CellWidth * CurrentWeaponScale[i];
			}
		}



		// Draw the Ammo Bars
		X = BarWidth * -0.5f;
		for (int i=FirstWeaponIndex; i<= LastWeaponIndex; i++)
		{
			if (WeaponList[i] != NULL)
			{
				if  (WeaponList[i]->Ammo > 0 && WeaponList[i]->MaxAmmo > 0)
				{
					FVector2D BarOffset = AmmoBarOffset * CurrentWeaponScale[i];
					FVector2D BarSize = (i == SelectedWeaponIndex) ?  AmmoBarSize * CurrentWeaponScale[i] : FVector2D(AmmoBarSize.X,2.0f);

					float Perc = float(WeaponList[i]->Ammo) / float(WeaponList[i]->MaxAmmo);
					DrawTexture(Canvas->DefaultTexture, X + BarOffset.X, BarOffset.Y, BarSize.X, BarSize.Y,0,0,1,1,1.0, FLinearColor::White);
					DrawTexture(Canvas->DefaultTexture, X + BarOffset.X-1, BarOffset.Y-1, BarSize.X * Perc+2, BarSize.Y+2,0,0,1,1,1.0, FLinearColor(0.0,1.0,0.0,1.0));
					DrawTexture(Canvas->DefaultTexture, X + BarOffset.X, BarOffset.Y, BarSize.X * Perc, BarSize.Y,0,0,1,1,1.0, FLinearColor(0.0,0.5,0.0,1.0));
				}
				X += CellWidth * CurrentWeaponScale[i];
			}
		}

		// Draw the Weapon Numbers
		X = BarWidth * -0.5f;
		UFont* NumberFont = UTHUDOwner->GetFontFromSizeIndex(0);
		for (int i=FirstWeaponIndex; i<= LastWeaponIndex; i++)
		{
			if (WeaponList[i] != NULL)
			{
				FVector2D NumberOffset = SlotOffset * CurrentWeaponScale[i];
				FString NumText = i < 10 ? FString::Printf(TEXT("%i"),i+1) : FString::Printf(TEXT("0"));
				DrawText(FText::FromString(NumText), X + NumberOffset.X, NumberOffset.Y, NumberFont, FVector2D(1,1), FLinearColor::Black, CurrentWeaponScale[i], 1.0f, i == SelectedWeaponIndex ? FLinearColor::Yellow : FLinearColor::White );
				X += CellWidth * CurrentWeaponScale[i];
			}
		}

	}
}

