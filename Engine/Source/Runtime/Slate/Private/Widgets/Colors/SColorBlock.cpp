// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SColorBlock::Construct( const FArguments& InArgs )
{
	Color = InArgs._Color;
	ColorIsHSV = InArgs._ColorIsHSV;
	IgnoreAlpha = InArgs._IgnoreAlpha;
	ShowBackgroundForAlpha = InArgs._ShowBackgroundForAlpha;
	MouseButtonDownHandler = InArgs._OnMouseButtonDown;
	bUseSRGB = InArgs._UseSRGB;
	ColorBlockSize = InArgs._Size;
}

int32 SColorBlock::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const FSlateBrush* GenericBrush = FCoreStyle::Get().GetBrush( "GenericWhiteBox" );

	const ESlateDrawEffect::Type DrawEffects = ESlateDrawEffect::None;
	
	FLinearColor InColor = Color.Get();
	if (ColorIsHSV.Get())
	{
		InColor = InColor.HSVToLinearRGB();
	}
	if (IgnoreAlpha.Get())
	{
		InColor.A = 1.f;
	}
	
	const FColor DrawColor = InColor.ToFColor(bUseSRGB.Get());
	if( ShowBackgroundForAlpha.Get() && DrawColor.A < 255 )
	{
		// If we are showing a background pattern and the colors is transparent, draw a checker pattern
		const FSlateBrush* CheckerBrush = FCoreStyle::Get().GetBrush("ColorPicker.AlphaBackground");
		FSlateDrawElement::MakeBox( OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), CheckerBrush, MyClippingRect, DrawEffects );
	}
	
	// determine if it is HDR
	const float MaxRGB = FMath::Max3(InColor.R, InColor.G, InColor.B);
	if (MaxRGB > 1.f)
	{
		FLinearColor NormalizedLinearColor = InColor / MaxRGB;
		NormalizedLinearColor.A = InColor.A;
		const FLinearColor DrawNormalizedColor = InWidgetStyle.GetColorAndOpacityTint() * NormalizedLinearColor.ToFColor(bUseSRGB.Get());

		FLinearColor ClampedLinearColor = InColor;
		ClampedLinearColor.A = InColor.A * MaxRGB;
		const FLinearColor DrawClampedColor = InWidgetStyle.GetColorAndOpacityTint() * ClampedLinearColor.ToFColor(bUseSRGB.Get());

		TArray<FSlateGradientStop> GradientStops;
		
		GradientStops.Add( FSlateGradientStop( FVector2D::ZeroVector, DrawNormalizedColor ) );
		GradientStops.Add( FSlateGradientStop( AllottedGeometry.Size * 0.5f, DrawClampedColor ) );
		GradientStops.Add( FSlateGradientStop( AllottedGeometry.Size, DrawNormalizedColor ) );

		FSlateDrawElement::MakeGradient(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(),
			GradientStops,
			(AllottedGeometry.Size.X > AllottedGeometry.Size.Y) ? Orient_Vertical : Orient_Horizontal,
			MyClippingRect,
			DrawEffects
		);
	}
	else
	{
		FSlateDrawElement::MakeBox( OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), GenericBrush, MyClippingRect, DrawEffects, InWidgetStyle.GetColorAndOpacityTint() * DrawColor );
	}

	return LayerId + 1;
}

FReply SColorBlock::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseButtonDownHandler.IsBound() )
	{
		// If a handler is assigned, call it.
		return MouseButtonDownHandler.Execute(MyGeometry, MouseEvent);
	}
	else
	{
		// otherwise the event is unhandled.
		return FReply::Unhandled();
	}
}

FVector2D SColorBlock::ComputeDesiredSize( float ) const
{
	return ColorBlockSize.Get();
}
