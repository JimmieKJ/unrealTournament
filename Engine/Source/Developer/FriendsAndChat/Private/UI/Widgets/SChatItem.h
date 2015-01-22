// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SChatItem : public SUserWidget
{
public:
	SLATE_USER_ARGS(SChatItem)
	{ }
	SLATE_ARGUMENT( const FFriendsAndChatStyle*, FriendStyle )
	SLATE_ARGUMENT(EPopupMethod, Method)
	SLATE_END_ARGS()

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 */
	virtual void Construct(const FArguments& InArgs, const TSharedRef<class FChatItemViewModel>& ViewModel, const TSharedRef<class FChatViewModel>& OwnerViewModel) = 0;
};
