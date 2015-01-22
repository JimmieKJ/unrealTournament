// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_FANCY_TEXT

class SLATE_API FSlateTextLayout : public FTextLayout
{
public:

	static TSharedRef< FSlateTextLayout > Create(FTextBlockStyle InDefaultTextStyle);

	FChildren* GetChildren();

	void ArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const;

	int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const;

	//void AddLine( const TSharedRef< FString >& Text, const FTextBlockStyle& TextStyle )
	//{
	//	FLineModel LineModel( Text );
	//	LineModel.Runs.Add( FLineModel::FRun( FSlateTextRun::Create( LineModel.Text, TextStyle ) ) );
	//	LineModels.Add( LineModel );
	//}

	virtual void EndLayout() override;

	void SetDefaultTextStyle(FTextBlockStyle InDefaultTextStyle);
	const FTextBlockStyle& GetDefaultTextStyle() const;


protected:

	FSlateTextLayout(FTextBlockStyle InDefaultTextStyle);

	int32 OnPaintHighlights(const FPaintArgs& Args, const FTextLayout::FLineView& LineView, const TArray<FLineViewHighlight>& Highlights, const FTextBlockStyle& DefaultTextStyle, const FGeometry& AllottedGeometry, const FSlateRect& ClippingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;

	void AggregateChildren();

	virtual TSharedRef<IRun> CreateDefaultTextRun(const TSharedRef<FString>& NewText, const FTextRange& NewRange) const override;

private:

	TSlotlessChildren< SWidget > Children;

	/** Default style used by the TextLayout */
	FTextBlockStyle DefaultTextStyle;

	friend class FSlateTextLayoutFactory;
};

#endif //WITH_FANCY_TEXT