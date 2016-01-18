// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SFilterFriendsListMenu : public SUserWidget
{
public:

	SLATE_USER_ARGS(SFilterFriendsListMenu)
	{}
		SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
		SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)
		SLATE_ATTRIBUTE(FSlateColor, BorderColorAndOpacity)
		SLATE_ATTRIBUTE(FSlateColor, FontColorAndOpacity)
	SLATE_END_ARGS()

	virtual void Construct(const FArguments& InArgs, const TSharedRef<class FFriendContainerViewModel>& FriendsViewModel){}

	virtual void Initialize() = 0;

	DECLARE_EVENT(SFilterFriendsListMenu, FOnCloseEvent);
	virtual FOnCloseEvent& OnCloseEvent() = 0;

	DECLARE_EVENT_OneParam(SFilterFriendsListMenu, FOnFilterEvent, const FText&);
	virtual FOnFilterEvent& OnFilterEvent() = 0;
};