// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SChatMarkupTips : public SUserWidget
{
public:
	SLATE_USER_ARGS(SChatMarkupTips)
	{ }
		SLATE_ARGUMENT(const FFriendsMarkupStyle*, MarkupStyle)
	SLATE_END_ARGS()

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 */
	 virtual void Construct(const FArguments& InArgs, const TSharedRef<class FChatTipViewModel>& InViewModel, const TSharedRef<FFriendsFontStyleService> InFontService) = 0;

};
