// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "DefaultFriendList.h"
#include "IFriendItem.h"
#include "FriendViewModel.h"

class FDefaultFriendListImpl
	: public FDefaultFriendList
{
public:

	virtual int32 GetFriendList(TArray< TSharedPtr<FFriendViewModel> >& OutFriendsList) override
	{
		TArray< TSharedPtr< IFriendItem > > FriendItemList;
		FFriendsAndChatManager::Get()->GetFilteredFriendsList(FriendItemList);
		TArray< TSharedPtr< IFriendItem > > OnlineFriendsList;
		TArray< TSharedPtr< IFriendItem > > OfflineFriendsList;

		int32 OnlineCount = 0;
		for( const auto& FriendItem : FriendItemList)
		{
			switch (ListType)
			{
				case EFriendsDisplayLists::DefaultDisplay :
				{
					if(FriendItem->GetInviteStatus() == EInviteStatus::Accepted)
					{
						if(FriendItem->IsOnline())
						{
							OnlineFriendsList.Add(FriendItem);
							OnlineCount++;
						}
						else
						{
							OfflineFriendsList.Add(FriendItem);
						}
					}
				}
				break;
				case EFriendsDisplayLists::FriendRequestsDisplay :
				{
					if( FriendItem->GetInviteStatus() == EInviteStatus::PendingInbound)
					{
						OfflineFriendsList.Add(FriendItem.ToSharedRef());
					}
				}
				break;
				case EFriendsDisplayLists::OutgoingFriendInvitesDisplay :
				{
					if( FriendItem->GetInviteStatus() == EInviteStatus::PendingOutbound)
					{
						OfflineFriendsList.Add(FriendItem.ToSharedRef());
					}
				}
				break;
			}
		}

		OnlineFriendsList.Sort(FCompareGroupByName());
		OfflineFriendsList.Sort(FCompareGroupByName());

		for(const auto& FriendItem : OnlineFriendsList)
		{
			OutFriendsList.Add(FFriendViewModelFactory::Create(FriendItem.ToSharedRef()));
		}
		for(const auto& FriendItem : OfflineFriendsList)
		{
			OutFriendsList.Add(FFriendViewModelFactory::Create(FriendItem.ToSharedRef()));
		}

		return OnlineCount;
	}

	DECLARE_DERIVED_EVENT(FDefaultFriendList, IFriendList::FFriendsListUpdated, FFriendsListUpdated);
	virtual FFriendsListUpdated& OnFriendsListUpdated() override
	{
		return FriendsListUpdatedEvent;
	}

public:
	FFriendsListUpdated FriendsListUpdatedEvent;

private:
	void Initialize()
	{
		FFriendsAndChatManager::Get()->OnFriendsListUpdated().AddSP(this, &FDefaultFriendListImpl::HandleChatListUpdated);
	}

	void HandleChatListUpdated()
	{
		OnFriendsListUpdated().Broadcast();
	}

	FDefaultFriendListImpl(EFriendsDisplayLists::Type InListType)
		: ListType(InListType)
	{}

private:
	const EFriendsDisplayLists::Type ListType;

	friend FDefaultFriendListFactory;
};

TSharedRef< FDefaultFriendList > FDefaultFriendListFactory::Create(
	EFriendsDisplayLists::Type ListType)
{
	TSharedRef< FDefaultFriendListImpl > ChatList(new FDefaultFriendListImpl(ListType));
	ChatList->Initialize();
	return ChatList;
}