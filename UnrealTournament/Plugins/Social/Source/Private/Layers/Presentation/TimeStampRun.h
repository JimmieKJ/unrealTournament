// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "ISlateRun.h"
#include "ChatTextLayoutMarshaller.h"

class FTimeStampRun : public ISlateRun, public TSharedFromThis< FTimeStampRun >
{
public:

	struct FTimeStampRunInfo
	{
		FDateTime TimeStamp;
		int16 Baseline;
		FTextBlockStyle TextBoxStyle;
		TOptional< FVector2D > Size;

		FTimeStampRunInfo(FDateTime InTimeStamp, int16 InBaseline, const FTextBlockStyle& InTextBoxStyle, TOptional< FVector2D > InSize = TOptional< FVector2D >())
			: TimeStamp(InTimeStamp)
			, Baseline(InBaseline)
			, TextBoxStyle(InTextBoxStyle)
			, Size(InSize)
		{

		}
	};

	static TSharedRef< FTimeStampRun > Create(const TSharedRef<class FTextLayout>& TextLayout, const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FTimeStampRunInfo& InWidgetInfo, const TSharedRef<FChatItemTransparency>& InChatFadeItem);

	virtual ~FTimeStampRun();

	virtual FTextRange GetTextRange() const override;
	virtual void SetTextRange( const FTextRange& Value ) override;
	virtual int16 GetBaseLine( float Scale ) const override;
	virtual int16 GetMaxHeight( float Scale ) const override;
	virtual FVector2D Measure(int32 StartIndex, int32 EndIndex, float Scale, const FRunTextContext& TextContext) const override;
	virtual int8 GetKerning(int32 CurrentIndex, float Scale, const FRunTextContext& TextContext) const override;
	virtual TSharedRef< ILayoutBlock > CreateBlock(int32 StartIndex, int32 EndIndex, FVector2D Size, const FLayoutBlockTextContext& TextContext, const TSharedPtr< IRunRenderer >& Renderer) override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FTextLayout::FLineView& Line, const TSharedRef< ILayoutBlock >& Block, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual const TArray< TSharedRef<SWidget> >& GetChildren() override;
	virtual void ArrangeChildren( const TSharedRef< ILayoutBlock >& Block, const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual int32 GetTextIndexAt( const TSharedRef< ILayoutBlock >& Block, const FVector2D& Location, float Scale, ETextHitPoint* const OutHitPoint = nullptr ) const override;
	virtual FVector2D GetLocationAt( const TSharedRef< ILayoutBlock >& Block, int32 Offset, float Scale ) const override;
	virtual void BeginLayout() override {}
	virtual void EndLayout() override {}
	virtual void Move(const TSharedRef<FString>& NewText, const FTextRange& NewRange) override;
	virtual TSharedRef<IRun> Clone() const override;
	virtual void AppendTextTo(FString& Text) const override;
	virtual void AppendTextTo(FString& AppendToText, const FTextRange& PartialRange) const override;
	virtual const FRunInfo& GetRunInfo() const override;
	virtual ERunAttributes GetRunAttributes() const override;

private:

	void Initialize();

	FText GetMessageTimeText() const
	{
		return MessageTimeAsText;
	}

	bool UpdateTimeStamp(float Delay);

	FTimeStampRun(const TSharedRef<class FTextLayout>& TextLayout, const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FTimeStampRunInfo& InWidgetInfo, const TSharedRef<FChatItemTransparency>& InChatFadeItem);

private:

	TWeakPtr<class FTextLayout> TextLayout;
	FRunInfo RunInfo;
	TSharedRef< const FString > Text;
	FTextRange Range;
	FTimeStampRunInfo Info;
	TArray< TSharedRef<SWidget> > Children;
	TSharedPtr<STextBlock> TimeStampWidget;
	FText MessageTimeAsText;
	FDelegateHandle TickerHandle;
	FDateTime TimeStamp;
	TSharedRef<FChatItemTransparency> ChatTransparency;
	mutable bool ForceRedraw;
	mutable FVector2D WidgetSize;

};
