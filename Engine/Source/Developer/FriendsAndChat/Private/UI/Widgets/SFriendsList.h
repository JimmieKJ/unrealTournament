// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FriendViewModel.h"

class SFriendsList : public SUserWidget
{
public:

	SLATE_USER_ARGS(SFriendsList)
	{ }
	SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
	SLATE_ARGUMENT(EPopupMethod, Method)
	SLATE_END_ARGS()

	/**
	 * Constructs the Friends list widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param ViewModel The widget view model.
	 */
	virtual void Construct(const FArguments& InArgs, const TSharedRef<class FFriendListViewModel>& ViewModel) = 0;

	static const FButtonStyle* GetActionButtonStyle(const FFriendsAndChatStyle& FriendStyle, EFriendActionLevel ActionLevel);
};
