// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "RecentPlayerList.h"
#include "IFriendItem.h"
#include "FriendViewModel.h"

class FRecentPlayerListImpl
	: public FRecentPlayerList
{
public:

	virtual int32 GetFriendList(TArray< TSharedPtr<FFriendViewModel> >& OutFriendsList) override
	{
		TArray< TSharedPtr< IFriendItem > > FriendItemList = FFriendsAndChatManager::Get()->GetRecentPlayerList();
		FriendItemList.Sort(FCompareGroupByName());
		for(const auto& FriendItem : FriendItemList)
		{
			OutFriendsList.Add(FFriendViewModelFactory::Create(FriendItem.ToSharedRef()));
		}
		return 0;
	}

	DECLARE_DERIVED_EVENT(FRecentPlayerList, IFriendList::FFriendsListUpdated, FFriendsListUpdated);
	virtual FFriendsListUpdated& OnFriendsListUpdated() override
	{
		return FriendsListUpdatedEvent;
	}

public:
	FFriendsListUpdated FriendsListUpdatedEvent;

private:
	void Initialize()
	{
		FFriendsAndChatManager::Get()->OnFriendsListUpdated().AddSP(this, &FRecentPlayerListImpl::HandleChatListUpdated);
	}

	void HandleChatListUpdated()
	{
		OnFriendsListUpdated().Broadcast();
	}

private:
	friend FRecentPlayerListFactory;
};

TSharedRef< FRecentPlayerList > FRecentPlayerListFactory::Create()
{
	TSharedRef< FRecentPlayerListImpl > ChatList(new FRecentPlayerListImpl());
	ChatList->Initialize();
	return ChatList;
}