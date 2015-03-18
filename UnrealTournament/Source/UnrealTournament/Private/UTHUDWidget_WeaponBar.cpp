// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	CellWidth = 145;

	static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Medium.fntScoreboard_Medium'"));
	WeaponNameText.Font = Font.Object;
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

	Super::InitializeWidget(Hud);
}

/**
 *	We aren't going to use DrawAllRenderObjects.  Instead we are going to have a nice little custom bit of drawing based on what weapon group this is.
 **/
void UUTHUDWidget_WeaponBar::Draw_Implementation(float DeltaTime)
{
	TArray<FWeaponGroup> WeaponGroups;

	if (UTCharacterOwner == NULL) return; // Don't draw without a character

	// Handle fading out.
	if (FadeTimer > 0.0f)
	{
		FadeTimer -= DeltaTime;
	}
	else 
	{
		if (InactiveOpacity != UTHUDOwner->HUDWidgetWeaponbarInactiveOpacity)
		{
			float Delta = (1.0 - UTHUDOwner->HUDWidgetWeaponbarInactiveOpacity) * DeltaTime;	// 1 second fade
			if (InactiveOpacity < UTHUDOwner->HUDWidgetWeaponbarInactiveOpacity) 
			{
				InactiveOpacity = FMath::Clamp<float>(InactiveOpacity + Delta, 0.0, UTHUDOwner->HUDWidgetWeaponbarInactiveOpacity);
			}
			else
			{
				InactiveOpacity = FMath::Clamp<float>(InactiveOpacity - Delta, UTHUDOwner->HUDWidgetWeaponbarInactiveOpacity, 1.0);
			}
		}

		if (InactiveIconOpacity != UTHUDOwner->HUDWidgetWeaponBarInactiveIconOpacity)
		{
			float Delta = (1.0 - UTHUDOwner->HUDWidgetWeaponBarInactiveIconOpacity) * DeltaTime;	// 1 second fade
			if (InactiveIconOpacity < UTHUDOwner->HUDWidgetWeaponBarInactiveIconOpacity) 
			{
				InactiveIconOpacity = FMath::Clamp<float>(InactiveIconOpacity + Delta, 0.0, UTHUDOwner->HUDWidgetWeaponBarInactiveIconOpacity);
			}
			else
			{
				InactiveIconOpacity = FMath::Clamp<float>(InactiveIconOpacity - Delta, UTHUDOwner->HUDWidgetWeaponBarInactiveIconOpacity, 1.0);
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
		if (!WeaponNameText.Text.EqualTo(SelectedWeapon->DisplayName))
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

	bool bUseClassicGroups = UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->bUseClassicGroups;
	int32 SelectedGroup = -1;
	if (SelectedWeapon)
	{
		SelectedGroup = bUseClassicGroups ? SelectedWeapon->ClassicGroup : SelectedWeapon->Group;
		if (SelectedGroup != LastGroup || SelectedWeapon->GroupSlot != LastGroupSlot)
		{
			// Weapon has changed.. set everything up.
			InactiveOpacity = 1.0;
			InactiveIconOpacity = 1.0;
			FadeTimer = 1.0;

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
			// We have no allied all of the animation and we know the biggest anim scale, so we can figure out how wide this group should be.
			float Y2 = YPosition;
			float TextXPosition = 0;
			bool bSelectedGroup = false;
			if (WeaponGroups[GroupIdx].WeaponsInGroup.Num() > 0)
			{
				// Draw the elements.
				for (int32 WeapIdx = 0; WeapIdx < WeaponGroups[GroupIdx].WeaponsInGroup.Num(); WeapIdx++)
				{
					AUTWeapon* CurrentWeapon = WeaponGroups[GroupIdx].WeaponsInGroup[WeapIdx];
					bool bSelected = CurrentWeapon == SelectedWeapon;
					int32 CurrentGroup = bUseClassicGroups ? CurrentWeapon->ClassicGroup : CurrentWeapon->Group;

					if (bSelected)
					{
						bSelectedGroup = true;
					}
					Opacity = bSelected ? 1.0 : InactiveOpacity;

					// Draw the background and the background's border.
					int32 Idx = (WeapIdx == 0) ? 0 : 1;
					float FullIconCellWidth = (CurrentGroup == SelectedGroup) ? CellWidth * SelectedCellScale : CellWidth;
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
					float StretchSize = FMath::Abs<float>(XPosition) -IconCellWidth - GroupHeaderCap[Idx].GetWidth();
					RenderObj_TextureAt(GroupSpacerBorder[Idx], XPosition, YPosition, StretchSize, CellHeight);
					XPosition += StretchSize;

					// Draw the cap
					RenderObj_TextureAt(GroupHeaderCap[Idx], XPosition, YPosition, GroupHeaderCap[Idx].GetWidth(), CellHeight);
					XPosition += GroupHeaderCap[Idx].GetWidth();

					// Draw the cell and the icon.
					RenderObj_TextureAt(CellBackground[Idx], XPosition, YPosition, IconCellWidth, CellHeight);
					RenderObj_TextureAt(CellBorders[Idx], XPosition, YPosition, IconCellWidth, CellHeight);

					Opacity = bSelected ? 1.0 : InactiveIconOpacity;

					// Draw the Weapon Icon
					if (CurrentWeapon)
					{
						WeaponIcon.UVs = bSelected ? CurrentWeapon->WeaponBarSelectedUVs : CurrentWeapon->WeaponBarInactiveUVs;
						WeaponIcon.RenderColor = UTHUDOwner->bUseWeaponColors ? CurrentWeapon->IconColor : FLinearColor::White;
					}

					float WeaponY = (CellHeight * 0.5) - (WeaponIcon.UVs.VL * CellScale * 0.5);
					RenderObj_TextureAt(WeaponIcon, -15, YPosition + WeaponY, WeaponIcon.UVs.UL * CellScale, WeaponIcon.UVs.VL * CellScale);

					// Draw the ammo bars
					if (BarTexture)
					{
						float AmmoPerc = CurrentWeapon->MaxAmmo > 0 ? float(CurrentWeapon->Ammo) / float(CurrentWeapon->MaxAmmo) : 0.0;
						float BarHeight = CellHeight - 16;
						float Width = bSelected ? 9.0 : 7.0;
						float X = (Width * -1) - 2;
						float Y = YPosition + 8;
						DrawTexture(BarTexture, X, Y, Width, BarHeight, BarTextureUVs.U, BarTextureUVs.V, BarTextureUVs.UL, BarTextureUVs.VL, 1.0, FLinearColor::Black);

						Y = Y + BarHeight - 1 - ((BarHeight-2) * AmmoPerc);
						BarHeight = (BarHeight -2) * AmmoPerc;
						FLinearColor BarColor = FLinearColor::Green;
						if (AmmoPerc <= 0.33)
						{
							BarColor = FLinearColor::Red;
						}
						else if (AmmoPerc <= 0.66)
						{
							BarColor = FLinearColor::Yellow;
						}
						DrawTexture(BarTexture, X+1, Y, Width-2, BarHeight, BarTextureUVs.U, BarTextureUVs.V, BarTextureUVs.UL, BarTextureUVs.VL, 1.0, BarColor);
					}
				}

				Opacity = bSelectedGroup ? 1.0 : InactiveIconOpacity;

				if (WeaponGroups[GroupIdx].Group == 10)
				{
					GroupText.Text = FText::FromString(TEXT("0"));
				}
				else
				{
					GroupText.Text = FText::AsNumber(WeaponGroups[GroupIdx].Group);
				}

				RenderObj_TextAt(GroupText, TextXPosition + GroupText.Position.X, YPosition + ((Y2 - YPosition) * 0.5) + GroupText.Position.Y);
			}
			else
			{
				Opacity = UTHUDOwner->HUDWidgetWeaponBarEmptyOpacity * InactiveOpacity;

				// Draw the background and the background's border.
				float FullIconCellWidth = CellWidth;
				float FullCellWidth = FullIconCellWidth + HeaderTab[0].GetWidth() + 3 + GroupHeaderCap[0].GetWidth();
				float CellScale = 1.0;
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
				RenderObj_TextAt(GroupText, TextXPosition + GroupText.Position.X, YPosition + ((Y2 - YPosition) * 0.5) + GroupText.Position.Y);
			}

			YPosition -= 10;
		}
	}

	if (WeaponNameText.RenderOpacity > 0.f)
	{
		Opacity = 1.0;

		// Recalc this to handle aspect ratio
		WeaponNameText.Position.X = (Canvas->ClipX * 0.5f) / RenderScale * -1.f;
		RenderObj_TextAt(WeaponNameText, WeaponNameText.Position.X, WeaponNameText.Position.Y);
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
		bool bUseClassicGroups = UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->bUseClassicGroups;
		int32 ActualRequiredGroups = bUseClassicGroups ? 10 : RequiredGroups;

		// Parse over the character and see what weapons they have.
		if (ActualRequiredGroups >= 0)
		{
			for (int i = ActualRequiredGroups; i >= 0; i--)
			{
				FWeaponGroup G = FWeaponGroup(i, NULL);
				WeaponGroups.Add(G);
			}
		}

		for (TInventoryIterator<AUTWeapon> It(UTCharacterOwner); It; ++It)
		{
			NumWeapons++;
			AUTWeapon* Weapon = *It;
			int32 WeaponGroup = bUseClassicGroups ? Weapon->ClassicGroup : Weapon->Group;
			int32 GroupIndex = -1;
			for (int32 i=0;i<WeaponGroups.Num();i++)
			{
				if (WeaponGroups[i].Group == WeaponGroup)
				{
					GroupIndex = i;
					int32 InsertPosition = -1;
					for (int32 i = 0; i<WeaponGroups[GroupIndex].WeaponsInGroup.Num(); i++)
					{
						if (WeaponGroups[GroupIndex].WeaponsInGroup[i]->GroupSlot < Weapon->GroupSlot)
						{
							InsertPosition = i;
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
