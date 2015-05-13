// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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
}

void UUTHUDWidget_Powerups::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);
}

void UUTHUDWidget_Powerups::Draw_Implementation(float DeltaTime)
{
	if (UTCharacterOwner && TimeText.Font)
	{
		int32 IconCount = 0;
		float BarWidth = 0;
		
		float TextWidth;
		float TextHeight;
		Canvas->TextSize(TimeText.Font, TEXT("00"), TextWidth, TextHeight);

		struct FRenderItemInfo
		{
			AUTInventory* InventoryItem;
			float Width;
			float Height;

			FText TextToRender;

			FRenderItemInfo() {};
			FRenderItemInfo(AUTInventory* inInventoryItem, float inWidth, float inHeight, FText inTextToRender)
						   : InventoryItem(inInventoryItem)
						   , Width(inWidth)
						   , Height(inHeight)
						   , TextToRender(inTextToRender)
			{};
		};
		
		TArray<FRenderItemInfo> RenderItems;

		// First pass is a sizing pass and builds the RenderItems Array.
		for (TInventoryIterator<> It(UTCharacterOwner); It; ++It)
		{
			AUTInventory* InventoryItem = (*It);
			if (InventoryItem && InventoryItem->HUDShouldRender(this))
			{
				float Height = InventoryItem->HUDIcon.VL;
				if (Height > (TileTexture[0].GetHeight() * 0.95))
				{
					Height *= (TileTexture[0].GetHeight() * 0.95) / Height;
				}

				float Width = (Height * (InventoryItem->HUDIcon.UL / InventoryItem->HUDIcon.VL));
				FText Text = InventoryItem->GetHUDText();
				BarWidth += Width + 10.0f + (Text.IsEmpty() ? 0.0f : TextWidth);
				int32 InsertPoint = -1;
				for (int32 i=0;i<RenderItems.Num();i++)
				{
					if (RenderItems[i].InventoryItem && RenderItems[i].InventoryItem->HUDRenderPriority > InventoryItem->HUDRenderPriority)
					{
						InsertPoint = i;
						break;
					}
				}

				if (InsertPoint >= 0)
				{
					RenderItems.Insert(FRenderItemInfo(InventoryItem, Width, Height, Text),InsertPoint);
				}
				else
				{
					RenderItems.Add(FRenderItemInfo(InventoryItem, Width, Height, Text));
				}
				IconCount++;
			}
		}

		if (IconCount == 0) return;

		BarWidth += 5.0;	// A little extra space at the end
		float XOffset = 0;

		RenderObj_TextureAt(LeftTexture[0], XOffset + LeftTexture[0].Position.X, LeftTexture[0].Position.Y, LeftTexture[0].GetWidth(), LeftTexture[0].GetHeight());
		RenderObj_TextureAt(LeftTexture[1], XOffset + LeftTexture[1].Position.X, LeftTexture[1].Position.Y, LeftTexture[1].GetWidth(), LeftTexture[1].GetHeight());

		XOffset += LeftTexture[0].GetWidth();

		RenderObj_TextureAt(TileTexture[0], XOffset + TileTexture[0].Position.X, TileTexture[0].Position.Y, BarWidth, TileTexture[0].GetHeight());
		RenderObj_TextureAt(TileTexture[1], XOffset + TileTexture[1].Position.X, TileTexture[1].Position.Y, BarWidth, TileTexture[1].GetHeight());

		RenderObj_TextureAt(RightTexture[0], XOffset + BarWidth + RightTexture[0].Position.X, RightTexture[0].Position.Y, RightTexture[0].GetWidth(), RightTexture[0].GetHeight());
		RenderObj_TextureAt(RightTexture[1], XOffset + BarWidth + RightTexture[1].Position.X, RightTexture[1].Position.Y, RightTexture[1].GetWidth(), RightTexture[1].GetHeight());

		for (int32 i=0;i<RenderItems.Num(); i++)
		{
			AUTInventory* InventoryItem = RenderItems[i].InventoryItem;
			if (InventoryItem)
			{
				// Draw the Text

				float DrawXOffset = XOffset + TextWidth;

				if (!RenderItems[i].TextToRender.IsEmpty())
				{
					TimeText.Text = RenderItems[i].TextToRender;
					RenderObj_TextAt(TimeText, DrawXOffset + TimeText.Position.X, TimeText.Position.Y);
					XOffset += TextWidth;
				}

				IconTexture.UVs.U = InventoryItem->HUDIcon.U;
				IconTexture.UVs.V = InventoryItem->HUDIcon.V;
				IconTexture.UVs.UL = InventoryItem->HUDIcon.UL;
				IconTexture.UVs.VL = InventoryItem->HUDIcon.VL;
				IconTexture.Atlas = InventoryItem->HUDIcon.Texture;
				float Scale = 1.f;
				if (InventoryItem->FlashTimer > 0.f)
				{
					Scale += InventoryItem->InitialFlashScale * (InventoryItem->FlashTimer / InventoryItem->InitialFlashTime);
					InventoryItem->FlashTimer -= DeltaTime;
				}

				RenderObj_TextureAt(IconTexture, DrawXOffset + IconTexture.Position.X, IconTexture.Position.Y, Scale*RenderItems[i].Width, Scale*RenderItems[i].Height);
				XOffset += RenderItems[i].Width;
			}
		}
	}
}