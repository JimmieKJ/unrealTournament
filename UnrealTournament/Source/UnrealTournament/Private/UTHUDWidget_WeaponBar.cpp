// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_WeaponBar.h"
#include "UTWeapon.h"
#include "UTHUDWidgetMessage.h"

UUTHUDWidget_WeaponBar::UUTHUDWidget_WeaponBar(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Position=FVector2D(0.0f, -90.0f);
	Size=FVector2D(0,0);
	ScreenPosition=FVector2D(1.0f, 1.0f);
	Origin=FVector2D(1.0f,1.0f);

	SelectedCellScale=1.1;
	SelectedAnimRate=0.3;
	CellWidth = 90;
}

void UUTHUDWidget_WeaponBar::InitializeWidget(AUTHUD* Hud)
{
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

	Super::InitializeWidget(Hud);
}

/**
 *	We aren't going tor use DrawAllRenderObjects.  Instead we are going to have a nice little custom bit of drawing based on what weapon gropup this 
 *  is.
 **/
void UUTHUDWidget_WeaponBar::Draw_Implementation(float DeltaTime)
{
	TArray<FWeaponGroup> WeaponGroups;

	CollectWeaponData(WeaponGroups);
	if (WeaponGroups.Num() > 0)
	{
		// Draw the Weapon Groups

		float YPosition = 0.0;

		AUTWeapon* SelectedWeapon = UTCharacterOwner->GetWeapon();
		int32 SelectedGroup = SelectedWeapon ? SelectedWeapon->Group : -1;

		for (int32 GroupIdx = 0; GroupIdx < WeaponGroups.Num(); GroupIdx++)
		{
			// We have no allied all of the animation and we know the biggest anim scale, so we can figure out how wide this group should be.
			
			float Y2 = YPosition;			
			float TextXPosition = 0;

			// Draw the elements.
			for (int32 WeapIdx = 0; WeapIdx < WeaponGroups[GroupIdx].WeaponsInGroup.Num(); WeapIdx++)
			{
				AUTWeapon* CurrentWeapon = WeaponGroups[GroupIdx].WeaponsInGroup[WeapIdx];
				bool bSelected = CurrentWeapon == SelectedWeapon;

				// Draw the background and the background's border.

				int32 Idx = (WeapIdx == 0) ? 0 : 1;

				float FullIconCellWidth = (CurrentWeapon->Group == SelectedGroup) ? CellWidth * SelectedCellScale : CellWidth;
				float FullCellWidth = FullIconCellWidth + HeaderTab[Idx].GetWidth() + 3 + GroupHeaderCap[Idx].GetWidth();

				float CellScale = bSelected ? SelectedCellScale : 1.0;
				float CellHeight = CellBackground[Idx].GetHeight() * CellScale;

				float IconCellWidth = CellWidth * CellScale;

				float XPosition = (FullCellWidth * -1);
				YPosition -= HeaderTab[Idx].GetHeight() * CellScale;

				// Draw the Tab.
				RenderObj_TextureAt(HeaderTab[Idx], XPosition, YPosition, HeaderTab[Idx].GetWidth(), CellHeight);
				XPosition += HeaderTab[Idx].GetWidth();
				TextXPosition = XPosition;

				// Draw the Stretch bar

				// Calculate the size of the stretch bar.

				float StretchSize = FMath::Abs<float>(XPosition) - IconCellWidth - GroupHeaderCap[Idx].GetWidth();
				RenderObj_TextureAt(GroupSpacerBorder[Idx], XPosition, YPosition, StretchSize, CellHeight);
				XPosition += StretchSize;

				// Draw the cap

				RenderObj_TextureAt(GroupHeaderCap[Idx], XPosition, YPosition, GroupHeaderCap[Idx].GetWidth(), CellHeight);
				XPosition += GroupHeaderCap[Idx].GetWidth();


				// Draw the cell and the icon.

				RenderObj_TextureAt(CellBackground[Idx], XPosition, YPosition, IconCellWidth, CellHeight);
				RenderObj_TextureAt(CellBorders[Idx], XPosition, YPosition, IconCellWidth, CellHeight);

				// Draw the Weapon Icon

				WeaponIcon.UVs = bSelected ? CurrentWeapon->WeaponBarSelectedUVs : CurrentWeapon->WeaponBarInactiveUVs;

				float WeaponY = (CellHeight * 0.5) -  (WeaponIcon.UVs.VL * CellScale * 0.5);
				RenderObj_TextureAt(WeaponIcon, -10, YPosition + WeaponY, WeaponIcon.UVs.UL * CellScale, WeaponIcon.UVs.VL * CellScale);

				// Draw the ammo bars

				if (BarTexture)
				{
					float AmmoPerc = CurrentWeapon->MaxAmmo > 0 ? float(CurrentWeapon->Ammo) / float(CurrentWeapon->MaxAmmo) : 0.0;
					float BarHeight = CellHeight-16;
					float Width = bSelected ? 4.0 : 2.0;
					float X = (Width * - 1) - 2;
					float Y = YPosition + 8;
					DrawTexture(BarTexture, X, Y, Width, BarHeight, BarTextureUVs.U, BarTextureUVs.V, BarTextureUVs.UL, BarTextureUVs.VL, bSelected ? 1.0 : 0.5, FLinearColor::Black);

					Y = Y + BarHeight - (BarHeight * AmmoPerc);
					BarHeight *= AmmoPerc;

					DrawTexture(BarTexture, X, Y, Width, BarHeight, BarTextureUVs.U, BarTextureUVs.V, BarTextureUVs.UL, BarTextureUVs.VL, bSelected ? 1.0 : 0.5, FLinearColor::White);
				}
			 
			}

			GroupText.Text = FText::AsNumber(WeaponGroups[GroupIdx].WeaponsInGroup[0]->Group);
			RenderObj_TextAt(GroupText, TextXPosition + GroupText.Position.X, YPosition + ( (Y2 - YPosition) * 0.5) + GroupText.Position.Y);

			YPosition -= 10;
		}
	}
}

void UUTHUDWidget_WeaponBar::CollectWeaponData(TArray<FWeaponGroup> &WeaponGroups)
{
	if (UTCharacterOwner)
	{
		// Parse over the character and see what weapons they have.

		for (TInventoryIterator<AUTWeapon> It(UTCharacterOwner); It; ++It)
		{
			AUTWeapon* Weapon = *It;
			int32 GroupIndex = -1;
			for (int32 i=0;i<WeaponGroups.Num();i++)
			{
				if (WeaponGroups[i].Group == Weapon->Group)
				{
					GroupIndex = i;
					break;
				}
			}
	
			if (GroupIndex < 0)
			{
				FWeaponGroup G = FWeaponGroup(Weapon->Group, Weapon);

				int32 InsertPosition = -1;
				for (int32 i=0;i<WeaponGroups.Num();i++)
				{
					if ( WeaponGroups[i].Group < G.Group)
					{
						InsertPosition = i;
						break;
					}
				}

				if (InsertPosition <0)
				{
					WeaponGroups.Add(G);
				}
				else
				{
					WeaponGroups.Insert(G,InsertPosition);
				}

			}
			else
			{
				WeaponGroups[GroupIndex].WeaponsInGroup.Add(Weapon);
			}
		}
	}
}