// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "GameInviteList.h"
#include "IFriendItem.h"
#include "FriendViewModel.h"
#include "GameAndPartyService.h"

class FGameInviteListImpl
	: public FGameInviteList
{
public:

	virtual int32 GetFriendList(TArray< TSharedPtr<FFriendViewModel> >& OutFriendsList) override
	{
		TArray< TSharedPtr< IFriendItem > > FriendItemList;
		GamePartyInviteService.Pin()->GetFilteredGameInviteList(FriendItemList);

		FriendItemList.Sort(FCompareGroupByName());
		for(const auto& FriendItem : FriendItemList)
		{
			OutFriendsList.Add(FriendViewModelFactory->Create(FriendItem.ToSharedRef()));
		}

		return 0;
	}

	DECLARE_DERIVED_EVENT(FGameInviteList, IFriendList::FFriendsListUpdated, FFriendsListUpdated);
	virtual FFriendsListUpdated& OnFriendsListUpdated() override
	{
		return FriendsListUpdatedEvent;
	}

public:
	FFriendsListUpdated FriendsListUpdatedEvent;

private:
	void Initialize()
	{
		GamePartyInviteService.Pin()->OnGameInvitesUpdated().AddSP(this, &FGameInviteListImpl::HandleChatListUpdated);
	}

	void HandleChatListUpdated()
	{
		OnFriendsListUpdated().Broadcast();
	}

	FGameInviteListImpl(const TSharedRef<IFriendViewModelFactory>& InFriendViewModelFactory,
		const TSharedRef<FGameAndPartyService>& InGamePartyInviteService)
		:FriendViewModelFactory(InFriendViewModelFactory)
		, GamePartyInviteService(InGamePartyInviteService)
	{}

private:
	TSharedRef<IFriendViewModelFactory> FriendViewModelFactory;
	TWeakPtr<FGameAndPartyService> GamePartyInviteService;
	friend FGameInviteListFactory;
};

TSharedRef< FGameInviteList > FGameInviteListFactory::Create(
	const TSharedRef<IFriendViewModelFactory>& FriendViewModelFactory,
	const TSharedRef<FGameAndPartyService>& GamePartyInviteService)
{
	TSharedRef< FGameInviteListImpl > ChatList(new FGameInviteListImpl(FriendViewModelFactory, GamePartyInviteService));
	ChatList->Initialize();
	return ChatList;
}