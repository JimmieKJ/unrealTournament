// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTRadialMenu_WeaponWheel.h"

UUTRadialMenu_WeaponWheel::UUTRadialMenu_WeaponWheel(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> MenuAtlas(TEXT("Texture2D'/Game/RestrictedAssets/UI/HUDAtlas04.HUDAtlas04'"));
	InnerCircleTemplate.QuickSet(MenuAtlas.Object, FTextureUVs(0.0f, 206.0f, 236.0f, 236.0f));

	SegmentTemplate.QuickSet(MenuAtlas.Object, FTextureUVs(275.0f, 200.0f, 189.0f, 113.0f));
	HighlightedSegmentTemplate.QuickSet(MenuAtlas.Object, FTextureUVs(275.0f, 319.0f, 189.0f, 113.0f));

	static ConstructorHelpers::FObjectFinder<UTexture2D> WeaponAtlas(TEXT("Texture2D'/Game/RestrictedAssets/UI/WeaponAtlas01.WeaponAtlas01'"));
	WeaponIconTemplate.Atlas = WeaponAtlas.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> HUDAtlas(TEXT("Texture2D'/Game/RestrictedAssets/UI/HUDAtlas01.HUDAtlas01'"));
	OutOfAmmoTemplate.Atlas = HUDAtlas.Object;
	OutOfAmmoTemplate.UVs = FTextureUVs(414.0f, 947.0f, 58.0f, 58.0f);
	OutOfAmmoTemplate.RenderColor = FLinearColor::Red;
}

void UUTRadialMenu_WeaponWheel::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);
	AutoLayout(8);
}

void UUTRadialMenu_WeaponWheel::BecomeInteractive()
{
	Super::BecomeInteractive();

	if (UTHUDOwner->UTPlayerOwner->GetUTCharacter())
	{
		WeaponList.Empty();
		WeaponList.AddZeroed(8);

		UUTProfileSettings* ProfileSettings = UTHUDOwner->UTPlayerOwner->GetProfileSettings();
		if (ProfileSettings)
		{
			// Find what weapons to display...
			for (TInventoryIterator<AUTWeapon> It(UTHUDOwner->UTPlayerOwner->GetUTCharacter()); It; ++It)
			{
				for (int32 SlotId=0; SlotId < ProfileSettings->WeaponWheelMapping.Num();SlotId++)
				{
					int32 Group = 0;
					int32 GroupPriority = 0;

					ProfileSettings->GetWeaponGroup(*It, Group, GroupPriority);
					if (ProfileSettings->WeaponWheelMapping[SlotId] == Group)
					{
						if (WeaponList[SlotId] == nullptr || (!WeaponList[SlotId]->HasAnyAmmo() && It->HasAnyAmmo()))
						{
							WeaponList[SlotId] = *It;
						}
						break;
					}
				}
			}
		}
	}
}


void UUTRadialMenu_WeaponWheel::BecomeNonInteractive()
{
	Super::BecomeNonInteractive();
	WeaponList.Empty();
}


void UUTRadialMenu_WeaponWheel::DrawMenu(FVector2D ScreenCenter, float RenderDelta)
{
	bActiveSlotOutOfAmmo = false;
	if (bIsInteractive)
	{
		FVector2D CenterPoint = FVector2D(0.0f, -250.0f);
		for (int32 i=0; i < WeaponList.Num(); i++)
		{
			if (WeaponList[i] != nullptr && !WeaponList[i]->IsPendingKillPending())
			{
				bool bCurrent = CurrentSegment == i && !ShouldCancel();

				if (bCurrent && !WeaponList[i]->HasAnyAmmo())
				{
					bActiveSlotOutOfAmmo = true;
				}

				Opacity = ( WeaponList[i]->HasAnyAmmo() ) ? 1.0f : 0.33f;

				float Angle = GetMidSegmentAngle(i);
				FVector2D DrawScreenPosition = Rotate(CenterPoint, Angle);
				SegmentTemplate.RenderScale = bCurrent ? 1.2f : 1.0f; 
				RenderObj_TextureAtWithRotation(SegmentTemplate, DrawScreenPosition, Angle);
				if (bCurrent)
				{
					HighlightedSegmentTemplate.RenderScale = bCurrent ? 1.2f : 1.0f; 
					RenderObj_TextureAtWithRotation(HighlightedSegmentTemplate, DrawScreenPosition, Angle);
				}

				FVector2D IconRenderPosition = Rotate(FVector2D(0.0f,-250.0f), Angle);
				WeaponIconTemplate.UVs = WeaponList[i]->WeaponBarSelectedUVs;
				WeaponIconTemplate.RenderOffset = FVector2D(0.5f,0.5f);

				// Draw it in black a little bigger
				WeaponIconTemplate.RenderColor = FLinearColor::Black;
				RenderObj_TextureAt(WeaponIconTemplate, IconRenderPosition.X, IconRenderPosition.Y, WeaponIconTemplate.UVs.UL * 1.55f, WeaponIconTemplate.UVs.VL * 1.05f);
			
				// Draw it colorized
				WeaponIconTemplate.RenderColor = WeaponList[i]->IconColor;
				RenderObj_TextureAt(WeaponIconTemplate, IconRenderPosition.X, IconRenderPosition.Y, WeaponIconTemplate.UVs.UL * 1.5f, WeaponIconTemplate.UVs.VL * 1.0f);
			}
		}

		if (WeaponList.IsValidIndex(CurrentSegment) && WeaponList[CurrentSegment] != nullptr)
		{
			CaptionTemplate.Text = WeaponList[CurrentSegment]->DisplayName;
			CaptionTemplate.TextScale = 1.0f;
			RenderObj_Text(CaptionTemplate);

			if (!WeaponList[CurrentSegment]->HasAnyAmmo())
			{
				CaptionTemplate.Text = NSLOCTEXT("WeaponWheel","NoAmmo","<Out of Ammo>");
				CaptionTemplate.TextScale = 0.75f;
				RenderObj_Text(CaptionTemplate, FVector2D(0.0f, 35.0f));
			}

		}
	}
}

void UUTRadialMenu_WeaponWheel::Execute()
{
	if (WeaponList.IsValidIndex(CurrentSegment) && WeaponList[CurrentSegment] != nullptr && !WeaponList[CurrentSegment]->IsPendingKillPending())
	{
		UUTProfileSettings* Profile = UTHUDOwner->UTPlayerOwner->GetProfileSettings();
		if (Profile)
		{
			UTHUDOwner->UTPlayerOwner->SwitchWeaponGroup(Profile->WeaponWheelMapping[CurrentSegment]);
		}
	}
}

void UUTRadialMenu_WeaponWheel::DrawCursor(FVector2D ScreenCenter, float RenderDelta)
{
	if (bActiveSlotOutOfAmmo)
	{
		// Draw the Cursor
		RenderObj_TextureAt(OutOfAmmoTemplate, CursorPosition.X, CursorPosition.Y, 26, 26);
	}
	else
	{
		Super::DrawCursor(ScreenCenter, RenderDelta);
	}
}
