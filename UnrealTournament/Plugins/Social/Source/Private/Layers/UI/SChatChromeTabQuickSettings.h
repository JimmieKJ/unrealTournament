// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SChatChromeTabQuickSettings : public SUserWidget
{
public:
	SLATE_USER_ARGS(SChatChromeTabQuickSettings)
	{ }
	SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
	SLATE_ARGUMENT(EChatMessageType::Type, ChatType)
	SLATE_END_ARGS()

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 */
	 virtual void Construct(const FArguments& InArgs, const TSharedRef<class FChatChromeTabViewModel>& InChromeTabViewModel) = 0;

};
