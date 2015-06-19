// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FriendViewModel.h"

class FChatItemViewModel;
namespace EChatMessageType
{
	enum Type : uint8;
}

struct FSelectedFriend
{
	TSharedPtr<FUniqueNetId> UserID;
	FText DisplayName;
	EChatMessageType::Type MessageType;
	TSharedPtr<FFriendViewModel> ViewModel;
	TSharedPtr<FChatItemViewModel> SelectedMessage;
};

class FChatViewModel
	: public TSharedFromThis<FChatViewModel>
{
public:
	virtual TArray<TSharedRef<FChatItemViewModel > >& GetMessages() = 0;
	virtual FReply HandleSelectionChanged(TSharedRef<FChatItemViewModel> ItemSelected) = 0;
	virtual FText GetViewGroupText() const = 0;
	virtual FText GetChatGroupText() const = 0;
	virtual EVisibility GetFriendRequestVisibility() const = 0;
	virtual EVisibility GetInviteToGameVisibility() const = 0;
	virtual EVisibility GetOpenWhisperVisibility() const = 0;
	virtual EVisibility GetAcceptFriendRequestVisibility() const = 0;
	virtual EVisibility GetIgnoreFriendRequestVisibility() const = 0;
	virtual EVisibility GetCancelFriendRequestVisibility() const = 0;
	virtual void EnumerateFriendOptions(TArray<EFriendActionType::Type>& OUTActionList) = 0;
	virtual void PerformFriendAction(EFriendActionType::Type ActionType) = 0;
	virtual void CancelAction() = 0;
	virtual void SetChatChannel(const EChatMessageType::Type NewOption) = 0;
	virtual void SetWhisperChannel(const TSharedPtr<FSelectedFriend> InFriend) = 0;
	virtual void SetViewChannel(const EChatMessageType::Type NewOption) = 0;
	virtual const EChatMessageType::Type GetChatChannel() const = 0;
	virtual bool IsChatChannelValid() const = 0;
	virtual bool IsChatConnected() const = 0;
	virtual FText GetChatDisconnectText() const = 0;
	virtual void SetChannelUserClicked(const TSharedRef<FChatItemViewModel> ChatItemSelected) = 0;
	virtual bool SendMessage(const FText NewMessage) = 0;
	virtual EChatMessageType::Type GetChatChannelType() const = 0;
	virtual const TArray<TSharedPtr<FSelectedFriend> >& GetRecentOptions() const = 0;
	virtual void SetDisplayGlobalChat(bool bAllow) = 0;
	virtual bool IsDisplayingGlobalChat() const = 0;
	virtual bool IsGlobalChatEnabled() const = 0;
	virtual bool HasValidSelectedFriend() const = 0;
	virtual bool HasFriendChatAction() const = 0;
	virtual bool HasActionPending() const = 0;
	virtual void SetInGame(bool bInGameSetting) = 0;
	virtual void LockChatChannel(bool bLocked) = 0;
	virtual bool IsChatChannelLocked() const = 0;	
	virtual void EnableGlobalChat(bool bEnable) = 0;
	DECLARE_EVENT(FChatViewModel, FChatListUpdated)
	virtual FChatListUpdated& OnChatListUpdated() = 0;

	virtual ~FChatViewModel() {}
};

/**
 * Creates the implementation for an ChatViewModel.
 *
 * @return the newly created ChatViewModel implementation.
 */
FACTORY(TSharedRef< FChatViewModel >, FChatViewModel,
	const TSharedRef<class FFriendsMessageManager>& MessageManager);