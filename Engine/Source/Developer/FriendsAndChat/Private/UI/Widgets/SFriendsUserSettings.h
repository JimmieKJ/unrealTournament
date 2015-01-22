// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SFriendsUserSettings : public SUserWidget
{
public:

	SLATE_USER_ARGS(SFriendsUserSettings) { }
	SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
	SLATE_ARGUMENT(EPopupMethod, Method)
	SLATE_END_ARGS()

	virtual void Construct(const FArguments& InArgs, const TSharedRef<class FFriendsUserSettingsViewModel>& ViewModel) = 0;
};
