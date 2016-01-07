// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IUserInfo.h"

class FFriendViewModel
	: public TSharedFromThis<FFriendViewModel>
	, public IUserInfo
{
public:
	virtual ~FFriendViewModel() {}
	virtual void EnumerateActions(TArray<EFriendActionType::Type>& Actions, bool bFromChat = false, bool DisplayChatOption = true) = 0;
	virtual const bool HasChatAction() const = 0;
	virtual void PerformAction(const EFriendActionType::Type ActionType) = 0;
	virtual void SetPendingAction(EFriendActionType::Type PendingAction) = 0;
	virtual EFriendActionType::Type GetPendingAction() = 0;
	virtual FReply ConfirmAction() = 0;
	virtual FReply CancelAction() = 0;
	virtual bool CanPerformAction(const EFriendActionType::Type ActionType) = 0;
	virtual FText GetJoinGameDisallowReason() const = 0;
	virtual FText GetFriendLocation() const = 0;
	virtual bool IsOnline() const = 0;
	virtual bool IsInGameSession() const = 0;
	virtual TSharedRef<class IFriendItem> GetFriendItem() const = 0;
	virtual const FString GetNameNoSpaces() const = 0;
	virtual bool IsLocalPlayerInActiveParty() const = 0;
	virtual bool IsInPartyChat() const = 0;
	virtual const FString GetAlternateName() const = 0;
	virtual bool DisplayPresenceScroller() const = 0;
	virtual bool DisplayPCIcon() const = 0;
};

/**
 * Creates the implementation for an FriendViewModel.
 *
 * @return the newly created FriendViewModel implementation.
 */
IFACTORY(TSharedRef< FFriendViewModel >, IFriendViewModel,
	const TSharedRef<class IFriendItem>& FriendItem);

FACTORY(TSharedRef< IFriendViewModelFactory >, FFriendViewModelFactory,
	const TSharedRef<class FFriendsNavigationService>& NavigationService,
	const TSharedRef<class FFriendsService>& InFriendsService,
	const TSharedRef<class FGameAndPartyService>& InGamePartyService,
	const TSharedRef<class FWebImageService>& InWebImageService);