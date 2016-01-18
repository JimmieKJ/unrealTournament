// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "BlockedPlayerList.h"
#include "IFriendItem.h"
#include "FriendViewModel.h"
#include "FriendsNavigationService.h"
#include "FriendsService.h"

class FBlockedPlayerListImpl
	: public FBlockedPlayerList
{
public:

	virtual int32 GetFriendList(TArray< TSharedPtr<FFriendViewModel> >& OutFriendsList) override
	{
		const int32 MaxResults = 10;
		int32 iResult = 0;
		TArray< TSharedPtr< IFriendItem > > FriendItemList = FriendsService.Pin()->GetBlockedPlayerList();
		FriendItemList.Sort(FCompareGroupByLastSeen());
		for(const auto& FriendItem : FriendItemList)
		{
			iResult++;
			OutFriendsList.Add(FriendViewModelFactory->Create(FriendItem.ToSharedRef()));
			if (iResult == MaxResults)
			{
				break;
			}
		}
		return 0;
	}

	DECLARE_DERIVED_EVENT(FBlockedPlayerList, IFriendList::FFriendsListUpdated, FFriendsListUpdated);
	virtual FFriendsListUpdated& OnFriendsListUpdated() override
	{
		return FriendsListUpdatedEvent;
	}

public:
	FFriendsListUpdated FriendsListUpdatedEvent;

private:

	void Initialize()
	{
		FriendsService.Pin()->OnFriendsListUpdated().AddSP(this, &FBlockedPlayerListImpl::HandleBlockedPlayerListUpdated);
	}

	void HandleBlockedPlayerListUpdated()
	{
		OnFriendsListUpdated().Broadcast();
	}

	FBlockedPlayerListImpl(
	const TSharedRef<IFriendViewModelFactory>& InFriendViewModelFactory,
	const TSharedRef<FFriendsService>& InFriendsService)
		:FriendViewModelFactory(InFriendViewModelFactory)
		, FriendsService(InFriendsService)
	{
	}

private:
	TSharedRef<IFriendViewModelFactory> FriendViewModelFactory;
	TWeakPtr<FFriendsService> FriendsService;
	friend FBlockedPlayerListFactory;
};

TSharedRef< FBlockedPlayerList > FBlockedPlayerListFactory::Create(
	const TSharedRef<IFriendViewModelFactory>& FriendViewModelFactory,
	const TSharedRef<FFriendsService>& FriendsService)
{
	TSharedRef< FBlockedPlayerListImpl > ChatList(new FBlockedPlayerListImpl(FriendViewModelFactory, FriendsService));
	ChatList->Initialize();
	return ChatList;
}