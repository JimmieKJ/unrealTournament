// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTRadialMenu_BoostPowerup.h"
#include "UTFlagRunPvEHUD.h"

UUTRadialMenu_BoostPowerup::UUTRadialMenu_BoostPowerup(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> MenuAtlas(TEXT("Texture2D'/Game/RestrictedAssets/UI/HUDAtlas04.HUDAtlas04'"));
	InnerCircleTemplate.QuickSet(MenuAtlas.Object, FTextureUVs(0.0f, 206.0f, 236.0f, 236.0f));

	SegmentTemplate.QuickSet(MenuAtlas.Object, FTextureUVs(275.0f, 200.0f, 189.0f, 113.0f));
	HighlightedSegmentTemplate.QuickSet(MenuAtlas.Object, FTextureUVs(275.0f, 319.0f, 189.0f, 113.0f));

	static ConstructorHelpers::FObjectFinder<UTexture2D> WeaponAtlas(TEXT("Texture2D'/Game/RestrictedAssets/UI/WeaponAtlas01.WeaponAtlas01'"));
	WeaponIconTemplate.Atlas = WeaponAtlas.Object;

	CaptionTemplate.TextScale = 1.0f;
}

void UUTRadialMenu_BoostPowerup::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);
	AutoLayout(4);
}

bool UUTRadialMenu_BoostPowerup::ShouldDraw_Implementation(bool bShowScores)
{
	AUTFlagRunPvEHUD* HUD = Cast<AUTFlagRunPvEHUD>(UTHUDOwner);
	return HUD != nullptr && HUD->bShowBoostWheel;
}

void UUTRadialMenu_BoostPowerup::DrawMenu(FVector2D ScreenCenter, float RenderDelta)
{
	AUTGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
	if (bIsInteractive && GS != nullptr && UTPlayerOwner != nullptr && UTPlayerOwner->UTPlayerState != nullptr)
	{
		const FVector2D CenterPoint = FVector2D(0.0f, -250.0f);
		Opacity = UTPlayerOwner->UTPlayerState->GetRemainingBoosts() > 0 ? 1.0f : 0.33f;
		CaptionTemplate.Text = FText();
		int i;
		TSubclassOf<AUTInventory> NextItem;
		for (i = 0, NextItem = GS->GetSelectableBoostByIndex(UTPlayerOwner->UTPlayerState, i); NextItem != nullptr; i++, NextItem = GS->GetSelectableBoostByIndex(UTPlayerOwner->UTPlayerState, i))
		{
			const bool bCurrent = CurrentSegment == i && !ShouldCancel();

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
			WeaponIconTemplate.Atlas = NextItem.GetDefaultObject()->HUDIcon.Texture;
			WeaponIconTemplate.UVs.U = NextItem.GetDefaultObject()->HUDIcon.U;
			WeaponIconTemplate.UVs.UL = NextItem.GetDefaultObject()->HUDIcon.UL;
			WeaponIconTemplate.UVs.V = NextItem.GetDefaultObject()->HUDIcon.V;
			WeaponIconTemplate.UVs.VL = NextItem.GetDefaultObject()->HUDIcon.VL;
			WeaponIconTemplate.RenderOffset = FVector2D(0.5f,0.5f);

			// Draw it in black a little bigger
			WeaponIconTemplate.RenderColor = FLinearColor::Black;
			RenderObj_TextureAt(WeaponIconTemplate, IconRenderPosition.X, IconRenderPosition.Y, WeaponIconTemplate.UVs.UL * 1.55f, WeaponIconTemplate.UVs.VL * 1.05f);
			
			// Draw it colorized
			WeaponIconTemplate.RenderColor = NextItem.GetDefaultObject()->IconColor;
			RenderObj_TextureAt(WeaponIconTemplate, IconRenderPosition.X, IconRenderPosition.Y, WeaponIconTemplate.UVs.UL * 1.5f, WeaponIconTemplate.UVs.VL * 1.0f);

			if (i == CurrentSegment)
			{
				CaptionTemplate.Text = NextItem.GetDefaultObject()->DisplayName;
			}
		}

		if (!CaptionTemplate.Text.IsEmpty())
		{
			RenderObj_Text(CaptionTemplate);

			if (UTPlayerOwner->UTPlayerState->GetRemainingBoosts() == 0)
			{
				CaptionTemplate.Text = NSLOCTEXT("PowerupWheel","Charging","<Charging...>");
				CaptionTemplate.TextScale = 0.75f;
				RenderObj_Text(CaptionTemplate, FVector2D(0.0f, 35.0f));
			}
		}
	}
}

void UUTRadialMenu_BoostPowerup::Execute()
{
	if (CurrentSegment != INDEX_NONE && UTPlayerOwner->UTPlayerState != nullptr)
	{
		AUTGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
		if (GS->GetSelectableBoostByIndex(UTPlayerOwner->UTPlayerState, CurrentSegment) != nullptr)
		{
			UTPlayerOwner->UTPlayerState->ServerSetBoostItem(CurrentSegment);
			UTPlayerOwner->ServerActivatePowerUpPress();
		}
	}
}

