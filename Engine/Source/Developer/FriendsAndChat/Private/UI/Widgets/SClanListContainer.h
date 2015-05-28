// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace EClanDisplayLists
{
	enum Type : uint8;
}

class SClanListContainer : public SUserWidget
{
public:

	SLATE_USER_ARGS(SClanListContainer) { }
	SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
	SLATE_ARGUMENT(EPopupMethod, Method)
	SLATE_ARGUMENT(EClanDisplayLists::Type, ClanListType)
	SLATE_END_ARGS()

	virtual void Construct(const FArguments& InArgs, const TSharedRef<class FClanViewModel>& ViewModel) = 0;
};
