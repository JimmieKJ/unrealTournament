// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SAddFriendMenu : public SUserWidget
{
public:

	SLATE_USER_ARGS(SAddFriendMenu)
	{}
		SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
		SLATE_ATTRIBUTE(float, Opacity)
	SLATE_END_ARGS()

	virtual void Construct(const FArguments& InArgs, const TSharedRef<class FFriendContainerViewModel>& FriendsViewModel, const TSharedRef<FFriendsFontStyleService> InFontService) = 0;

	virtual void Initialize() = 0;

	DECLARE_EVENT(SAddFriendMenu, FOnCloseEvent);
	virtual FOnCloseEvent& OnCloseEvent() = 0;
};