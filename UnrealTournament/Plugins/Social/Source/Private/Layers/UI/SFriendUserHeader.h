// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SFriendUserHeader : public SUserWidget
{
public:
	SLATE_USER_ARGS(SFriendUserHeader)
		: _ShowUserName(false)
		, _ShowPresenceBrush(true)
	{ }
	SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
	SLATE_ARGUMENT(const FFriendsFontStyle*, FontStyle)
	SLATE_ARGUMENT(bool, ShowUserName)
	SLATE_ARGUMENT(bool, ShowPresenceBrush)
	SLATE_END_ARGS()

	/**
	* Constructs the widget.
	*
	* @param InArgs The Slate argument list.
	*/
	virtual void Construct(const FArguments& InArgs, const TSharedRef<const class IUserInfo>& ViewModel) = 0;
};
