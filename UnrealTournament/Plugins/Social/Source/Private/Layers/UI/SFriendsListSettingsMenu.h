// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

DECLARE_DELEGATE_RetVal(bool, FIsMenuOpen);

class SFriendsListSettingsMenu : public SUserWidget
{
public:

	SLATE_USER_ARGS(SFriendsListSettingsMenu)
	{}
		SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
		SLATE_ATTRIBUTE(float, Opacity)
	SLATE_END_ARGS()

	virtual void Construct(const FArguments& InArgs, const TSharedRef<FChatSettingsService>& SettingsService, const TSharedRef<FFriendsFontStyleService> InFontService) = 0;

	DECLARE_EVENT(SFriendsListSettingsMenu, FOnCloseEvent);
	virtual FOnCloseEvent& OnCloseEvent() = 0;
};