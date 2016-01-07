// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "DefaultFriendList.h"
#include "IFriendItem.h"
#include "FriendViewModel.h"
#include "FriendsService.h"
#include "ChatSettingsService.h"

class FDefaultFriendListImpl
	: public FDefaultFriendList
{
public:

	virtual int32 GetFriendList(TArray< TSharedPtr<FFriendViewModel> >& OutFriendsList) override
	{
		TArray< TSharedPtr< IFriendItem > > FriendItemList;
		FriendsService.Pin()->GetFilteredFriendsList(FriendItemList);
		TArray< TSharedPtr< IFriendItem > > OnlineFriendsList;
		TArray< TSharedPtr< IFriendItem > > OfflineFriendsList;

		int32 OnlineCount = 0;
		for( const auto& FriendItem : FriendItemList)
		{
			switch (ListType)
			{
				case EFriendsDisplayLists::DefaultDisplay :
				{
					if (IsFilterSet() && FriendItem->GetOnlineUser().IsValid())
					{
						if(!FriendItem->GetOnlineUser()->GetDisplayName().Contains(FilterText.ToString()))
						{ 
							// Skip filtered entry
							continue;
						}
					}

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
			OutFriendsList.Add(FriendViewModelFactory->Create(FriendItem.ToSharedRef()));
		}

		TSharedPtr<FChatSettingsService> PinnedSettingsService = SettingsService.Pin();

		if (ListType != EFriendsDisplayLists::DefaultDisplay || 
			(PinnedSettingsService.IsValid() && !PinnedSettingsService->GetFlagOption(EChatSettingsType::HideOffline)))
		{
			for (const auto& FriendItem : OfflineFriendsList)
			{
				OutFriendsList.Add(FriendViewModelFactory->Create(FriendItem.ToSharedRef()));
			}
		}

		return OnlineCount;
	}

	virtual bool IsFilterSet() override
	{
		return !FilterText.IsEmpty();
	}

	virtual void SetListFilter(const FText& InFilterText) override
	{
		FilterText = InFilterText;
		OnFriendsListUpdated().Broadcast();
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
		FriendsService.Pin()->OnFriendsListUpdated().AddSP(this, &FDefaultFriendListImpl::HandleChatListUpdated);

		TSharedPtr<FChatSettingsService> PinnedSettingsService = SettingsService.Pin();
		if (PinnedSettingsService.IsValid())
		{
			PinnedSettingsService->OnChatSettingStateUpdated().AddSP(this, &FDefaultFriendListImpl::SettingUpdated);
		}
	}

	void SettingUpdated(EChatSettingsType::Type Setting, bool NewState)
	{
		if (Setting == EChatSettingsType::HideOffline)
		{
			OnFriendsListUpdated().Broadcast();
		}
	}

	void HandleChatListUpdated()
	{
		OnFriendsListUpdated().Broadcast();
	}

	FDefaultFriendListImpl(
		EFriendsDisplayLists::Type InListType,
		const TSharedRef<IFriendViewModelFactory>& InFriendViewModelFactory,
		const TSharedRef<FFriendsService>& InFriendsService,
		const TSharedRef<FChatSettingsService> & InSettingsService)
		: ListType(InListType)
		, FriendViewModelFactory(InFriendViewModelFactory)
		, FriendsService(InFriendsService)
		, SettingsService(InSettingsService)
	{}

private:
	const EFriendsDisplayLists::Type ListType;
	TSharedRef<IFriendViewModelFactory> FriendViewModelFactory;
	TWeakPtr<FFriendsService> FriendsService;
	TWeakPtr<FChatSettingsService> SettingsService;
	FText FilterText;

	friend FDefaultFriendListFactory;
};

TSharedRef< FDefaultFriendList > FDefaultFriendListFactory::Create(
	EFriendsDisplayLists::Type ListType,
	const TSharedRef<IFriendViewModelFactory>& FriendViewModelFactory,
	const TSharedRef<FFriendsService>& FriendsService,
	const TSharedRef<FChatSettingsService> & SettingsService)
{
	TSharedRef< FDefaultFriendListImpl > ChatList(new FDefaultFriendListImpl(ListType, FriendViewModelFactory, FriendsService, SettingsService));
	ChatList->Initialize();
	return ChatList;
}