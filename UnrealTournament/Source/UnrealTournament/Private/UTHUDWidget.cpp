// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#include "EngineMaterialClasses.h"
#include "BatchedElements.h"
#include "RenderResource.h"

UUTHUDWidget::UUTHUDWidget(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	Opacity = 1.0f;
	Origin = FVector2D(0.0f, 0.0f);
	ScreenPosition = FVector2D(0.0f, 0.0f);

	bScaleByDesignedResolution = true;
	bMaintainAspectRatio = true;
}

bool UUTHUDWidget::IsHidden()
{
	return bHidden;
}

void UUTHUDWidget::SetHidden(bool bIsHidden)
{
	bHidden = bIsHidden;
}

void UUTHUDWidget::PreDraw(AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter)
{
	UTHUDOwner = InUTHUDOwner;
	Canvas = InCanvas;
	CanvasCenter = InCanvasCenter;

	AspectScale = Size.Y > 0 ? Size.X / Size.Y : 0;

	// Figure out the initial position.

	RenderPosition.X = Canvas->ClipX * ScreenPosition.X;
	RenderPosition.Y = Canvas->ClipY * ScreenPosition.Y;

	RenderScale = (bScaleByDesignedResolution) ? Canvas->ClipY / WIDGET_DEFAULT_Y_RESOLUTION : 1.0f;

	// Apply any scaling
	RenderSize.Y = Size.Y * RenderScale;
	if (Size.X > 0)
	{
		RenderSize.X = (bMaintainAspectRatio ?  RenderSize.Y * AspectScale : RenderSize.X * RenderScale);
	}

	RenderPosition.X += (Position.X * RenderScale) - (RenderSize.X * Origin.X);
	RenderPosition.Y += (Position.Y * RenderScale) - (RenderSize.Y * Origin.Y);
}

void UUTHUDWidget::Draw(float DeltaTime)
{
}

void UUTHUDWidget::PostDraw(float RenderedTime)
{
	LastRenderTime = RenderedTime;
	Canvas = NULL;
}


void UUTHUDWidget::DrawText(FText Text, float X, float Y, UFont* Font, float TextScale, float DrawOpacity, FLinearColor DrawColor, ETextHorzPos::Type TextHorzAlignment, ETextVertPos::Type TextVertAlignment)
{
	if (Font && !Text.IsEmpty())
	{
		if (bScaleByDesignedResolution)
		{
			X *= RenderScale;
			Y *= RenderScale;
		}

		FVector2D RenderPos = FVector2D(RenderPosition.X + X,RenderPosition.Y + Y);
		// Handle Justification
		if (TextHorzAlignment != ETextHorzPos::Left || TextVertAlignment != ETextVertPos::Top )
		{
			float XL, YL;
			Canvas->StrLen(Font, Text.ToString(), XL, YL);
		
			if (bScaleByDesignedResolution)
			{
				XL *= RenderScale * TextScale;
				YL *= RenderScale* TextScale;
			}

			if (TextHorzAlignment != ETextHorzPos::Left)
			{
				RenderPos.X -= TextHorzAlignment == ETextHorzPos::Right ? XL : XL * 0.5f;
			}

			if (TextVertAlignment != ETextVertPos::Top)
			{
				RenderPos.Y -= TextVertAlignment == ETextVertPos::Bottom ? XL : XL * 0.5f;
			}
		}

		FCanvasTextItem TextItem(RenderPos, Text, Font, DrawColor);

		if (bScaleByDesignedResolution)
		{
			TextItem.Scale = FVector2D(RenderScale * TextScale, RenderScale * TextScale);
		}
		Canvas->DrawColor = DrawColor;
		Canvas->DrawColor.A *= DrawOpacity * UTHUDOwner->WidgetOpacity;
		Canvas->DrawItem(TextItem);
	}
}

void UUTHUDWidget::DrawTexture(UTexture* Texture, float X, float Y, float Width, float Height, float U, float V, float UL, float VL, float DrawOpacity, FLinearColor DrawColor, FVector2D RenderOffset, float Rotation, FVector2D RotPivot)
{

	float ImageAspectScale = Height > 0 ? Width / Height : 0.0f;

	if (Texture && Texture->Resource )
	{
		if (bScaleByDesignedResolution)
		{
			X *= RenderScale;
			Y *= RenderScale;
			
			Height = Height * RenderScale;
			Width = (bMaintainAspectRatio) ? (Height * ImageAspectScale) : Width * RenderScale;
		}

		FVector2D RenderPos = FVector2D(RenderPosition.X + X - (Width * RenderOffset.X), RenderPosition.Y + Y - (Height * RenderOffset.Y));

		U = U / Texture->Resource->GetSizeX();
		V = V / Texture->Resource->GetSizeY();
		UL = U + (UL / Texture->Resource->GetSizeX());
		VL = V + (VL / Texture->Resource->GetSizeY());

		DrawColor.A *= DrawOpacity * UTHUDOwner->WidgetOpacity;

		FCanvasTileItem ImageItem(RenderPos, Texture->Resource, FVector2D(Width, Height), FVector2D(U,V), FVector2D(UL,VL), DrawColor);
		ImageItem.Rotation = FRotator(0,Rotation,0);
		ImageItem.PivotPoint = RotPivot;
		ImageItem.BlendMode = ESimpleElementBlendMode::SE_BLEND_Translucent;
		Canvas->DrawItem( ImageItem );
	}
}

void UUTHUDWidget::DrawMaterial( UMaterialInterface* Material, float X, float Y, float Width, float Height, float MaterialU, float MaterialV, float MaterialUWidth, float MaterialVHeight, float DrawOpacity, FLinearColor DrawColor, FVector2D RenderOffset, float Rotation, FVector2D RotPivot)
{
	if (Material)
	{
		if (bScaleByDesignedResolution)
		{
			X *= RenderScale;
			Y *= RenderScale;
			
			Width = (bMaintainAspectRatio) ? ((Height * RenderScale) * AspectScale) : Width * RenderScale;
			Height = Height * RenderScale;
		}

		FVector2D RenderPos = FVector2D(RenderPosition.X + X - (Width * RenderOffset.X), RenderPosition.Y + Y - (Height * RenderOffset.Y));


		FCanvasTileItem MaterialItem( RenderPos, Material->GetRenderProxy(0), FVector2D( Width, Height) , FVector2D( MaterialU, MaterialV ), FVector2D( MaterialU+MaterialUWidth, MaterialV +MaterialVHeight));

		DrawColor.A *= DrawOpacity + UTHUDOwner->WidgetOpacity;
		MaterialItem.SetColor(DrawColor);
		MaterialItem.Rotation = FRotator(0, Rotation, 0);
		MaterialItem.PivotPoint = RotPivot;
		Canvas->DrawColor.A *= DrawOpacity * UTHUDOwner->WidgetOpacity;
		Canvas->DrawItem( MaterialItem );
	}
}