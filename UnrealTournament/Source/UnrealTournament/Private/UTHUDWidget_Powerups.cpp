// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
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

/**
 *	We aren't going tor use DrawAllRenderObjects.  Instead we are going to have a nice little custom bit of drawing based on what weapon gropup this 
 *  is.
 **/
void UUTHUDWidget_Powerups::Draw_Implementation(float DeltaTime)
{
	if (UTCharacterOwner)
	{

		int32 IconCount = 0;
		float BarWidth = 0;

		// Size all of the characters.

		//TimeText.Text = FText::FromString(TEXT("00"));
		FVector2D TextSize = FVector2D(32, 59);

		for (TInventoryIterator<> It(UTCharacterOwner); It; ++It)
		{
			AUTTimedPowerup* Powerup = Cast<AUTTimedPowerup>(*It);
			if (Powerup)
			{
				float Height = Powerup->HUDIcon.VL;
				if (Height > (TileTexture[0].GetHeight() * 0.95))
				{
					Height *= (TileTexture[0].GetHeight() * 0.95) / Height;
				}
				BarWidth += 10.0 + TextSize.X + 5.0 + (Height * (Powerup->HUDIcon.UL / Powerup->HUDIcon.VL));
				IconCount++;
			
			}
			else
			{
				AUTJumpBoots* JumpBoots = Cast<AUTJumpBoots>(*It);
				if (JumpBoots)
				{

					BarWidth += 10.0 + TextSize.X + 5.0 + JumpBootsTexture.GetWidth();
					IconCount++;
				}
			}

		}

		BarWidth += 5.0;	// A little extra space at the end

		if (IconCount == 0) return;

		float XOffset = LeftTexture[0].GetWidth();

		RenderObj_TextureAt(LeftTexture[0], XOffset + LeftTexture[0].Position.X, LeftTexture[0].Position.Y, LeftTexture[0].GetWidth(), LeftTexture[0].GetHeight());
		RenderObj_TextureAt(LeftTexture[1], XOffset + LeftTexture[1].Position.X, LeftTexture[1].Position.Y, LeftTexture[1].GetWidth(), LeftTexture[1].GetHeight());


		XOffset += LeftTexture[0].GetWidth();

		RenderObj_TextureAt(TileTexture[0], XOffset + TileTexture[0].Position.X, TileTexture[0].Position.Y, BarWidth, TileTexture[0].GetHeight());
		RenderObj_TextureAt(TileTexture[1], XOffset + TileTexture[1].Position.X, TileTexture[1].Position.Y, BarWidth, TileTexture[1].GetHeight());

		RenderObj_TextureAt(RightTexture[0], XOffset +BarWidth + RightTexture[0].Position.X, RightTexture[0].Position.Y, RightTexture[0].GetWidth(), RightTexture[0].GetHeight());
		RenderObj_TextureAt(RightTexture[1], XOffset +BarWidth + RightTexture[1].Position.X, RightTexture[1].Position.Y, RightTexture[1].GetWidth(), RightTexture[1].GetHeight());

		for (TInventoryIterator<> It(UTCharacterOwner); It; ++It)
		{
			AUTTimedPowerup* Powerup = Cast<AUTTimedPowerup>(*It);
			if (Powerup)
			{
				XOffset += 10 + TextSize.X;

				int TimeRemaining = int(int(Powerup->TimeRemaining));
				TimeText.Text = FText::AsNumber(TimeRemaining);
				RenderObj_TextAt(TimeText, XOffset + TimeText.Position.X, TimeText.Position.Y);
				XOffset += 5;

				IconTexture.UVs.U = Powerup->HUDIcon.U;
				IconTexture.UVs.V = Powerup->HUDIcon.V;
				IconTexture.UVs.UL = Powerup->HUDIcon.UL;
				IconTexture.UVs.VL = Powerup->HUDIcon.VL;
				IconTexture.Atlas = Powerup->HUDIcon.Texture;
	
					// Force it to fit.

				float Height = Powerup->HUDIcon.VL;
				if (Height > (TileTexture[0].GetHeight() * 0.95))
				{
					Height *= (TileTexture[0].GetHeight() * 0.95) / Height;
				}

				float Width = Height * (Powerup->HUDIcon.UL / Powerup->HUDIcon.VL);

			
				RenderObj_TextureAt(IconTexture, XOffset + IconTexture.Position.X, IconTexture.Position.Y, Width, Height);
				XOffset += Width;
			}
			else
			{
				AUTJumpBoots* JumpBoots = Cast<AUTJumpBoots>(*It);
				if (JumpBoots)
				{
					XOffset += 10 + TextSize.X;
					TimeText.Text = FText::FromString( FString::Printf(TEXT("x%i"),JumpBoots->NumJumps));
					RenderObj_TextAt(TimeText, XOffset + TimeText.Position.X, TimeText.Position.Y);
					XOffset += 5;
			
					RenderObj_TextureAt(JumpBootsTexture, XOffset + JumpBootsTexture.Position.X, JumpBootsTexture.Position.Y, JumpBootsTexture.GetWidth(), JumpBootsTexture.GetHeight());
					XOffset += JumpBootsTexture.GetWidth();
				}
			}
		}
	}
}