// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SFriendActions : public SUserWidget
{
public:
	SLATE_USER_ARGS(SFriendActions)
		: _FromChat(false)
		, _DisplayChatOption(true)
	{ }
		SLATE_ARGUMENT( const FFriendsAndChatStyle*, FriendStyle )
		SLATE_ARGUMENT( bool, FromChat )
		SLATE_ARGUMENT( bool, DisplayChatOption )
	SLATE_END_ARGS()

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 */
	 virtual void Construct(const FArguments& InArgs, const TSharedRef<class FFriendViewModel>& ViewModel, const TSharedRef<class FFriendsFontStyleService>& InFontService) = 0;
};
