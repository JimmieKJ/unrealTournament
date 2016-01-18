// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFriendContainerViewModel;

class SFriendsContainer : public SUserWidget
{
public:

	SLATE_USER_ARGS(SFriendsContainer)
		: _Method(EPopupMethod::UseCurrentWindow)
	 { }
	SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
	SLATE_ARGUMENT(EPopupMethod, Method)
	SLATE_END_ARGS()

	virtual void Construct(const FArguments& InArgs, const TSharedRef<FFriendsFontStyleService> FontService, const TSharedRef<FChatSettingsService>& SettingsService, const TSharedRef<FFriendContainerViewModel>& ViewModel) = 0;
};
