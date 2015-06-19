// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "FriendsViewModel.h"
#include "FriendViewModel.h"
#include "FriendsStatusViewModel.h"
#include "FriendsUserSettingsViewModel.h"
#include "FriendListViewModel.h"
#include "FriendsUserViewModel.h"
#include "ClanViewModel.h"
#include "ClanRepository.h"
#include "IFriendList.h"

class FFriendsViewModelImpl
	: public FFriendsViewModel
{
public:

	virtual bool IsPerformingAction() const override
	{
		return bIsPerformingAction;
	}

	virtual void PerformAction() override
	{
		bIsPerformingAction = !bIsPerformingAction;
	}

	virtual TSharedRef< class FFriendsUserViewModel > GetUserViewModel() override
	{
		return FFriendsUserViewModelFactory::Create(FriendsAndChatManager.Pin().ToSharedRef());
	}

	virtual TSharedRef< FFriendsStatusViewModel > GetStatusViewModel() override
	{
		return FFriendsStatusViewModelFactory::Create(FriendsAndChatManager.Pin().ToSharedRef());
	}

	virtual TSharedRef< FFriendsUserSettingsViewModel > GetUserSettingsViewModel() override
	{
		return FFriendsUserSettingsViewModelFactory::Create(FriendsAndChatManager.Pin().ToSharedRef());
	}

	virtual TSharedRef< FFriendListViewModel > GetFriendListViewModel(EFriendsDisplayLists::Type ListType) override
	{
		return FFriendListViewModelFactory::Create(FriendsListFactory->Create(ListType), ListType);
	}

	virtual TSharedRef< FClanViewModel > GetClanViewModel() override
	{
		return FClanViewModelFactory::Create(ClanRepository);
	}

	virtual void RequestFriend(const FText& FriendName) const override
	{
		if (FriendsAndChatManager.IsValid())
		{
			FriendsAndChatManager.Pin()->RequestFriend(FriendName);
		}
	}

	virtual EVisibility GetGlobalChatButtonVisibility() const override
	{
		if (FriendsAndChatManager.IsValid())
		{
			return FriendsAndChatManager.Pin()->IsInGlobalChat() ? EVisibility::Visible : EVisibility::Collapsed;
		}
		return EVisibility::Collapsed;
	}

	virtual void DisplayGlobalChatWindow() const override
	{
		if (FriendsAndChatManager.IsValid())
		{
			FriendsAndChatManager.Pin()->OpenGlobalChat();
		}
	}

	virtual const FString GetName() const override
	{
		FString Nickname;
		TSharedPtr<FFriendsAndChatManager> ManagerPinned = FriendsAndChatManager.Pin();
		if (ManagerPinned.IsValid())
		{
			Nickname = ManagerPinned->GetUserNickname();
		}
		return Nickname;
	}

	~FFriendsViewModelImpl()
	{
		Uninitialize();
	}

private:

	void Uninitialize()
	{
		if( FriendsAndChatManager.IsValid())
		{
			FriendsAndChatManager.Reset();
		}
	}

	FFriendsViewModelImpl(
		const TSharedRef<FFriendsAndChatManager>& InFriendsAndChatManager,
		const TSharedRef<IClanRepository>& InClanRepository,
		const TSharedRef<IFriendListFactory>& InFriendsListFactory
		)
		: FriendsAndChatManager(InFriendsAndChatManager)
		, ClanRepository(InClanRepository)
		, FriendsListFactory(InFriendsListFactory)
		, bIsPerformingAction(false)
	{
	}

private:
	TWeakPtr<FFriendsAndChatManager> FriendsAndChatManager;
	TSharedRef<IClanRepository> ClanRepository;
	TSharedRef<IFriendListFactory> FriendsListFactory;
	bool bIsPerformingAction;

private:
	friend FFriendsViewModelFactory;
};

TSharedRef< FFriendsViewModel > FFriendsViewModelFactory::Create(
	const TSharedRef<FFriendsAndChatManager>& FriendsAndChatManager,
	const TSharedRef<IClanRepository>& ClanRepository,
	const TSharedRef<IFriendListFactory>& FriendsListFactory
	)
{
	TSharedRef< FFriendsViewModelImpl > ViewModel(new FFriendsViewModelImpl(FriendsAndChatManager, ClanRepository, FriendsListFactory));
	return ViewModel;
}
