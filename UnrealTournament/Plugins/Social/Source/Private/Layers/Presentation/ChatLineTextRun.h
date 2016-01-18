// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateTextRun.h"
#include "ChatTextLayoutMarshaller.h"

class FChatLineTextRun : public FSlateTextRun
{
public:
	static TSharedRef< FChatLineTextRun > Create(const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FTextBlockStyle& Style, const FTextRange& InRange, const TSharedRef<FChatItemTransparency>& InChatTransparency);

	virtual int32 OnPaint( const FPaintArgs& Args, const FTextLayout::FLineView& Line, const TSharedRef< ILayoutBlock >& Block, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

protected:
	FChatLineTextRun(const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FTextBlockStyle& InStyle, const FTextRange& InRange, const TSharedRef<FChatItemTransparency>& InChatTransparency);

private:
	TSharedRef<FChatItemTransparency> ChatTransparency;
};
