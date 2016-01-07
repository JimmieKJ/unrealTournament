// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "ChatHyperlinkRun.h"
#include "SRichTextHyperlink.h"

TSharedRef< FChatHyperlinkRun > FChatHyperlinkRun::Create( const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FHyperlinkStyle& InStyle, FOnClick NavigateDelegate, FOnGenerateTooltip InTooltipDelegate, FOnGetTooltipText InTooltipTextDelegate, const FTextRange& InRange, const TSharedRef<FChatItemTransparency>& InChatTransparency )
{
	return MakeShareable( new FChatHyperlinkRun( InRunInfo, InText, InStyle, NavigateDelegate, InTooltipDelegate, InTooltipTextDelegate, InRange, InChatTransparency ) );
}

int32 FChatHyperlinkRun::OnPaint( const FPaintArgs& Args, const FTextLayout::FLineView& Line, const TSharedRef< ILayoutBlock >& Block, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	FWidgetStyle BlendedWidgetStyle = InWidgetStyle;
	BlendedWidgetStyle.BlendColorAndOpacityTint(FLinearColor(1,1,1,ChatTransparency->GetTransparency()));

	return FSlateHyperlinkRun::OnPaint(Args, Line, Block, DefaultStyle, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, BlendedWidgetStyle, bParentEnabled );
}

FChatHyperlinkRun::FChatHyperlinkRun( const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FHyperlinkStyle& InStyle, FOnClick InNavigateDelegate, FOnGenerateTooltip InTooltipDelegate, FOnGetTooltipText InTooltipTextDelegate, const FTextRange& InRange, const TSharedRef<FChatItemTransparency>& InChatTransparency ) 
	: FSlateHyperlinkRun(InRunInfo, InText, InStyle, InNavigateDelegate, InTooltipDelegate, InTooltipTextDelegate,InRange)
	, ChatTransparency(InChatTransparency)
{
}
