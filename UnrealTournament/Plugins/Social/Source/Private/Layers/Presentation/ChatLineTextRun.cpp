// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "ChatLineTextRun.h"

TSharedRef< FChatLineTextRun > FChatLineTextRun::Create(const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FTextBlockStyle& Style, const FTextRange& InRange, const TSharedRef<FChatItemTransparency>& InChatTransparency)
{
	return MakeShareable(new FChatLineTextRun(InRunInfo, InText, Style, InRange, InChatTransparency));
}

int32 FChatLineTextRun::OnPaint( const FPaintArgs& Args, const FTextLayout::FLineView& Line, const TSharedRef< ILayoutBlock >& Block, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	FWidgetStyle BlendedWidgetStyle = InWidgetStyle;
	BlendedWidgetStyle.BlendColorAndOpacityTint(FLinearColor(1,1,1,ChatTransparency->GetTransparency()));

	return FSlateTextRun::OnPaint(Args, Line, Block, DefaultStyle, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, BlendedWidgetStyle,bParentEnabled);
}

FChatLineTextRun::FChatLineTextRun(const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FTextBlockStyle& InStyle, const FTextRange& InRange, const TSharedRef<FChatItemTransparency>& InChatTransparency) 
	: FSlateTextRun(InRunInfo, InText, InStyle, InRange)
	, ChatTransparency(InChatTransparency)
{
}
