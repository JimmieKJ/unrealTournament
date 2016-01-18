// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "FriendsMessageType.h"
#include "FriendsChatMarkupService.h"

class FChatChromeViewModel
	: public TSharedFromThis<FChatChromeViewModel>
{
public:

	virtual void AddTab(const TSharedRef<class FChatChromeTabViewModel>& Tab) = 0;
	virtual void ActivateTab(const TSharedRef<class FChatChromeTabViewModel>& Tab, bool GiveFocus = false) = 0;
	virtual TSharedPtr<class FChatChromeTabViewModel> GetActiveTab() const = 0;
	virtual TArray<TSharedRef<class FChatChromeTabViewModel>>& GetVisibleTabs() = 0;
	virtual TArray<TSharedRef<class FChatChromeTabViewModel>>& GetAllTabs() = 0;
	virtual bool IsActive() const = 0;
	virtual EVisibility GetHeaderVisibility() const = 0;
	virtual EVisibility GetChatWindowVisibility() const = 0;
	virtual EVisibility GetChatMinimizedVisibility() const = 0;
	virtual EVisibility GetMinimizedButtonVisibility() const = 0;
	virtual EVisibility GetMaximizedButtonVisibility() const = 0;
	virtual const FText GetOutgoingChannelText() const = 0;
	virtual EVisibility GetChatEntryVisibility() const = 0;
	virtual EChatMessageType::Type GetOutgoingChannel() const = 0;
	virtual TSharedPtr<class FChatSettingsViewModel> GetChatSettingsViewModel() = 0;
	virtual bool DisplayChatSettings() const = 0;
	virtual void SetChatMinimized(bool bIsMinimized) = 0;
	virtual bool IsMinimizeEnabled() const = 0;
	virtual bool IsChatMinimized() const = 0;
	virtual TSharedRef<class FChatEntryViewModel> GetChatEntryViewModel() = 0;
	virtual TSharedRef<class FChatTipViewModel> GetChatTipViewModel() const = 0;
	virtual bool HasPendingMessages() const = 0;
	virtual FFriendsChatMarkupService::FChatTipAvailable& OnChatTipAvailable() = 0;

	virtual TSharedRef<FChatChromeViewModel> Clone(TSharedRef<class FChatDisplayService> ChatDisplayService,
		TSharedRef<class IChatSettingsService> InChatSettingsService, 
		const TSharedRef<class FChatEntryViewModel>& ChatEntryViewModel,
		const TSharedRef<class FFriendsChatMarkupService>& MarkupService
		) = 0;

	virtual ~FChatChromeViewModel() {}


	DECLARE_EVENT(FChatChromeViewModel, FActiveTabChangedEvent)
	virtual FActiveTabChangedEvent& OnActiveTabChanged() = 0;

	DECLARE_EVENT(FChatChromeViewModel, FVisibleTabsChangedEvent)
	virtual FVisibleTabsChangedEvent& OnVisibleTabsChanged() = 0;

	DECLARE_EVENT(FChatChromeViewModel, FOnOpenSettingsEvent)
	virtual FOnOpenSettingsEvent& OnOpenSettings() = 0;

	DECLARE_EVENT(FChatChromeViewModel, FOnRegenerateChatEvent)
	virtual FOnRegenerateChatEvent& OnRegenerateChat() = 0;

	DECLARE_EVENT_OneParam(FChatChromeViewModel, FPlayNotificationEvent, EChatMessageType::Type /*MessageType*/)
	virtual FPlayNotificationEvent& OnPlayNotification() = 0;
};

/**
 * Creates the implementation for an ChatChromeViewModel.
 *
 * @return the newly created ChatChromeViewModel implementation.
 */
FACTORY(TSharedRef< FChatChromeViewModel >, FChatChromeViewModel,
	const TSharedRef<class FFriendsNavigationService>& NavigationService,
	const TSharedRef<class FGameAndPartyService>& GamePartyService,
	const TSharedRef<class FChatDisplayService> & ChatDisplayService,
	const TSharedRef<class IChatSettingsService>& ChatSettingsService,
	const TSharedRef<class FChatEntryViewModel>& ChatEntryViewModel,
	const TSharedRef<class FFriendsChatMarkupService>& MarkupService,
	const TSharedRef<class FMessageService>& MessageService
	);