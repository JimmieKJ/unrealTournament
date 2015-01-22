// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FSlateTextLayout;
class FBaseTextLayoutMarshaller;
class ISlateRunRenderer;

#if WITH_FANCY_TEXT

/** Class to handle the cached layout of STextBlock/SRichTextBlock by proxying around a FTextLayout */
class FTextBlockLayout
{
public:
	struct FWidgetArgs
	{
		FORCEINLINE FWidgetArgs(
			const TAttribute<FText>& InText, 
			const TAttribute<FText>& InHighlightText, 
			const TAttribute<float>& InWrapTextAt, 
			const TAttribute<bool>& InAutoWrapText, 
			const TAttribute<FMargin>& InMargin, 
			const TAttribute<float>& InLineHeightPercentage, 
			const TAttribute<ETextJustify::Type>& InJustification
			)
			: Text(InText)
			, HighlightText(InHighlightText)
			, WrapTextAt(InWrapTextAt)
			, AutoWrapText(InAutoWrapText)
			, Margin(InMargin)
			, LineHeightPercentage(InLineHeightPercentage)
			, Justification(InJustification)
		{
		}

		const TAttribute<FText>& Text;
		const TAttribute<FText>& HighlightText;
		const TAttribute<float>& WrapTextAt;
		const TAttribute<bool>& AutoWrapText;
		const TAttribute<FMargin>& Margin;
		const TAttribute<float>& LineHeightPercentage;
		const TAttribute<ETextJustify::Type>& Justification;
	};

	static TSharedRef<FTextBlockLayout> Create(FTextBlockStyle InDefaultTextStyle, TSharedRef<FBaseTextLayoutMarshaller> InMarshaller, TSharedPtr<IBreakIterator> InLineBreakPolicy);

	/**
	 * Get the computed desired size for this layout, updating the internal cache as required
	 */
	FVector2D ComputeDesiredSize(const FWidgetArgs& InWidgetArgs/*, const float InScale*/, const FTextBlockStyle& InTextStyle);

	/**
	 * Paint this layout, updating the internal cache as required
	 */
	int32 OnPaint(const FPaintArgs& InPaintArgs, const FGeometry& InAllottedGeometry, const FSlateRect& InClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled);

	/**
	 * Force dirty the layout due to an external change that can't be picked up automatically by this cache
	 */
	void DirtyLayout();

	/**
	 * Get the child widgets of this layout
	 */
	FChildren* GetChildren();

	/**
	 * Arrange any child widgets in this layout
	 */
	void ArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const;

private:
	/** Updates the text layout to contain the given text */
	void UpdateTextLayout(const FText& InText);

	/** Update the text highlights */
	void UpdateTextHighlights(const FText& InHighlightText);

	/** Is the style used by the text marshaller up-to-date? */
	bool IsStyleUpToDate(const FTextBlockStyle& NewStyle) const;

	/** Calculate the wrapping width based on the given fixed wrap width, and whether we're auto-wrapping */
	float CalculateWrappingWidth(const FWidgetArgs& InWidgetArgs) const;

	FTextBlockLayout(FTextBlockStyle InDefaultTextStyle, TSharedRef<FBaseTextLayoutMarshaller> InMarshaller, TSharedPtr<IBreakIterator> InLineBreakPolicy);

	/** In control of the layout and wrapping of the text */
	TSharedPtr<FSlateTextLayout> TextLayout;

	/** The marshaller used to get/set the text to/from the text layout. */
	TSharedPtr<FBaseTextLayoutMarshaller> Marshaller;

	/** Used to render the current highlights in the text layout */
	TSharedPtr<ISlateRunRenderer> TextHighlighter;

	/** The last known size of the layout from the previous OnPaint, used to guess at an auto-wrapping width in ComputeDesiredSize */
	FVector2D CachedSize;

	/** The cached desired size - this should be updated using a text layout scale of 1 whenever the text layout becomes dirty */
	FVector2D CachedDesiredSize;

	/** The state of the text the last time it was updated (used to allow updates when the text is changed) */
	FTextSnapshot TextLastUpdate;

	/** The state of the highlight text the last time it was updated (used to allow updates when the text is changed) */
	FTextSnapshot HighlightTextLastUpdate;
};

#endif //WITH_FANCY_TEXT
