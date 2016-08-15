// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_Powerups.h"
#include "UTTimedPowerup.h"
#include "UTJumpBoots.h"

UUTHUDWidget_Powerups::UUTHUDWidget_Powerups(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	Position = FVector2D(260.0f, 0.0f);
	Size = FVector2D(0.0f, 0.0f);
	ScreenPosition = FVector2D(0.5f, 1.0f);
	ScaleFactor = 1.25f;
}

void UUTHUDWidget_Powerups::Draw_Implementation(float DeltaTime)
{
	if (UTCharacterOwner && TimeText.Font)
	{
		float TextWidth;
		float TextHeight;
		Canvas->TextSize(TimeText.Font, TEXT("00"), TextWidth, TextHeight, ScaleFactor, ScaleFactor);
		float XOffset = 0;
		float YOffset = 0;

		for (TInventoryIterator<> It(UTCharacterOwner); It; ++It)
		{
			AUTInventory* InventoryItem = (*It);
			if (InventoryItem && InventoryItem->HUDShouldRender(this))
			{
				float Height = InventoryItem->HUDIcon.VL;
				float Width = (Height * (InventoryItem->HUDIcon.UL / InventoryItem->HUDIcon.VL));
				Height *= ScaleFactor;
				Width *= ScaleFactor;
				TimeText.Text = InventoryItem->GetHUDText();
				float BarWidth = Width + 2.f + (TimeText.Text.IsEmpty() ? 0.0f : 8.f * ScaleFactor + TextWidth);

				RenderObj_TextureAt(TileTexture[0], XOffset + TileTexture[0].Position.X, YOffset + TileTexture[0].Position.Y, BarWidth, TileTexture[0].GetHeight() * ScaleFactor);
				RenderObj_TextureAt(TileTexture[1], XOffset + TileTexture[1].Position.X, YOffset + TileTexture[1].Position.Y, BarWidth, TileTexture[1].GetHeight() * ScaleFactor);

				if (Height > (TileTexture[0].GetHeight() * 0.95f * ScaleFactor))
				{
					Height *= (TileTexture[0].GetHeight() * 0.95f * ScaleFactor) / Height;
				}

				DrawText(InventoryItem->GetHUDText(),
					IconTexture.Position.X + XOffset + Width + 8.f * ScaleFactor,
					IconTexture.Position.Y + YOffset,
					UTHUDOwner->SmallFont,
					false,
					FVector2D(1.f,1.f),
					FLinearColor::Black,
					false,
					FLinearColor::Black,
					ScaleFactor,
					UTHUDOwner->GetHUDWidgetOpacity(),
					FLinearColor::White,
					FLinearColor(0.0f,0.0f,0.0f,0.0f),
					ETextHorzPos::Left,
					ETextVertPos::Bottom);

				IconTexture.UVs.U = InventoryItem->HUDIcon.U;
				IconTexture.UVs.V = InventoryItem->HUDIcon.V;
				IconTexture.UVs.UL = InventoryItem->HUDIcon.UL;
				IconTexture.UVs.VL = InventoryItem->HUDIcon.VL;
				IconTexture.Atlas = InventoryItem->HUDIcon.Texture;
				float Scale = ScaleFactor;
				if (InventoryItem->FlashTimer > 0.f)
				{
					Scale += InventoryItem->InitialFlashScale * (InventoryItem->FlashTimer / InventoryItem->InitialFlashTime);
					InventoryItem->FlashTimer -= DeltaTime;
				}

				RenderObj_TextureAt(IconTexture, IconTexture.Position.X + XOffset, IconTexture.Position.Y + YOffset, Scale*Width, Scale*Height);
				YOffset += TileTexture[0].GetHeight() * ScaleFactor;
			}
		}
	}
}

bool UUTHUDWidget_Powerups::ShouldDraw_Implementation(bool bShowScores)
{
	if (!UTHUDOwner->GetQuickInfoHidden())
	{
		return false;
	}
	if (UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->UTPlayerState && UTHUDOwner->UTPlayerOwner->UTPlayerState->bOnlySpectator)
	{
		return !UTHUDOwner->UTPlayerOwner->bShowCameraBinds && !UTHUDOwner->UTPlayerOwner->bRequestingSlideOut && Super::ShouldDraw_Implementation(bShowScores);
	}
		
	return Super::ShouldDraw_Implementation(bShowScores);
}
