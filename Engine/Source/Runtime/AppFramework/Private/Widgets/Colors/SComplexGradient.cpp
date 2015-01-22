// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AppFrameworkPrivatePCH.h"
#include "SComplexGradient.h"


/* SComplexGradient interface
 *****************************************************************************/

void SComplexGradient::Construct( const FArguments& InArgs )
{
	GradientColors = InArgs._GradientColors;
	bHasAlphaBackground = InArgs._HasAlphaBackground.Get();
	Orientation = InArgs._Orientation.Get();
}


/* SCompoundWidget overrides
 *****************************************************************************/

int32 SComplexGradient::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	ESlateDrawEffect::Type DrawEffects = (bParentEnabled && IsEnabled()) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	if (bHasAlphaBackground)
	{
		const FSlateBrush* StyleInfo = FCoreStyle::Get().GetBrush("ColorPicker.AlphaBackground");

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			StyleInfo,
			MyClippingRect,
			DrawEffects
		);
	}

	const TArray<FLinearColor>& Colors = GradientColors.Get();
	int32 NumColors = Colors.Num();

	if (NumColors > 0)
	{
		TArray<FSlateGradientStop> GradientStops;

		for (int32 ColorIndex = 0; ColorIndex < NumColors; ++ColorIndex)
		{
			GradientStops.Add(FSlateGradientStop(AllottedGeometry.Size * (float(ColorIndex) / (NumColors - 1)), Colors[ColorIndex]));
		}

		FSlateDrawElement::MakeGradient(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(),
			GradientStops,
			Orientation,
			MyClippingRect,
			DrawEffects
		);
	}

	return LayerId + 1;
}
