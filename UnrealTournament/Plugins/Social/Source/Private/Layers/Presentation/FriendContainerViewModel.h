// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFriendContainerViewModel
	: public TSharedFromThis<FFriendContainerViewModel>
{
public:
	virtual ~FFriendContainerViewModel() {}
	virtual TSharedRef< class FFriendsUserViewModel > GetUserViewModel() = 0;
	virtual TSharedRef< class FFriendsStatusViewModel > GetStatusViewModel() = 0;
	virtual TSharedRef< class FFriendListViewModel > GetFriendListViewModel(EFriendsDisplayLists::Type ListType) = 0;
	virtual TSharedRef< class FClanCollectionViewModel > GetClanCollectionViewModel() = 0;
	virtual void RequestFriend(const FText& FriendName) const = 0;
	virtual EVisibility GetGlobalChatButtonVisibility() const = 0;
	virtual void DisplayGlobalChatWindow() const = 0;
	virtual const FString GetName() const = 0;
	virtual void OpenConfirmDialog(FText Description, TSharedPtr<class FFriendViewModel> FriendViewModel) = 0;
	virtual FText GetConfirmDescription() const = 0;
	virtual EVisibility GetConfirmVisibility() const = 0;
	virtual void OnConfirm() = 0;
	virtual void OnCancel() = 0;
};

/**
 * Creates the implementation for an FriendsViewModel.
 *
 * @return the newly created FriendsViewModel implementation.
 */
FACTORY(TSharedRef< FFriendContainerViewModel >, FFriendContainerViewModel,
	const TSharedRef<class FFriendsService>& FriendsService,
	const TSharedRef<class FGameAndPartyService>& GameAndPartyService,
	const TSharedRef<class FMessageService>& MessageService,
	const TSharedRef<class IClanRepository>& ClanRepository,
	const TSharedRef<class IFriendListFactory>& FriendsListFactory,
	const TSharedRef<class FFriendsNavigationService>& NavigationService,
	const TSharedRef<class FWebImageService>& WebImageService,	const TSharedRef<class FChatSettingsService>& ChatSettingService
	);