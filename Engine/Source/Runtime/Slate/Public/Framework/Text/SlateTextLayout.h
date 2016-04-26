// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

class SLATE_API FSlateTextLayout : public FTextLayout
{
public:

	static TSharedRef< FSlateTextLayout > Create(FTextBlockStyle InDefaultTextStyle);

	FChildren* GetChildren();

	void ArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const;

	int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const;

	virtual void EndLayout() override;

	virtual void UpdateIfNeeded() override;

	void SetDefaultTextStyle(FTextBlockStyle InDefaultTextStyle);
	const FTextBlockStyle& GetDefaultTextStyle() const;

	void SetIsPassword(const TAttribute<bool>& InIsPassword);

protected:

	FSlateTextLayout(FTextBlockStyle InDefaultTextStyle);

	int32 OnPaintHighlights(const FPaintArgs& Args, const FTextLayout::FLineView& LineView, const TArray<FLineViewHighlight>& Highlights, const FTextBlockStyle& DefaultTextStyle, const FGeometry& AllottedGeometry, const FSlateRect& ClippingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;

	void AggregateChildren();

	virtual TSharedRef<IRun> CreateDefaultTextRun(const TSharedRef<FString>& NewText, const FTextRange& NewRange) const override;

private:

	TSlotlessChildren< SWidget > Children;

	/** Default style used by the TextLayout */
	FTextBlockStyle DefaultTextStyle;

	/** This this layout displaying a password? */
	TAttribute<bool> bIsPassword;

	/** The localized fallback font revision the last time the text layout was updated. Used to force a flush if the font changes. */
	int32 LocalizedFallbackFontRevision;

	friend class FSlateTextLayoutFactory;
};
