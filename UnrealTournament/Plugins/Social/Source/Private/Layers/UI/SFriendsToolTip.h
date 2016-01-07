// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class SFriendsToolTip : public SToolTip
{
public:
	SLATE_BEGIN_ARGS(SFriendsToolTip)
	: _DisplayText()
	, _TextColor(FLinearColor::White)
	, _BackgroundImage(FCoreStyle::Get().GetBrush( "Border" ))
	, _Font()
	{ }
	/** Style of the tool tip */
	SLATE_ARGUMENT(FText, DisplayText)
	SLATE_ARGUMENT(FLinearColor, TextColor)
	SLATE_ATTRIBUTE(const FSlateBrush*, BackgroundImage)
	SLATE_ATTRIBUTE(FSlateFontInfo, Font)
	SLATE_END_ARGS()

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 */
	virtual void Construct(const FArguments& InArgs);

	/** This tooltip needs to use deferred painting because otherwise it gets drawn under the menu popup to which it belongs (which also uses deferred painting) */
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};
