// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ChatTextLayoutMarshaller.h"

class FChatHyperlinkRun : public FSlateHyperlinkRun
{
public:

	static TSharedRef< FChatHyperlinkRun > Create( const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FHyperlinkStyle& InStyle, FOnClick NavigateDelegate, FOnGenerateTooltip InTooltipDelegate, FOnGetTooltipText InTooltipTextDelegate, const FTextRange& InRange, const TSharedRef<FChatItemTransparency>& InChatTransparency );

public:

	virtual ~FChatHyperlinkRun() {}

	virtual int32 OnPaint( const FPaintArgs& Args, const FTextLayout::FLineView& Line, const TSharedRef< ILayoutBlock >& Block, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;


private:

	FChatHyperlinkRun( const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FHyperlinkStyle& InStyle, FOnClick InNavigateDelegate, FOnGenerateTooltip InTooltipDelegate, FOnGetTooltipText InTooltipTextDelegate, const FTextRange& InRange, const TSharedRef<FChatItemTransparency>& InChatTransparency );
	TSharedRef<FChatItemTransparency> ChatTransparency;
};
