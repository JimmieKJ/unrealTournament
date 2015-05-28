// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "PlainTextLayoutMarshaller.h"
#include "TextBlockLayout.h"

void STextBlock::Construct( const FArguments& InArgs )
{
	TextStyle = InArgs._TextStyle;

	HighlightText = InArgs._HighlightText;
	WrapTextAt = InArgs._WrapTextAt;
	AutoWrapText = InArgs._AutoWrapText;
	Margin = InArgs._Margin;
	LineHeightPercentage = InArgs._LineHeightPercentage;
	Justification = InArgs._Justification;
	MinDesiredWidth = InArgs._MinDesiredWidth;

	Font = InArgs._Font;
	ColorAndOpacity = InArgs._ColorAndOpacity;
	ShadowOffset = InArgs._ShadowOffset;
	ShadowColorAndOpacity = InArgs._ShadowColorAndOpacity;
	HighlightColor = InArgs._HighlightColor;
	HighlightShape = InArgs._HighlightShape;

	OnDoubleClicked = InArgs._OnDoubleClicked;

	BoundText = InArgs._Text;

#if WITH_FANCY_TEXT

	// We use a dummy style here (as it may not be safe to call the delegates used to compute the style), but the correct style is set by ComputeDesiredSize
	TextLayoutCache = FTextBlockLayout::Create(FTextBlockStyle::GetDefault(), FPlainTextLayoutMarshaller::Create(), InArgs._LineBreakPolicy);

#endif//WITH_FANCY_TEXT
}

FSlateFontInfo STextBlock::GetFont() const
{
	return Font.IsSet() ? Font.Get() : TextStyle->Font;
}

FSlateColor STextBlock::GetColorAndOpacity() const
{
	return ColorAndOpacity.IsSet() ? ColorAndOpacity.Get() : TextStyle->ColorAndOpacity;
}

FVector2D STextBlock::GetShadowOffset() const
{
	return ShadowOffset.IsSet() ? ShadowOffset.Get() : TextStyle->ShadowOffset;
}

FLinearColor STextBlock::GetShadowColorAndOpacity() const
{
	return ShadowColorAndOpacity.IsSet() ? ShadowColorAndOpacity.Get() : TextStyle->ShadowColorAndOpacity;
}

FLinearColor STextBlock::GetHighlightColor() const
{
	return HighlightColor.IsSet() ? HighlightColor.Get() : TextStyle->HighlightColor;
}

const FSlateBrush* STextBlock::GetHighlightShape() const
{
	return HighlightShape.IsSet() ? HighlightShape.Get() : &TextStyle->HighlightShape;
}

void STextBlock::SetText( const TAttribute< FString >& InText )
{
	struct Local
	{
		static FText PassThroughAttribute( TAttribute< FString > InString )
		{
			return FText::FromString( InString.Get( TEXT("") ) );
		}
	};

	BoundText = TAttribute< FText >::Create(TAttribute<FText>::FGetter::CreateStatic( &Local::PassThroughAttribute, InText) );
}

void STextBlock::SetText( const FString& InText )
{
	BoundText = FText::FromString( InText );
}

void STextBlock::SetText( const TAttribute< FText >& InText )
{
	BoundText = InText;
}

void STextBlock::SetText( const FText& InText )
{
	BoundText = InText;
}

int32 STextBlock::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
#if WITH_FANCY_TEXT

	// OnPaint will also update the text layout cache if required
	LayerId = TextLayoutCache->OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, ShouldBeEnabled(bParentEnabled));

#else//WITH_FANCY_TEXT

	// Simple stubbed version for when WITH_FANCY_TEXT is disabled (which should only ever be for testing purposes)
	// Just draw the text as is, with no attempt at handling wrapping or highlighting
	{
		const FSlateRect ClippingRect = AllottedGeometry.GetClippingRect().IntersectionWith(MyClippingRect);
		const ESlateDrawEffect::Type DrawEffects = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
		const FLinearColor CurShadowColor = GetShadowColorAndOpacity();
		const FVector2D CurShadowOffset = GetShadowOffset();
		const bool ShouldDropShadow = CurShadowOffset.Size() > 0 && CurShadowColor.A > 0;
		const FSlateFontInfo FontInfo = GetFont();
		const FText& TextToDraw = BoundText.Get(FText::GetEmpty());

		// Draw the optional shadow
		if (ShouldDropShadow)
		{
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(CurShadowOffset, AllottedGeometry.Size),
				TextToDraw,
				FontInfo,
				ClippingRect,
				DrawEffects,
				CurShadowColor * InWidgetStyle.GetColorAndOpacityTint()
				);
		}

		// Draw the text itself
		FSlateDrawElement::MakeText(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(),
			TextToDraw,
			FontInfo,
			ClippingRect,
			DrawEffects,
			InWidgetStyle.GetColorAndOpacityTint() * GetColorAndOpacity().GetColor(InWidgetStyle)
			);
	}

#endif//WITH_FANCY_TEXT

	return LayerId;
}

FReply STextBlock::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	if ( InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		if( OnDoubleClicked.IsBound() )
		{
			OnDoubleClicked.Execute();

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FVector2D STextBlock::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
#if WITH_FANCY_TEXT

	// todo: jdale - The scale needs to be passed to ComputeDesiredSize
	//const float Scale = CachedScale;

	// ComputeDesiredSize will also update the text layout cache if required
	const FVector2D TextSize = TextLayoutCache->ComputeDesiredSize(
		FTextBlockLayout::FWidgetArgs(BoundText, HighlightText, WrapTextAt, AutoWrapText, Margin, LineHeightPercentage, Justification), 
		LayoutScaleMultiplier, GetComputedTextStyle()
		);

#else//WITH_FANCY_TEXT

	// Simple stubbed version for when WITH_FANCY_TEXT is disabled (which should only ever be for testing purposes)
	// Just measure the text as is, with no attempt at handling wrapping
	const TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D TextSize = FontMeasureService->Measure(BoundText.Get(FText::GetEmpty()), GetFont());

#endif//WITH_FANCY_TEXT

	return FVector2D(FMath::Max(MinDesiredWidth.Get(0.0f), TextSize.X), TextSize.Y);
}

void STextBlock::SetFont(const TAttribute< FSlateFontInfo >& InFont)
{
	Font = InFont;
}

void STextBlock::SetColorAndOpacity(const TAttribute<FSlateColor>& InColorAndOpacity)
{
	ColorAndOpacity = InColorAndOpacity;
}

void STextBlock::SetTextStyle(const FTextBlockStyle* InTextStyle)
{
	TextStyle = InTextStyle;
}

void STextBlock::SetWrapTextAt(const TAttribute<float>& InWrapTextAt)
{
	WrapTextAt = InWrapTextAt;
}

void STextBlock::SetAutoWrapText(const TAttribute<bool>& InAutoWrapText)
{
	AutoWrapText = InAutoWrapText;
}

void STextBlock::SetShadowOffset(const TAttribute<FVector2D>& InShadowOffset)
{
	ShadowOffset = InShadowOffset;
}

void STextBlock::SetShadowColorAndOpacity(const TAttribute<FLinearColor>& InShadowColorAndOpacity)
{
	ShadowColorAndOpacity = InShadowColorAndOpacity;
}

void STextBlock::SetMinDesiredWidth(const TAttribute<float>& InMinDesiredWidth)
{
	MinDesiredWidth = InMinDesiredWidth;
}

void STextBlock::SetLineHeightPercentage(const TAttribute<float>& InLineHeightPercentage)
{
	LineHeightPercentage = InLineHeightPercentage;
}

void STextBlock::SetMargin(const TAttribute<FMargin>& InMargin)
{
	Margin = InMargin;
}

void STextBlock::SetJustification(const TAttribute<ETextJustify::Type>& InJustification)
{
	Justification = InJustification;
}

FTextBlockStyle STextBlock::GetComputedTextStyle() const
{
	FTextBlockStyle ComputedStyle = *TextStyle;
	ComputedStyle.SetFont( GetFont() );
	ComputedStyle.SetColorAndOpacity( GetColorAndOpacity() );
	ComputedStyle.SetShadowOffset( GetShadowOffset() );
	ComputedStyle.SetShadowColorAndOpacity( GetShadowColorAndOpacity() );
	ComputedStyle.SetHighlightColor( GetHighlightColor() );
	ComputedStyle.SetHighlightShape( *GetHighlightShape() );
	return ComputedStyle;
}

