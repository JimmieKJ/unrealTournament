// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

/** Run highlighter used to draw underlines */
class SLATE_API FSlateTextUnderlineLineHighlighter : public ISlateLineHighlighter
{
public:
	static TSharedRef<FSlateTextUnderlineLineHighlighter> Create(const FSlateBrush& InUnderlineBrush, const FSlateFontInfo& InFontInfo, const FSlateColor InColorAndOpacity, const FVector2D InShadowOffset, const FLinearColor InShadowColorAndOpacity);

	virtual int32 OnPaint(const FPaintArgs& Args, const FTextLayout::FLineView& Line, const float OffsetX, const float Width, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	static const int32 DefaultZIndex = 1;

protected:
	FSlateTextUnderlineLineHighlighter(const FSlateBrush& InUnderlineBrush, const FSlateFontInfo& InFontInfo, const FSlateColor InColorAndOpacity, const FVector2D InShadowOffset, const FLinearColor InShadowColorAndOpacity);

	/** Brush used to draw the underline */
	FSlateBrush UnderlineBrush;

	/** Font the underline is associated with */
	FSlateFontInfo FontInfo;

	/** The color to draw the underline (typically matches the text its associated with) */
	FSlateColor ColorAndOpacity;

	/** Offset at which to draw the shadow (if any) */
	FVector2D ShadowOffset;

	/** The color to draw the shadow */
	FLinearColor ShadowColorAndOpacity;
};
