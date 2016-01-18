// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFriendViewModel;

class SFriendItemToolTip : public SUserWidget
{
public:
	SLATE_USER_ARGS(SFriendItemToolTip)
	{}
		SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
	SLATE_END_ARGS()

	virtual void Construct(const FArguments& InArgs, const TSharedRef<FFriendsFontStyleService> InFontService, const TSharedRef<FFriendViewModel>& InFriendViewModel){}
	virtual void PlayIntroAnim() = 0;
};