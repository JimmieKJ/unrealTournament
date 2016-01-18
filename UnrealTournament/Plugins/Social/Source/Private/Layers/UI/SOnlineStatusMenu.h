// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFriendsViewModel;

class SOnlineStatusMenu : public SUserWidget
{
public:

	SLATE_USER_ARGS(SOnlineStatusMenu)
	{}
		SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
			SLATE_ATTRIBUTE(float, Opacity)
		SLATE_END_ARGS()

	virtual void Construct(const FArguments& InArgs, const TSharedRef<class FFriendContainerViewModel>& FriendsViewModel, const TSharedRef<FFriendsFontStyleService> InFontService){}

	DECLARE_EVENT(SOnlineStatusMenu, FOnCloseEvent);
	virtual FOnCloseEvent& OnCloseEvent() = 0;


};