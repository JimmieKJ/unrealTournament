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
		FontRenderInfo.bEnableShadow = true;
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
		UTDrawStringInternal(InCanvas, DrawPos + FVector2D(-1.0f, -1.0f), DrawColor);
		UTDrawStringInternal(InCanvas, DrawPos + FVector2D(-1.0f, 1.0f), DrawColor);
		UTDrawStringInternal(InCanvas, DrawPos + FVector2D(1.0f, 1.0f), DrawColor);
		UTDrawStringInternal(InCanvas, DrawPos + FVector2D(1.0f, -1.0f), DrawColor);
	}
	// If we have a shadow - draw it now
	if (bHasShadow)
	{
		DrawColor = ShadowColor;
		// Copy the Alpha from the shadow otherwise if we fade the text the shadow wont fade - which is almost certainly not what we will want.
		DrawColor.A = Color.A;
		DrawColor.A *= InCanvas->AlphaModulate;
		UTDrawStringInternal(InCanvas, DrawPos + ShadowOffset, DrawColor);
	}
	DrawColor = Color;
	DrawColor.A *= InCanvas->AlphaModulate;
	// TODO: we need to pass the shadow color and direction in the distance field case... (currently engine support is missing)
	UTDrawStringInternal(InCanvas, DrawPos, DrawColor);
}

void FUTCanvasTextItem::UTDrawStringInternal(class FCanvas* InCanvas, const FVector2D& DrawPos, const FLinearColor& InColour)
{
	DrawnSize = FVector2D(EForceInit::ForceInitToZero);
	FVector2D CurrentPos = FVector2D(EForceInit::ForceInitToZero);
	FHitProxyId HitProxyId = InCanvas->GetHitProxyId();
	FTexture* LastTexture = NULL;
	UTexture2D* Tex = NULL;
	FVector2D InvTextureSize(1.0f, 1.0f);
	const TArray< TCHAR >& Chars = Text.ToString().GetCharArray();

	// allocate extra (overlapping) space to draw distance field effects, if enabled
	float ExtraXSpace = 0.0f;
	float ExtraYSpace = 0.0f;
	if (Font->ImportOptions.bUseDistanceFieldAlpha)
	{
		if (FontRenderInfo.bEnableShadow || BlendMode == SE_BLEND_MaskedDistanceFieldShadowed || BlendMode == SE_BLEND_TranslucentDistanceFieldShadowed)
		{
			// TODO: should be based on shadow settings, but those are currently hardcoded in FBatchedElements
			ExtraXSpace = 2.0f;
			ExtraYSpace = 2.0f;
		}
		if (FontRenderInfo.GlowInfo.bEnableGlow)
		{
			// we can calculate the maximum distance a glowing pixel will be by multiplying
			// the distance field scan radius (font height / 2) by the extremes of the glow radii settings
			// this lets us get away with less padding in the texture when the glow being applied
			// won't extend to the maximum values

			float GlowRadius = Font->ImportOptions.Height * 0.5;
			// < 0.5 is outside the glyph so that's all we care about
			float EdgeRadius = FMath::Min<float>( FMath::Min<float>(FontRenderInfo.GlowInfo.GlowInnerRadius.X, FontRenderInfo.GlowInfo.GlowInnerRadius.Y),
												FMath::Min<float>(FontRenderInfo.GlowInfo.GlowOuterRadius.X, FontRenderInfo.GlowInfo.GlowOuterRadius.Y) );
			if (EdgeRadius < 0.5f)
			{
				GlowRadius = FMath::CeilToFloat(GlowRadius * (0.5f - EdgeRadius) * 2.0f);
				ExtraXSpace = FMath::Max<float>(ExtraXSpace, GlowRadius);
				ExtraYSpace = FMath::Max<float>(ExtraYSpace, GlowRadius);
			}
		}
		// clamp by the padding in the texture (prefer box-y look over bleeding to adjacent glyphs)
		ExtraXSpace = FMath::Min<float>(ExtraXSpace, FMath::TruncToFloat((Font->ImportOptions.XPadding - 1.0f) * 0.5f));
		ExtraYSpace = FMath::Min<float>(ExtraYSpace, FMath::TruncToFloat((Font->ImportOptions.YPadding - 1.0f) * 0.5f));
	}

	// Draw all characters in string.
	for (int32 i = 0; i < TextLen; i++)
	{
		int32 Ch = (int32)Font->RemapChar(Chars[i]);

		// Skip invalid characters.
		if (!Font->Characters.IsValidIndex(Ch))
		{
			continue;
		}

		FFontCharacter& Char = Font->Characters[Ch];

		if (DrawnSize.Y == 0)
		{
			// We have a valid character so initialize vertical DrawnSize
			DrawnSize.Y = Font->GetMaxCharHeight() * YScale;
		}

		if (FChar::IsLinebreak(Chars[i]))
		{
			// Set current character offset to the beginning of next line.
			CurrentPos.X = 0.0f;
			CurrentPos.Y += Font->GetMaxCharHeight() * YScale;

			// Increase the vertical DrawnSize
			DrawnSize.Y += Font->GetMaxCharHeight() * YScale;

			// Don't draw newline character
			continue;
		}

		if (Font->Textures.IsValidIndex(Char.TextureIndex) &&
			(Tex = Font->Textures[Char.TextureIndex]) != NULL &&
			Tex->Resource != NULL)
		{
			if (LastTexture != Tex->Resource || BatchedElements == NULL)
			{
				FBatchedElementParameters* BatchedElementParameters = NULL;
				BatchedElements = InCanvas->GetBatchedElements(FCanvas::ET_Triangle, BatchedElementParameters, Tex->Resource, BlendMode, FontRenderInfo.GlowInfo);
				check(BatchedElements != NULL);
				// trade-off between memory and performance by pre-allocating more reserved space 
				// for the triangles/vertices of the batched elements used to render the text tiles
				//BatchedElements->AddReserveTriangles(TextLen*2,Tex->Resource,BlendMode);
				//BatchedElements->AddReserveVertices(TextLen*4);

				FIntPoint ImportedTextureSize = Tex->GetImportedSize();
				InvTextureSize.X = 1.0f / (float)ImportedTextureSize.X;
				InvTextureSize.Y = 1.0f / (float)ImportedTextureSize.Y;
			}
			LastTexture = Tex->Resource;

			const float X = CurrentPos.X + DrawPos.X - ExtraXSpace * XScale;
			const float Y = CurrentPos.Y + DrawPos.Y + (Char.VerticalOffset - ExtraYSpace) * YScale;
			float SizeX = (Char.USize + ExtraXSpace * 2.0f) * XScale;
			const float SizeY = (Char.VSize + ExtraYSpace * 2.0f) * YScale;
			const float U = (Char.StartU - ExtraXSpace) * InvTextureSize.X;
			const float V = (Char.StartV - ExtraYSpace) * InvTextureSize.Y;
			const float SizeU = (Char.USize + ExtraXSpace * 2.0f) * InvTextureSize.X;
			const float SizeV = (Char.VSize + ExtraYSpace * 2.0f) * InvTextureSize.Y;

			float Left, Top, Right, Bottom;
			Left = X * Depth;
			Top = Y * Depth;
			Right = (X + SizeX) * Depth;
			Bottom = (Y + SizeY) * Depth;

			int32 V00 = BatchedElements->AddVertex(
				FVector4(Left, Top, 0.f, Depth),
				FVector2D(U, V),
				InColour,
				HitProxyId);
			int32 V10 = BatchedElements->AddVertex(
				FVector4(Right, Top, 0.0f, Depth),
				FVector2D(U + SizeU, V),
				InColour,
				HitProxyId);
			int32 V01 = BatchedElements->AddVertex(
				FVector4(Left, Bottom, 0.0f, Depth),
				FVector2D(U, V + SizeV),
				InColour,
				HitProxyId);
			int32 V11 = BatchedElements->AddVertex(
				FVector4(Right, Bottom, 0.0f, Depth),
				FVector2D(U + SizeU, V + SizeV),
				InColour,
				HitProxyId);

			BatchedElements->AddTriangle(V00, V10, V11, Tex->Resource, BlendMode, FontRenderInfo.GlowInfo);
			BatchedElements->AddTriangle(V00, V11, V01, Tex->Resource, BlendMode, FontRenderInfo.GlowInfo);

			// if we have another non-whitespace character to render, add the font's kerning.
			if (Chars[i + 1] && !FChar::IsWhitespace(Chars[i + 1]))
			{
				SizeX += CharIncrement;
			}

			// Update the current rendering position
			CurrentPos.X += SizeX - (ExtraXSpace * 2.0f) * XScale;

			// Increase the Horizontal DrawnSize
			if (CurrentPos.X > DrawnSize.X)
			{
				DrawnSize.X = CurrentPos.X;
			}
		}
	}
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
