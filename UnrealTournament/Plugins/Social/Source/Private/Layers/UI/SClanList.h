// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class SClanList : public SUserWidget
{
public:

	SLATE_USER_ARGS(SClanList)
	{ }
	SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
	SLATE_ARGUMENT(EPopupMethod, Method)
	SLATE_END_ARGS()

	/**
	 * Constructs the Clans list widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param ViewModel The widget view model.
	 */
	virtual void Construct(const FArguments& InArgs, const TSharedRef<class FClanListViewModel>& ViewModel) = 0;

};
