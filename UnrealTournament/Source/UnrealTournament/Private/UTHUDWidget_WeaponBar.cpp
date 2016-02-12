// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_WeaponBar.h"
#include "UTWeapon.h"
#include "UTHUDWidgetMessage.h"

UUTHUDWidget_WeaponBar::UUTHUDWidget_WeaponBar(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Position=FVector2D(-8.0f, 0.0f);
	Size=FVector2D(1.0f,0.0f);
	ScreenPosition=FVector2D(1.0f, 0.5f);
	Origin=FVector2D(1.f,0.5f);

	PaddingBetweenCells = 10.0f;
	SelectedCellScale=1.1;
	SelectedAnimRate=0.3;
	CellWidth = 145;
}

void UUTHUDWidget_WeaponBar::InitializeWidget(AUTHUD* Hud)
{
	LastGroup = -1;
	LastGroupSlot = -1.0;

	if (CellBackground.Num() < 2)
	{
		CellBackground.SetNumZeroed(2);
	}
	if (CellBorders.Num() < 2)
	{
		CellBorders.SetNumZeroed(2);
	}
	if (GroupHeaderCap.Num() < 2)
	{
		GroupHeaderCap.SetNumZeroed(2);
	}
	if (GroupSpacerBorder.Num() < 2)
	{
		GroupSpacerBorder.SetNumZeroed(2);
	}
	if (HeaderTab.Num() < 2)
	{
		HeaderTab.SetNumZeroed(2);
	}
	WeaponNameText.Font = Hud->MediumFont;
	Super::InitializeWidget(Hud);
}

/**
	The WeaponBar scaling size is incorrect due to it having a dynamic size. We need to adjust the end RenderPosition to account for its size to keep it centered.
*/
void UUTHUDWidget_WeaponBar::PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter)
{
	Super::PreDraw(DeltaTime, InUTHUDOwner, InCanvas, InCanvasCenter);

	//Calculate size of WeaponBar
	TArray<FWeaponGroup> WeaponGroups;
	const int32 NumWeapons = CollectWeaponData(WeaponGroups, DeltaTime);
	const int32 NumberOfCells = (NumWeapons > RequiredGroups) ? NumWeapons : RequiredGroups; //We always draw at least enough cells for the required groups, but it could be more cells if we have >1 weapon in any groups.

	const float NormalCellHeight = CellBackground[0].GetHeight() + PaddingBetweenCells;
	const float SelectedCellHeight = (CellBackground[0].GetHeight() * SelectedCellScale) + PaddingBetweenCells;

	const float WeaponBarSize = ((NormalCellHeight * (NumberOfCells - 1)) + SelectedCellHeight) * RenderScale;

	//Move the bar down by 1/2 the size to keep it centered on the screen.
	RenderPosition.Y += WeaponBarSize / 2;
}

/**
 *	We aren't going to use DrawAllRenderObjects.  Instead we are going to have a nice little custom bit of drawing based on what weapon group this is.
 **/
void UUTHUDWidget_WeaponBar::Draw_Implementation(float DeltaTime)
{
	TArray<FWeaponGroup> WeaponGroups;

	if (UTCharacterOwner == NULL) return; // Don't draw without a character

	if (UTHUDOwner->bHUDWeaponBarSettingChanged)
	{
		InactiveOpacity = UTHUDOwner->HUDWidgetWeaponbarInactiveOpacity;
		InactiveIconOpacity = UTHUDOwner->HUDWidgetWeaponBarInactiveIconOpacity;
		UTHUDOwner->bHUDWeaponBarSettingChanged = false;
	}

	// Handle fading out.
	if (FadeTimer > 0.0f)
	{
		FadeTimer -= DeltaTime;
	}
	else 
	{
		if (InactiveOpacity != UTHUDOwner->HUDWidgetWeaponbarInactiveOpacity)
		{
			float Delta = (1.f - UTHUDOwner->HUDWidgetWeaponbarInactiveOpacity) * DeltaTime;	// 1 second fade
			if (InactiveOpacity < UTHUDOwner->HUDWidgetWeaponbarInactiveOpacity) 
			{
				InactiveOpacity = FMath::Clamp<float>(InactiveOpacity + Delta, 0.f, UTHUDOwner->HUDWidgetWeaponbarInactiveOpacity);
			}
			else
			{
				InactiveOpacity = FMath::Clamp<float>(InactiveOpacity - Delta, UTHUDOwner->HUDWidgetWeaponbarInactiveOpacity, 1.f);
			}
		}

		if (InactiveIconOpacity != UTHUDOwner->HUDWidgetWeaponBarInactiveIconOpacity)
		{
			float MaxIconOpacity = FMath::Max(0.6f, UTHUDOwner->HUDWidgetWeaponBarInactiveIconOpacity);
			float Delta = (MaxIconOpacity - UTHUDOwner->HUDWidgetWeaponBarInactiveIconOpacity) * DeltaTime;	// 1 second fade
			if (InactiveIconOpacity < UTHUDOwner->HUDWidgetWeaponBarInactiveIconOpacity) 
			{
				InactiveIconOpacity = FMath::Clamp<float>(InactiveIconOpacity + Delta, 0.f, UTHUDOwner->HUDWidgetWeaponBarInactiveIconOpacity);
			}
			else
			{
				InactiveIconOpacity = FMath::Clamp<float>(InactiveIconOpacity - Delta, UTHUDOwner->HUDWidgetWeaponBarInactiveIconOpacity, MaxIconOpacity);
			}
		}
	}

	AUTWeapon* SelectedWeapon = UTCharacterOwner->GetPendingWeapon();
	if (SelectedWeapon == NULL) 
	{
		SelectedWeapon = UTCharacterOwner->GetWeapon();
		if (SelectedWeapon)
		{
			WeaponNameText.Text = SelectedWeapon->DisplayName;
		}
	}
	else
	{
		AUTWeapon* CurrentWeapon = UTCharacterOwner->GetWeapon();
		if ((!CurrentWeapon || CurrentWeapon->IsUnEquipping()) && !WeaponNameText.Text.EqualTo(SelectedWeapon->DisplayName))
		{
			WeaponNameText.Text = SelectedWeapon->DisplayName;
			WeaponNameText.RenderOpacity = 1.f;
			WeaponNameDisplayTimer = WeaponNameDisplayTime;
			UUTHUDWidgetMessage* DestinationWidget = (UTHUDOwner->HudMessageWidgets.FindRef(FName(TEXT("PickupMessage"))));
			if (DestinationWidget != NULL)
			{
				DestinationWidget->FadeMessage(SelectedWeapon->DisplayName);
			}
		}
	}

	int32 SelectedGroup = -1;
	if (SelectedWeapon)
	{
		SelectedGroup = SelectedWeapon->Group;
		if (SelectedGroup != LastGroup || SelectedWeapon->GroupSlot != LastGroupSlot)
		{
			// Weapon has changed.. set everything up.
			InactiveOpacity = 1.f;
			InactiveIconOpacity = FMath::Max(0.6f, UTHUDOwner->HUDWidgetWeaponBarInactiveIconOpacity);
			FadeTimer = 1.f;

			LastGroup = SelectedGroup;
			LastGroupSlot = SelectedWeapon->GroupSlot;
		}
	}

	int32 NumWeapons = CollectWeaponData(WeaponGroups, DeltaTime);
	if (NumWeapons < 2)
	{
		// skip rendering weapon bar if 0 or 1 weapons
		return;
	}

	if (WeaponGroups.Num() > 0)
	{
		// Draw the Weapon Groups
		float YPosition = 0.0;

		for (int32 GroupIdx = 0; GroupIdx < WeaponGroups.Num(); GroupIdx++)
		{
			if (WeaponGroups[GroupIdx].Group == 0)
			{
				// skip translocator on weapon bar
				continue;
			}
			// We have now allied all of the animation and we know the biggest anim scale, so we can figure out how wide this group should be.
			float Y2 = YPosition;
			float TextXPosition = 0.f;
			bool bSelectedGroup = false;
			if (WeaponGroups[GroupIdx].WeaponsInGroup.Num() > 0)
			{
				// Draw the elements.
				for (int32 WeapIdx = 0; WeapIdx < WeaponGroups[GroupIdx].WeaponsInGroup.Num(); WeapIdx++)
				{
					AUTWeapon* CurrentWeapon = WeaponGroups[GroupIdx].WeaponsInGroup[WeapIdx];
					bool bSelected = CurrentWeapon == SelectedWeapon;
					bSelectedGroup = bSelectedGroup || bSelected;
					Opacity = bSelected ? 1.f : InactiveOpacity;

					// Draw the background and the background's border.
					int32 Idx = (WeapIdx == 0) ? 0 : 1;
					float FullIconCellWidth = (CurrentWeapon->Group == SelectedGroup) ? CellWidth * SelectedCellScale : CellWidth;
					float FullCellWidth = FullIconCellWidth + HeaderTab[Idx].GetWidth() + 3.f + GroupHeaderCap[Idx].GetWidth();
					float CellScale = bSelected ? SelectedCellScale : 1.f;
					float CellHeight = CellBackground[Idx].GetHeight() * CellScale;
					float IconCellWidth = CellWidth * CellScale;
					float XPosition = -1.f * FullCellWidth;
					YPosition -= HeaderTab[Idx].GetHeight() * CellScale;

					// Draw the Tab.
					RenderObj_TextureAt(HeaderTab[Idx], XPosition, YPosition, HeaderTab[Idx].GetWidth(), CellHeight);
					XPosition += HeaderTab[Idx].GetWidth();
					TextXPosition = XPosition;

					// Draw the Stretch bar

					// Calculate the size of the stretch bar.
					float StretchSize = FMath::Abs<float>(XPosition) -IconCellWidth - GroupHeaderCap[Idx].GetWidth();
					RenderObj_TextureAt(GroupSpacerBorder[Idx], XPosition, YPosition, StretchSize, CellHeight);
					XPosition += StretchSize;

					// Draw the cap
					RenderObj_TextureAt(GroupHeaderCap[Idx], XPosition, YPosition, GroupHeaderCap[Idx].GetWidth(), CellHeight);
					XPosition += GroupHeaderCap[Idx].GetWidth();

					// Draw the cell and the icon.
					RenderObj_TextureAt(CellBackground[Idx], XPosition, YPosition, IconCellWidth, CellHeight);
					RenderObj_TextureAt(CellBorders[Idx], XPosition, YPosition, IconCellWidth, CellHeight);

					Opacity = bSelected ? 1.f : InactiveIconOpacity;

					// Draw the Weapon Icon
					if (CurrentWeapon)
					{
						WeaponIcon.UVs = CurrentWeapon->WeaponBarSelectedUVs;
						WeaponIcon.RenderColor = (bSelected || UTHUDOwner->bUseWeaponColors) ? CurrentWeapon->IconColor : FLinearColor::White;
					}

					float WeaponY = (CellHeight * 0.5f) - (WeaponIcon.UVs.VL * CellScale * 0.5f);
					RenderObj_TextureAt(WeaponIcon, -15, YPosition + WeaponY, WeaponIcon.UVs.UL * CellScale, WeaponIcon.UVs.VL * CellScale);

					// Draw the ammo bars
					if (BarTexture)
					{
						Opacity = (UTHUDOwner->HUDWidgetOpacity > 0.f) ? 1.f : 0.f;
						float AmmoPerc = CurrentWeapon->MaxAmmo > 0 ? float(CurrentWeapon->Ammo) / float(CurrentWeapon->MaxAmmo) : 0.f;
						float BarHeight = CellHeight - 16.f;
						float Width = bSelected ? 9.f : 7.f;
						float X = (Width * -1.f) - 2.f;
						float Y = YPosition + 4.f;
						DrawTexture(BarTexture, X, Y, Width, BarHeight, BarTextureUVs.U, BarTextureUVs.V, BarTextureUVs.UL, BarTextureUVs.VL, 1.f, FLinearColor::Black);

						Y = Y + BarHeight - 1.f - ((BarHeight-2.f) * AmmoPerc);
						BarHeight = (BarHeight -2.f) * AmmoPerc;
						FLinearColor BarColor = FLinearColor(0.5f, 0.5f, 1.f ,1.f);
						if (AmmoPerc <= 0.33f)
						{
							BarColor = FLinearColor(1.f, 0.5f, 0.5f, 1.f);
						}
						else if (AmmoPerc <= 0.66f)
						{
							BarColor = FLinearColor(1.f, 1.f, 0.5f, 1.f);
						}
						DrawTexture(BarTexture, X + 1.f, Y, Width - 2.f, BarHeight, BarTextureUVs.U, BarTextureUVs.V, BarTextureUVs.UL, BarTextureUVs.VL, Opacity, BarColor);
					}
				}

				Opacity = bSelectedGroup ? 1.f : InactiveIconOpacity;
				if (UTHUDOwner && UTHUDOwner->UTPlayerOwner)
				{
					FString* GroupKey = UTHUDOwner->UTPlayerOwner->WeaponGroupKeys.Find(WeaponGroups[GroupIdx].Group);
					if ((GroupKey == nullptr) ||GroupKey->IsEmpty())
					{
						GroupText.Text = (WeaponGroups[GroupIdx].Group == 10) ? FText::FromString(TEXT("0")) : FText::AsNumber(WeaponGroups[GroupIdx].Group);
					}
					else
					{
						GroupText.Text = (GroupKey->Len() == 1) ? FText::FromString(*GroupKey) : FText::FromString(TEXT(""));
					}
				}
			}
			else
			{
				Opacity = UTHUDOwner->HUDWidgetWeaponBarEmptyOpacity * InactiveOpacity;

				// Draw the background and the background's border.
				float FullIconCellWidth = CellWidth;
				float FullCellWidth = FullIconCellWidth + HeaderTab[0].GetWidth() + 3 + GroupHeaderCap[0].GetWidth();
				float CellScale = 1.f;
				float CellHeight = CellBackground[0].GetHeight() * CellScale;
				float IconCellWidth = CellWidth * CellScale;
				float XPosition = (FullCellWidth * -1);
				YPosition -= HeaderTab[0].GetHeight() * CellScale;

				// Draw the Tab.
				RenderObj_TextureAt(HeaderTab[0], XPosition, YPosition, HeaderTab[0].GetWidth(), CellHeight);
				XPosition += HeaderTab[0].GetWidth();
				TextXPosition = XPosition;

				// Draw the Stretch bar
				// Calculate the size of the stretch bar.
				float StretchSize = FMath::Abs<float>(XPosition) -IconCellWidth - GroupHeaderCap[0].GetWidth();
				RenderObj_TextureAt(GroupSpacerBorder[0], XPosition, YPosition, StretchSize, CellHeight);
				XPosition += StretchSize;

				// Draw the cap
				RenderObj_TextureAt(GroupHeaderCap[0], XPosition, YPosition, GroupHeaderCap[0].GetWidth(), CellHeight);
				XPosition += GroupHeaderCap[0].GetWidth();

				// Draw the cell and the icon.
				RenderObj_TextureAt(CellBackground[0], XPosition, YPosition, IconCellWidth, CellHeight);
				RenderObj_TextureAt(CellBorders[0], XPosition, YPosition, IconCellWidth, CellHeight);

				Opacity = UTHUDOwner->HUDWidgetWeaponBarEmptyOpacity * InactiveIconOpacity;
				GroupText.Text = FText::AsNumber(WeaponGroups[GroupIdx].Group);
			}
			RenderObj_TextAt(GroupText, TextXPosition + GroupText.Position.X, YPosition + ((Y2 - YPosition) * 0.5f) + GroupText.Position.Y);
			YPosition -= PaddingBetweenCells;
		}
	}

	if (WeaponNameText.RenderOpacity > 0.f)
	{
		bool bRealScaling = bScaleByDesignedResolution;
		bScaleByDesignedResolution = false;
		Opacity = 1.f;
		DrawText(WeaponNameText.Text, Canvas->ClipX * 0.5f - RenderPosition.X, Canvas->ClipY * 0.83f - RenderPosition.Y, WeaponNameText.Font, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, 1.f, WeaponNameText.RenderOpacity, WeaponNameText.RenderColor, ETextHorzPos::Center, ETextVertPos::Top);
		bScaleByDesignedResolution = bRealScaling;
	}

	if (WeaponNameDisplayTimer > 0.f)
	{
		WeaponNameDisplayTimer -= DeltaTime;
		if ( WeaponNameDisplayTimer <= (WeaponNameDisplayTime * 0.5f) )
		{
			WeaponNameText.RenderOpacity = WeaponNameDisplayTimer / (WeaponNameDisplayTime * 0.5f);
		}
		else
		{
			Opacity = 1.f;
		}
	}
}

int32 UUTHUDWidget_WeaponBar::CollectWeaponData(TArray<FWeaponGroup> &WeaponGroups, float DeltaTime)
{
	int32 NumWeapons = 0;
	if (UTCharacterOwner)
	{
		int32 ActualRequiredGroups = RequiredGroups;

		// Parse over the character and see what weapons they have.
		if (ActualRequiredGroups >= 0)
		{
			for (int32 i = ActualRequiredGroups; i >= 0; i--)
			{
				FWeaponGroup G = FWeaponGroup(i, NULL);
				WeaponGroups.Add(G);
			}
		}

		for (TInventoryIterator<AUTWeapon> It(UTCharacterOwner); It; ++It)
		{
			NumWeapons++;
			AUTWeapon* Weapon = *It;
			int32 WeaponGroup = Weapon->Group;
			int32 GroupIndex = -1;
			for (int32 i=0;i<WeaponGroups.Num();i++)
			{
				if (WeaponGroups[i].Group == WeaponGroup)
				{
					GroupIndex = i;
					int32 InsertPosition = -1;
					for (int32 WeaponInGroupIdx = 0; WeaponInGroupIdx < WeaponGroups[GroupIndex].WeaponsInGroup.Num(); WeaponInGroupIdx++)
					{
						if (WeaponGroups[GroupIndex].WeaponsInGroup[WeaponInGroupIdx]->GroupSlot < Weapon->GroupSlot)
						{
							InsertPosition = WeaponInGroupIdx;
							break;
						}
					}

					if (InsertPosition < 0)
					{
						WeaponGroups[GroupIndex].WeaponsInGroup.Add(Weapon);
					}
					else
					{
						WeaponGroups[GroupIndex].WeaponsInGroup.Insert(Weapon, InsertPosition);
					}
					break;
				}
			}
	
			if (GroupIndex < 0)
			{
				FWeaponGroup G = FWeaponGroup(WeaponGroup, Weapon);

				int32 InsertPosition = -1;
				for (int32 i=0;i<WeaponGroups.Num();i++)
				{
					if ( WeaponGroups[i].Group < G.Group)
					{
						InsertPosition = i;
						break;
					}
				}

				if (InsertPosition < 0)
				{
					WeaponGroups.Add(G);
				}
				else
				{
					WeaponGroups.Insert(G,InsertPosition);
				}
			}
		}
	}
	return NumWeapons;
}

float UUTHUDWidget_WeaponBar::GetDrawScaleOverride()
{
	return UTHUDOwner->HUDWidgetScaleOverride * UTHUDOwner->HUDWidgetWeaponBarScaleOverride;
}

bool UUTHUDWidget_WeaponBar::ShouldDraw_Implementation(bool bShowScores)
{
	return Super::ShouldDraw_Implementation(bShowScores) && !UTHUDOwner->bDrawMinimap;
}
