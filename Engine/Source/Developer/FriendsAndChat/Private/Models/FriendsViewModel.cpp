// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "FriendsViewModel.h"
#include "FriendViewModel.h"
#include "FriendsStatusViewModel.h"
#include "FriendsUserSettingsViewModel.h"
#include "FriendListViewModel.h"

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
		return FFriendListViewModelFactory::Create(SharedThis(this), ListType);
	}

	virtual void RequestFriend(const FText& FriendName) const override
	{
		if (FriendsAndChatManager.IsValid())
		{
			FriendsAndChatManager.Pin()->RequestFriend(FriendName);
		}
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
		const TSharedRef<FFriendsAndChatManager>& FriendsAndChatManager
		)
		: FriendsAndChatManager(FriendsAndChatManager)
		, bIsPerformingAction(false)
	{
	}

private:
	TWeakPtr<FFriendsAndChatManager> FriendsAndChatManager;
	bool bIsPerformingAction;

private:
	friend FFriendsViewModelFactory;
};

TSharedRef< FFriendsViewModel > FFriendsViewModelFactory::Create(
	const TSharedRef<FFriendsAndChatManager>& FriendsAndChatManager
	)
{
	TSharedRef< FFriendsViewModelImpl > ViewModel(new FFriendsViewModelImpl(FriendsAndChatManager));
	return ViewModel;
}
