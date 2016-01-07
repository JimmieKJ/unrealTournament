// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SFriendsListHeader : public SUserWidget
{
public:

	SLATE_USER_ARGS(SFriendsListHeader) { }
	SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
	SLATE_ARGUMENT(EPopupMethod, Method)
	SLATE_END_ARGS()

	virtual void Construct(const FArguments& InArgs, const TSharedRef<FFriendsFontStyleService>& FontService, const TSharedRef<class FFriendListViewModel>& ViewModel) = 0;

	DECLARE_EVENT_OneParam(SFriendsListContainer, FOnFriendsListVisibilityChanged, EFriendsDisplayLists::Type)
	virtual FOnFriendsListVisibilityChanged& OnVisibilityChanged() = 0;

	virtual void SetHighlighted(bool Highlighted) = 0;
};
