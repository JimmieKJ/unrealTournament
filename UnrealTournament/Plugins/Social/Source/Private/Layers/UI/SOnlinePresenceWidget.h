// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFriendsViewModel;

class SOnlinePresenceWidget : public SUserWidget
{
public:

	SLATE_USER_ARGS(SOnlinePresenceWidget)
	{}
		SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
			SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)
	SLATE_END_ARGS()

	virtual void Construct(const FArguments& InArgs, const TSharedRef<class FFriendContainerViewModel>& ViewModel){}

	virtual void SetColorAndOpacity(const TAttribute<FSlateColor>& InColorAndOpacity) = 0;

};