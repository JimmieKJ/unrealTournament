// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#include "BatchedElements.h"
#include "RenderResource.h"

DECLARE_CYCLE_STAT(TEXT("CanvasTextItem Time"), STAT_Canvas_TextItemTime, STATGROUP_Canvas);
void FUTCanvasTextItem::Draw(FCanvas* InCanvas)
{
	SCOPE_CYCLE_COUNTER(STAT_Canvas_TextItemTime);

	if (InCanvas == NULL || Font == NULL || Text.IsEmpty())
	{
		return;
	}

	XScale = Scale.X;
	YScale = Scale.Y;

	bool bHasShadow = ShadowOffset.Size() != 0.0f;
	if (FontRenderInfo.bEnableShadow && !bHasShadow)
	{
		EnableShadow(FLinearColor::Black);
		bHasShadow = true;
	}
	if (Font->ImportOptions.bUseDistanceFieldAlpha)
	{
		// convert blend mode to distance field type
		switch (BlendMode)
		{
		case SE_BLEND_Translucent:
			BlendMode = (bHasShadow || FontRenderInfo.bEnableShadow) ? SE_BLEND_TranslucentDistanceFieldShadowed : SE_BLEND_TranslucentDistanceField;
			break;
		case SE_BLEND_Masked:
			BlendMode = (bHasShadow || FontRenderInfo.bEnableShadow) ? SE_BLEND_MaskedDistanceFieldShadowed : SE_BLEND_MaskedDistanceField;
			break;
		}

		// we don't need to use the double draw method because the shader will do it better
		bHasShadow = false;
	}

	CharIncrement = ((float)Font->Kerning + HorizSpacingAdjust) * Scale.X;
	DrawnSize.Y = Font->GetMaxCharHeight() * Scale.Y;

	FVector2D DrawPos(Position.X, Position.Y);

	// If we are centering the string or we want to fix stereoscopic rending issues we need to measure the string
	if ((bCentreX || bCentreY) || (!bDontCorrectStereoscopic))
	{
		FTextSizingParameters Parameters(Font, Scale.X, Scale.Y);
		UCanvas::CanvasStringSize(Parameters, *Text.ToString());

		// Calculate the offset if we are centering
		if (bCentreX || bCentreY)
		{
			// Note we drop the fraction after the length divide or we can end up with coords on 1/2 pixel boundaries
			if (bCentreX)
			{
				DrawPos.X = DrawPos.X - (int)(Parameters.DrawXL / 2);
			}
			if (bCentreY)
			{
				DrawPos.Y = DrawPos.Y - (int)(Parameters.DrawYL / 2);
			}
		}

		// Check if we want to correct the stereo3d issues - if we do, render the correction now
		bool CorrectStereo = !bDontCorrectStereoscopic  && GEngine->IsStereoscopic3D();
		if (CorrectStereo)
		{
			FVector2D StereoOutlineBoxSize(2.0f, 2.0f);
			TileItem.MaterialRenderProxy = GEngine->RemoveSurfaceMaterial->GetRenderProxy(false);
			TileItem.Position = DrawPos - StereoOutlineBoxSize;
			FVector2D CorrectionSize = FVector2D(Parameters.DrawXL, Parameters.DrawYL) + StereoOutlineBoxSize + StereoOutlineBoxSize;
			TileItem.Size = CorrectionSize;
			TileItem.bFreezeTime = true;
			TileItem.Draw(InCanvas);
		}
	}

	FLinearColor DrawColor;
	BatchedElements = NULL;
	TextLen = Text.ToString().Len();
	if (bOutlined && !FontRenderInfo.GlowInfo.bEnableGlow) // efficient distance field glow takes priority
	{
		DrawColor = OutlineColor;
		DrawColor.A *= InCanvas->AlphaModulate;
		DrawStringInternal(InCanvas, DrawPos + FVector2D(-1.0f, -1.0f), DrawColor);
		DrawStringInternal(InCanvas, DrawPos + FVector2D(-1.0f, 1.0f), DrawColor);
		DrawStringInternal(InCanvas, DrawPos + FVector2D(1.0f, 1.0f), DrawColor);
		DrawStringInternal(InCanvas, DrawPos + FVector2D(1.0f, -1.0f), DrawColor);
	}
	// If we have a shadow - draw it now
	if (bHasShadow)
	{
		DrawColor = ShadowColor;
		// Copy the Alpha from the shadow otherwise if we fade the text the shadow wont fade - which is almost certainly not what we will want.
		DrawColor.A = Color.A;
		DrawColor.A *= InCanvas->AlphaModulate;
		DrawStringInternal(InCanvas, DrawPos + ShadowOffset, DrawColor);
	}
	DrawColor = Color;
	DrawColor.A *= InCanvas->AlphaModulate;
	// TODO: we need to pass the shadow color and direction in the distance field case... (currently engine support is missing)
	DrawStringInternal(InCanvas, DrawPos, DrawColor);
}

UUTHUDWidget::UUTHUDWidget(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	bIgnoreHUDBaseColor = false;

	Opacity = 1.0f;
	Origin = FVector2D(0.0f, 0.0f);
	ScreenPosition = FVector2D(0.0f, 0.0f);

	bScaleByDesignedResolution = true;
	bMaintainAspectRatio = true;
}

void UUTHUDWidget::InitializeWidget(AUTHUD* Hud)
{
}

UCanvas* UUTHUDWidget::GetCanvas()
{
	return Canvas;
}

FVector2D UUTHUDWidget::GetRenderPosition()
{
	return RenderPosition;
}

FVector2D UUTHUDWidget::GetRenderSize()
{
	return RenderSize;
}

float UUTHUDWidget::GetRenderScale()
{
	return RenderScale;
}

bool UUTHUDWidget::IsHidden()
{
	return bHidden;
}

void UUTHUDWidget::SetHidden(bool bIsHidden)
{
	bHidden = bIsHidden;
}

bool UUTHUDWidget::ShouldDraw_Implementation(bool bShowScores)
{
	return !bShowScores;
}

void UUTHUDWidget::PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter)
{
	UTHUDOwner = InUTHUDOwner;

	if (UTHUDOwner != NULL && UTHUDOwner->UTPlayerOwner != NULL)
	{
		UTPlayerOwner = UTHUDOwner->UTPlayerOwner;
		if (UTPlayerOwner != NULL)
		{
			UTCharacterOwner = UTPlayerOwner->GetUTCharacter();
		}
	}

	Canvas = InCanvas;
	CanvasCenter = InCanvasCenter;

	AspectScale = Size.Y > 0 ? Size.X / Size.Y : 1.0;

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

void UUTHUDWidget::Draw_Implementation(float DeltaTime)
{
}

void UUTHUDWidget::PostDraw(float RenderedTime)
{
	LastRenderTime = RenderedTime;
	Canvas = NULL;
	UTPlayerOwner = NULL;
	UTCharacterOwner = NULL;
}


FVector2D UUTHUDWidget::DrawText(FText Text, float X, float Y, UFont* Font, float TextScale, float DrawOpacity, FLinearColor DrawColor, ETextHorzPos::Type TextHorzAlignment, ETextVertPos::Type TextVertAlignment)
{
	return DrawText(Text, X, Y, Font, false, FVector2D::ZeroVector, FLinearColor::Black, false, FLinearColor::Black, TextScale, DrawOpacity, DrawColor, TextHorzAlignment, TextVertAlignment);
}

FVector2D UUTHUDWidget::DrawText(FText Text, float X, float Y, UFont* Font, FLinearColor OutlineColor, float TextScale, float DrawOpacity, FLinearColor DrawColor, ETextHorzPos::Type TextHorzAlignment, ETextVertPos::Type TextVertAlignment)
{
	return DrawText(Text, X, Y, Font, false, FVector2D::ZeroVector, FLinearColor::Black, true, OutlineColor, TextScale, DrawOpacity, DrawColor, TextHorzAlignment, TextVertAlignment);
}

FVector2D UUTHUDWidget::DrawText(FText Text, float X, float Y, UFont* Font, FVector2D ShadowDirection, FLinearColor ShadowColor, float TextScale, float DrawOpacity, FLinearColor DrawColor, ETextHorzPos::Type TextHorzAlignment, ETextVertPos::Type TextVertAlignment)
{
	return DrawText(Text, X, Y, Font, true, ShadowDirection, ShadowColor, false, FLinearColor::Black, TextScale, DrawOpacity, DrawColor, TextHorzAlignment, TextVertAlignment);
}
FVector2D UUTHUDWidget::DrawText(FText Text, float X, float Y, UFont* Font, FVector2D ShadowDirection, FLinearColor ShadowColor, FLinearColor OutlineColor, float TextScale, float DrawOpacity, FLinearColor DrawColor, ETextHorzPos::Type TextHorzAlignment, ETextVertPos::Type TextVertAlignment)
{
	return DrawText(Text, X, Y, Font, true, ShadowDirection, ShadowColor, true, OutlineColor, TextScale, DrawOpacity, DrawColor, TextHorzAlignment, TextVertAlignment);
}
FVector2D UUTHUDWidget::DrawText(FText Text, float X, float Y, UFont* Font, const FFontRenderInfo& RenderInfo, float TextScale, float DrawOpacity, FLinearColor DrawColor, ETextHorzPos::Type TextHorzAlignment, ETextVertPos::Type TextVertAlignment)
{
	return DrawText(Text, X, Y, Font, false, FVector2D::ZeroVector, FLinearColor::Black, false, FLinearColor::Black, TextScale, DrawOpacity, DrawColor, TextHorzAlignment, TextVertAlignment, RenderInfo);
}

FVector2D UUTHUDWidget::DrawText(FText Text, float X, float Y, UFont* Font, bool bDrawShadow, FVector2D ShadowDirection, FLinearColor ShadowColor, bool bDrawOutline, FLinearColor OutlineColor, float TextScale, float DrawOpacity, FLinearColor DrawColor, ETextHorzPos::Type TextHorzAlignment, ETextVertPos::Type TextVertAlignment, const FFontRenderInfo& RenderInfo)
{

	float XL = 0.0f, YL = 0.0f;
	if (Font && !Text.IsEmpty())
	{
		Canvas->StrLen(Font, Text.ToString(), XL, YL);

		if (bScaleByDesignedResolution)
		{
			X *= RenderScale;
			Y *= RenderScale;
		}

		FVector2D RenderPos = FVector2D(RenderPosition.X + X,RenderPosition.Y + Y);
		// Handle Justification
		if (TextHorzAlignment != ETextHorzPos::Left || TextVertAlignment != ETextVertPos::Top )
		{
		
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
				RenderPos.Y -= TextVertAlignment == ETextVertPos::Bottom ? YL : YL * 0.5f;
			}
		}

		DrawColor.A *= DrawOpacity * UTHUDOwner->WidgetOpacity;
		Canvas->DrawColor = DrawColor;

		FUTCanvasTextItem TextItem(RenderPos, Text, Font, DrawColor);
		TextItem.FontRenderInfo = RenderInfo;

		if (bDrawOutline)
		{
			TextItem.bOutlined = true;
			TextItem.OutlineColor = OutlineColor;
			TextItem.OutlineColor.A *= DrawOpacity * UTHUDOwner->WidgetOpacity;
		}

		if (bDrawShadow)
		{
			ShadowColor.A *= DrawOpacity * UTHUDOwner->WidgetOpacity;
			TextItem.EnableShadow(ShadowColor, ShadowDirection);
		}

		if (bScaleByDesignedResolution)
		{
			TextItem.Scale = FVector2D(RenderScale * TextScale, RenderScale * TextScale);
		}
		Canvas->DrawItem(TextItem);
	}

	return FVector2D(XL,YL);
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

FLinearColor UUTHUDWidget::ApplyHUDColor(FLinearColor DrawColor)
{
	if (!bIgnoreHUDBaseColor)
	{
		DrawColor *= UTHUDOwner->GetBaseHUDColor();
	}
	return DrawColor;
}
