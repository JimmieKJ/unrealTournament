// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SuggestedFriendsList.h"
#include "IFriendItem.h"
#include "FriendViewModel.h"
#include "FriendsNavigationService.h"
#include "FriendsService.h"
#include "FriendImportPlayerItem.h"

class FSuggestedFriendsListImpl
	: public FSuggestedFriendsList
{
public:

	virtual int32 GetFriendList(TArray< TSharedPtr<FFriendViewModel> >& OutFriendsList) override
	{
		TArray< TSharedPtr< IFriendItem > > ImportItemList = FriendsService.Pin()->GetExternalPlayerList();

		// Loop through list and create friend view models
		for (const auto& FriendItem : ImportItemList)
		{
			OutFriendsList.Add(FriendViewModelFactory->Create(FriendItem.ToSharedRef()));
		}
		return 0;
	}

	DECLARE_DERIVED_EVENT(FSuggestedFriendsList, IFriendList::FFriendsListUpdated, FFriendsListUpdated);
	virtual FFriendsListUpdated& OnFriendsListUpdated() override
	{
		return FriendsListUpdatedEvent;
	}

public:
	FFriendsListUpdated FriendsListUpdatedEvent;

private:

	void Initialize()
	{
		FriendsService.Pin()->OnFriendsListUpdated().AddSP(this, &FSuggestedFriendsListImpl::HandleImportListUpdated);
	}

	void HandleImportListUpdated()
	{
		OnFriendsListUpdated().Broadcast();
	}

	FSuggestedFriendsListImpl(
	const TSharedRef<IFriendViewModelFactory>& InFriendViewModelFactory,
	const TSharedRef<FFriendsService>& InFriendsService)
		:FriendViewModelFactory(InFriendViewModelFactory)
		, FriendsService(InFriendsService)
	{
	}

private:
	TSharedRef<IFriendViewModelFactory> FriendViewModelFactory;
	TWeakPtr<FFriendsService> FriendsService;
	friend FSuggestedFriendsListFactory;
};

TSharedRef< FSuggestedFriendsList > FSuggestedFriendsListFactory::Create(
	const TSharedRef<IFriendViewModelFactory>& FriendViewModelFactory,
	const TSharedRef<FFriendsService>& FriendsService)
{
	TSharedRef< FSuggestedFriendsListImpl > ChatList(new FSuggestedFriendsListImpl(FriendViewModelFactory, FriendsService));
	ChatList->Initialize();
	return ChatList;
}