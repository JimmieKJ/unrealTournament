// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFriendViewModel;

class SHeaderButtonToolTip : public SUserWidget
{
public:
	SLATE_USER_ARGS(SHeaderButtonToolTip)
	{}
		SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
		SLATE_ARGUMENT(bool, ShowStatus)
		SLATE_ARGUMENT(FText, Tip)
	SLATE_END_ARGS()

	virtual void Construct(const FArguments& InArgs, const TSharedRef<FFriendsFontStyleService> InFontService, const TSharedRef<class FFriendContainerViewModel>& ViewModel){}
	virtual void PlayIntroAnim() = 0;
};