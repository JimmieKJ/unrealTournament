// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class SFriendsToolTip : public SToolTip
{
public:
	SLATE_BEGIN_ARGS(SFriendsToolTip)
	: _DisplayText(FText::GetEmpty())
	{ }
	/** The style to use */
	SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
	/** Style of the tool tip */
	SLATE_ARGUMENT(FText, DisplayText)
	SLATE_END_ARGS()

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 */
	virtual void Construct(const FArguments& InArgs);

	/** This tooltip needs to use deferred painting because otherwise it gets drawn under the menu popup to which it belongs (which also uses deferred painting) */
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	private:
	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;
};
