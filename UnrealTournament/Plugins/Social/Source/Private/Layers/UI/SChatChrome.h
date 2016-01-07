// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SChatChrome : public SUserWidget
{
public:
	SLATE_USER_ARGS(SChatChrome)
		: _ChatOrientation(EChatOrientationType::COT_Horizontal)
	{ }

	SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
		SLATE_ARGUMENT(EChatOrientationType::Type, ChatOrientation)
	SLATE_END_ARGS()

	virtual void HandleWindowActivated() = 0;

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 */
	virtual void Construct(const FArguments& InArgs, const TSharedRef<class FChatChromeViewModel>& InChromeViewModel, const TSharedRef<FFriendsFontStyleService> InFontService) = 0;

};
