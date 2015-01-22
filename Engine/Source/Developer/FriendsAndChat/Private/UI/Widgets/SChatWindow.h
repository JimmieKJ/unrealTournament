// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SChatWindow : public SUserWidget
{
public:
	SLATE_USER_ARGS(SChatWindow)
	: _MaxChatLength(200)
	{ }
		SLATE_ARGUMENT( const FFriendsAndChatStyle*, FriendStyle )
		SLATE_ARGUMENT(EPopupMethod, Method)
		SLATE_ARGUMENT(int32, MaxChatLength)
	SLATE_END_ARGS()

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 */
	virtual void Construct(const FArguments& InArgs, const TSharedRef<class FChatViewModel>& ViewModel) = 0;
};
