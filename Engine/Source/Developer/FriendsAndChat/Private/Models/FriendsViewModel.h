// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFriendsViewModel
	: public TSharedFromThis<FFriendsViewModel>
{
public:
	virtual ~FFriendsViewModel() {}
	virtual bool IsPerformingAction() const = 0;
	virtual void PerformAction() = 0;
	virtual TSharedRef< class FFriendsUserViewModel > GetUserViewModel() = 0;
	virtual TSharedRef< class FFriendsStatusViewModel > GetStatusViewModel() = 0;
	virtual TSharedRef< class FFriendsUserSettingsViewModel > GetUserSettingsViewModel() = 0;
	virtual TSharedRef< class FFriendListViewModel > GetFriendListViewModel(EFriendsDisplayLists::Type ListType) = 0;
	virtual TSharedRef< class FClanViewModel > GetClanViewModel() = 0;
	virtual void RequestFriend(const FText& FriendName) const = 0;
	virtual EVisibility GetGlobalChatButtonVisibility() const = 0;
	virtual void DisplayGlobalChatWindow() const = 0;
	virtual const FString GetName() const = 0;
};

/**
 * Creates the implementation for an FriendsViewModel.
 *
 * @return the newly created FriendsViewModel implementation.
 */
FACTORY(TSharedRef< FFriendsViewModel >, FFriendsViewModel,
	const TSharedRef<class FFriendsAndChatManager>& FFriendsAndChatManager,
	const TSharedRef<class IClanRepository>& ClanRepository,
	const TSharedRef<class IFriendListFactory>& FriendsListFactory);