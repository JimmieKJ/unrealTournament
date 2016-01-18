// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "ChatChromeViewModel.h"
#include "FriendsNavigationService.h"
#include "GameAndPartyService.h"
#include "ChatViewModel.h"
#include "ChatEntryViewModel.h"
#include "IChatSettingsService.h"
#include "ChatChromeTabViewModel.h"
#include "ChatSettingsViewModel.h"
#include "ChatDisplayService.h"
#include "ChatSettingsService.h"
#include "ChatTipViewModel.h"
#include "FriendsChatMarkupService.h"

class FChatChromeViewModelImpl
	: public FChatChromeViewModel
{
public:

	virtual void AddTab(const TSharedRef<FChatChromeTabViewModel>& Tab) override
	{
		Tabs.Add(Tab);

		if(ChatDisplayService->HideChatChrome())
		{
			// Activate Party Tab by default
			if(Tab->GetTabID() == EChatMessageType::Combined)
			{
				ActivateTab(Tab);
			}
		}
		if (!ActiveTab.IsValid() && Tab->IsTabVisible()) // Set a default tab if one is not active
		{
			ActivateTab(Tab);
		}
		
		Tab->GetChatViewModel()->SetChatSettingsService(ChatSettingsService);
	}

	virtual void ActivateTab(const TSharedRef<FChatChromeTabViewModel>& Tab, bool GiveFocus = false) override
	{
		if(ActiveTab.IsValid())
		{
			ActiveTab->GetChatViewModel()->SetIsActive(false);
		}
		ActiveTab = Tab;
		ActiveTab->GetChatViewModel()->SetIsActive(true);
		ActiveTab->ClearPendingMessages();

		if(GiveFocus && ChatDisplayService->IsChatMinimized())
		{
			SetChatMinimized(false);
		}

		if(GiveFocus)
		{
			ChatEntryViewModel->SetFocus();
		}

		for(const auto& EachTab : Tabs)
		{
			if(EachTab->HasPendingMessages())
			{
				OnPlayNotification().Broadcast(EachTab->GetTabID());
				break;
			}
		}

		ChatEntryViewModel->SetChatViewModel(ActiveTab->GetChatViewModel().ToSharedRef());
		OnActiveTabChanged().Broadcast();
	}

	virtual TSharedPtr<FChatChromeTabViewModel> GetActiveTab() const override
	{
		return ActiveTab;
	}

	virtual TArray<TSharedRef<FChatChromeTabViewModel>>& GetAllTabs() override
	{
		return Tabs;
	}
	
	virtual TArray<TSharedRef<FChatChromeTabViewModel>>& GetVisibleTabs() override
	{
		VisibleTabs.Empty();
		for (TSharedRef<FChatChromeTabViewModel> Tab : Tabs)
		{
			if (Tab->IsTabVisible())
			{
				if(!ChatDisplayService->HideChatChrome() || Tab->GetTabID() == EChatMessageType::Combined)
				{
					VisibleTabs.Add(Tab);
				}
			}
		}
		return VisibleTabs;
	}

	virtual TSharedPtr <FChatSettingsViewModel> GetChatSettingsViewModel() override
	{
		if (!ChatSettingsViewModel.IsValid())
		{
			ChatSettingsViewModel = FChatSettingsViewModelFactory::Create(ChatSettingsService);
		}
		return ChatSettingsViewModel;
	}

	virtual bool IsActive() const override
	{
		return ChatDisplayService->IsActive();
	}

	virtual EVisibility GetHeaderVisibility() const override
	{
		return ChatDisplayService->GetChatHeaderVisibiliy();
	}

	virtual EVisibility GetChatWindowVisibility() const override
	{
		return ChatDisplayService->GetChatWindowVisibiliy();
	}

	virtual EVisibility GetChatMinimizedVisibility() const override
	{
		return ChatDisplayService->GetChatMinimizedVisibility();
	}

	virtual EVisibility GetMinimizedButtonVisibility() const override
	{
		return !ChatDisplayService->IsChatMinimizeEnabled() || ChatDisplayService->IsChatMinimized() ? EVisibility::Collapsed : EVisibility::Visible;
	}

	virtual EVisibility GetMaximizedButtonVisibility() const override
	{
		return ChatDisplayService->IsChatMinimizeEnabled() && ChatDisplayService->IsChatMinimized() ?  EVisibility::Visible : EVisibility::Collapsed;
	}

	virtual const FText GetOutgoingChannelText() const override
	{
		if(ActiveTab.IsValid())
		{
			return ActiveTab->GetOutgoingChannelText();
		}
		return FText::GetEmpty();
	}

	virtual EVisibility GetChatEntryVisibility() const override
	{
		return ChatDisplayService->GetEntryBarVisibility();
	}

	virtual EChatMessageType::Type GetOutgoingChannel() const override
	{
		if(ActiveTab.IsValid())
		{
			return ActiveTab->GetOutgoingChannel();
		}
		return EChatMessageType::Invalid;
	}

	virtual bool DisplayChatSettings() const override
	{
		return !NavigationService->IsInGame();
	}

	virtual void SetChatMinimized(bool bIsMinimized) override
	{
		ChatDisplayService->SetChatMinimized(bIsMinimized);

		if (!bIsMinimized) // minimized
		{
			if(ActiveTab.IsValid())
			{
				ActivateTab(ActiveTab.ToSharedRef());
				HasPendingMessage = false;
			}
		}
	}

	virtual bool IsMinimizeEnabled() const override
	{
		return ChatDisplayService->IsChatMinimizeEnabled();
	}

	virtual bool IsChatMinimized() const override
	{
		return ChatDisplayService->IsChatMinimized();
	}

	virtual TSharedRef<FChatEntryViewModel> GetChatEntryViewModel() override
	{
		return ChatEntryViewModel;
	}

	virtual TSharedRef<FChatTipViewModel> GetChatTipViewModel() const
	{
		return FChatTipViewModelFactory::Create(MarkupService);
	}

	virtual bool HasPendingMessages() const
	{
		return HasPendingMessage;
	}

	virtual FFriendsChatMarkupService::FChatTipAvailable& OnChatTipAvailable() override
	{
		return MarkupService->OnChatTipAvailable();
	}

	virtual TSharedRef<FChatChromeViewModel> Clone(
			TSharedRef<FChatDisplayService> InChatDisplayService,
			TSharedRef<IChatSettingsService> InChatSettingsService,
			const TSharedRef<FChatEntryViewModel>& InChatEntryViewModel,
			const TSharedRef< FFriendsChatMarkupService>& InMarkupService
		) override
	{

		if(ChatDisplayService->IsChatMinimized())
		{
			InChatDisplayService->SetChatMinimized(false);
		}

		TSharedRef< FChatChromeViewModelImpl > ViewModel(new FChatChromeViewModelImpl(NavigationService, GamePartyService, InChatDisplayService, InChatSettingsService, InChatEntryViewModel, InMarkupService, MessageService));
		ViewModel->Initialize();
		for (const auto& Tab: Tabs)
		{
			ViewModel->AddTab(Tab->Clone(InChatDisplayService));
		}

		if(InChatDisplayService->HideChatChrome())
		{
			for (const auto& Tab: ViewModel->Tabs)
			{
				if(EChatMessageType::Combined == Tab->GetTabID())
				{
					ViewModel->ActivateTab(Tab);
					break;
				}
			}
		}
		else if(ActiveTab.IsValid())
		{
			for (const auto& Tab: ViewModel->Tabs)
			{
				if(ActiveTab->GetTabID() == Tab->GetTabID())
				{
					ViewModel->ActivateTab(Tab);
					break;
				}
			}
		}

		return ViewModel;
	}

	DECLARE_DERIVED_EVENT(FChatChromeViewModelImpl, FChatChromeViewModel::FActiveTabChangedEvent, FActiveTabChangedEvent)
	virtual FActiveTabChangedEvent& OnActiveTabChanged() { return ActiveTabChangedEvent; }

	DECLARE_DERIVED_EVENT(FChatChromeViewModelImpl, FChatChromeViewModel::FVisibleTabsChangedEvent, FVisibleTabsChangedEvent)
	virtual FVisibleTabsChangedEvent& OnVisibleTabsChanged() { return VisibleTabsChangedEvent; }

	DECLARE_DERIVED_EVENT(FChatChromeViewModelImpl, FChatChromeViewModel::FOnOpenSettingsEvent, FOnOpenSettingsEvent)
	virtual FOnOpenSettingsEvent& OnOpenSettings() { return OpenSettingsEvent; }

	DECLARE_DERIVED_EVENT(FChatChromeViewModelImpl, FChatChromeViewModel::FOnRegenerateChatEvent, FOnRegenerateChatEvent)
	virtual FOnRegenerateChatEvent& OnRegenerateChat() { return RegenerateChatEvent; }

	DECLARE_DERIVED_EVENT(FChatChromeViewModelImpl, FChatChromeViewModel::FPlayNotificationEvent, FPlayNotificationEvent)
	virtual FPlayNotificationEvent& OnPlayNotification() { return PlayNotificationEvent; }

private:

	void HandleViewChangedEvent(EChatMessageType::Type NewChannel)
	{
		if(ChatDisplayService->HideChatChrome())
		{
			ActiveTab->GetChatViewModel()->SetOutgoingMessageChannel(NewChannel);
		}
		else
		{
			EChatMessageType::Type SelectedTab = NewChannel;
			// Hack to combine Party and Game tabs - NDavies
			if (NewChannel == EChatMessageType::Game && GamePartyService->ShouldCombineGameChatIntoPartyTab())
			{
				SelectedTab = EChatMessageType::Party;
			}

			for (const auto& Tab: Tabs)
			{
				if(Tab->GetTabID() == SelectedTab)
				{
					// Find the new tab, make it visible, active, and the selected output channel
					if (!VisibleTabs.Contains(Tab))
					{
						OnVisibleTabsChanged().Broadcast();
					}
					ActivateTab(Tab);

					if (NewChannel == EChatMessageType::Game || NewChannel == EChatMessageType::Party)
					{
						// Update the outgoing channel.  (Game output will still get redirected to Party tab if combine is enabled)
						ActiveTab->GetChatViewModel()->SetOutgoingMessageChannel(NewChannel);
					}

					break;
				}
			}
		}
	}

	void HandleSettingsChanged(EChatSettingsType::Type OptionType, bool NewState)
	{
// 		if (NewState == ECheckBoxState::Unchecked)
// 		{
// 		}
	}

	void HandleRadioSettingsChanged(EChatSettingsType::Type OptionType, uint8 NewIndex)
	{
		if (OptionType == EChatSettingsType::FontSize)
		{
			OnRegenerateChat().Broadcast();
		}
	}


	void HandleGameChatChanged()
	{
		// ShouldCombineGameChatIntoPartyTab is handled by HandleViewChanged and other events triggered here, this method can pretend to know nothing of the combining
		if (GamePartyService->IsInGameChat())
		{
			// If we just entered game chat, bring game chat to the front and set it as the selected channel
			for (const auto& Tab : Tabs)
			{
				if (Tab->GetTabID() == EChatMessageType::Game)
				{
					if (!VisibleTabs.Contains(Tab))
					{
						ActivateTab(Tab);
						NavigationService->ChangeViewChannel(EChatMessageType::Game);
					}
					break;
				}
			}
		}
		else
		{
			// If we just left game chat, change view and selection away from Game chat
			if (ActiveTab.IsValid() && ActiveTab->GetTabID() == EChatMessageType::Game)
			{
				// Try to duck out to Party if party still together after leaving game. Else go all the way out to global.
				if (GamePartyService->IsInPartyChat())
				{
					NavigationService->ChangeViewChannel(EChatMessageType::Party);
				}
				else
				{
					NavigateToValidTab();
				}
			}
		}
	}

	void HandleTeamChatChanged(bool bEnabled)
	{
		if(ChatDisplayService->HideChatChrome())
		{
			if(ActiveTab.IsValid())
			{
				if(bEnabled)
				{
					ActiveTab->GetChatViewModel()->SetOutgoingMessageChannel(EChatMessageType::Team);
				}
				else
				{
					ActiveTab->GetChatViewModel()->SetOutgoingMessageChannel(EChatMessageType::Whisper);
				}
			}
		}
		else
		{
			if(bEnabled)
			{
				if(!ActiveTab.IsValid() || ActiveTab->GetTabID() != EChatMessageType::Party)
				{
					NavigationService->ChangeViewChannel(EChatMessageType::Team);
				}
			}
			else
			{
				NavigateToValidTab();
			}
		}
	}

	void HandlePartyChatChanged()
	{
		if(ChatDisplayService->HideChatChrome())
		{
			if(ActiveTab.IsValid())
			{
				ActiveTab->GetChatViewModel()->SetOutgoingMessageChannel(EChatMessageType::Party);
			}
		}
		else
		{
			if (GamePartyService->IsInPartyChat())
			{
				// If we just entered party chat, bring party chat to the front and set it as the selected channel
				for (const auto& Tab: Tabs)
				{
					if(Tab->GetTabID() == EChatMessageType::Party)
					{
						if (!VisibleTabs.Contains(Tab))
						{
							ActivateTab(Tab);
							NavigationService->ChangeViewChannel(EChatMessageType::Party);
						}
						break;
					}
				}
			}
			else
			{
				// If we just left party chat, change view and selection to global as the default
				if (ActiveTab.IsValid() && ActiveTab->GetTabID() == EChatMessageType::Party)
				{
					HandleCycleTabs();
				}
			}
		}
	}

	void NavigateToValidTab()
	{
		TSharedPtr<FChatChromeTabViewModel> TabToActivate;
		GetVisibleTabs();
		for(const auto& VisibleTab : VisibleTabs)
		{
			if(VisibleTab->GetTabID() == EChatMessageType::Global)
			{
				TabToActivate = VisibleTab;
				break;
			}
			else if(VisibleTab->GetTabID() == EChatMessageType::Whisper)
			{
				TabToActivate = VisibleTab;
			}
		}

		if(TabToActivate.IsValid())
		{
			ActivateTab(TabToActivate.ToSharedRef());
		}
		else if(WhisperTab.IsValid())
		{
			ActivateTab(WhisperTab.ToSharedRef());
		}
		else
		{
			NavigationService->ChangeViewChannel(EChatMessageType::Whisper);
		}
	}

	void HandleCycleTabs()
	{
		if(ChatDisplayService->HideChatChrome())
		{
			bool bAtEndOfList = MarkupService->IsAtEndOfFriendsList();

			EChatMessageType::Type CurrentMessageChannel = ActiveTab->GetOutgoingChannel();
			switch (CurrentMessageChannel)
			{
				case EChatMessageType::Whisper:
				{
					MarkupService->SetNextValidFriend();
					if(bAtEndOfList)
					{
						if(GamePartyService->IsInPartyChat())
						{
							ActiveTab->GetChatViewModel()->SetOutgoingMessageChannel(EChatMessageType::Party);
						}
						else if (GamePartyService->IsInTeamChat())
						{
							ActiveTab->GetChatViewModel()->SetOutgoingMessageChannel(EChatMessageType::Team);
						}
					}
					return;
				}
				case EChatMessageType::Party:
				{
					if(GamePartyService->IsInTeamChat())
					{
						ActiveTab->GetChatViewModel()->SetOutgoingMessageChannel(EChatMessageType::Team);
						return;
					}
				}
			}
			// Nothing set yet, return to Whisper
			ActiveTab->GetChatViewModel()->SetOutgoingMessageChannel(EChatMessageType::Whisper);
			return;
		}

		GetVisibleTabs();

		// If we have more than one active tab, select the next tab
		if(VisibleTabs.Num() > 1 || !ActiveTab.IsValid() || !ActiveTab->IsTabVisible())
		{
			bool SetNext = false;
			for(const auto& VisibleTab : VisibleTabs)
			{
				if(SetNext)
				{
					ActivateTab(VisibleTab, true);
					SetNext = false;
					break;
				}
				else if(VisibleTab == ActiveTab)
				{
					SetNext = true;
				}
			}

			// We are at the end of the tab list.
			if((SetNext || !ActiveTab.IsValid()) && VisibleTabs.Num())
			{
				ActivateTab(VisibleTabs[0], true);
			}
		}
	}

	void HandleSetFocus()
	{
		ChatEntryViewModel->SetFocus();
	}

	void HandleMinimized()
	{
		MarkupService->OnChatTipAvailable().Broadcast(false);
	}

	void HandleChatMessageReceived(TSharedRef< FFriendChatMessage > NewMessage)
	{
		if(!ActiveTab.IsValid() || ActiveTab->GetChatViewModel()->IsChannelSet(NewMessage->MessageType) == false)
		{
			bool bRequestedNotificationAnimation = false;
			for(const auto& Tab : Tabs)
			{
				if(Tab->GetChatViewModel()->GetDefaultChannelType() == NewMessage->MessageType)
				{
					Tab->SetPendingMessage();
					OnPlayNotification().Broadcast(Tab->GetTabID());
					bRequestedNotificationAnimation = true;
					break;
				}
			}

			if(ChatDisplayService->IsChatMinimized())
			{
				HasPendingMessage = true;
				if(!bRequestedNotificationAnimation)
				{
					OnPlayNotification().Broadcast(NewMessage->MessageType);
				}
			}
		}
		if(NewMessage->MessageType == EChatMessageType::Whisper)
		{
			if(!NewMessage->bIsFromSelf)
			{
				MarkupService->SetLastMessageFriend(NewMessage->SenderId);
			}
			else
			{
				MarkupService->SetLastMessageFriend(NewMessage->RecipientId);
			}
		}
	}

	void Initialize()
	{
		ChatSettingsService->OnChatSettingStateUpdated().AddSP(this, &FChatChromeViewModelImpl::HandleSettingsChanged);
		ChatSettingsService->OnChatSettingRadioStateUpdated().AddSP(this, &FChatChromeViewModelImpl::HandleRadioSettingsChanged);
		NavigationService->OnChatViewChanged().AddSP(this, &FChatChromeViewModelImpl::HandleViewChangedEvent);
		NavigationService->OnCycleChatTabEvent().AddSP(this, &FChatChromeViewModelImpl::HandleCycleTabs);
		GamePartyService->OnPartyChatChanged().AddSP(this, &FChatChromeViewModelImpl::HandlePartyChatChanged);

		if(!ChatDisplayService->HideChatChrome())
		{
			GamePartyService->OnGameChatChanged().AddSP(this, &FChatChromeViewModelImpl::HandleGameChatChanged);
			GamePartyService->OnTeamChatChanged().AddSP(this, &FChatChromeViewModelImpl::HandleTeamChatChanged);
			MessageService->OnChatMessageAdded().AddSP(this, &FChatChromeViewModelImpl::HandleChatMessageReceived);
		}
		ChatDisplayService->OnChatListSetFocus().AddSP(this, &FChatChromeViewModelImpl::HandleSetFocus);
		ChatDisplayService->OnChatMinimized().AddSP(this, &FChatChromeViewModelImpl::HandleMinimized);
	}

	FChatChromeViewModelImpl(
		const TSharedRef<FFriendsNavigationService>& InNavigationService,
		const TSharedRef<FGameAndPartyService>& InGamePartyService,
		const TSharedRef<FChatDisplayService> & InChatDisplayService,
		const TSharedRef<IChatSettingsService>& InChatSettingsService,
		const TSharedRef<FChatEntryViewModel>& InChatEntryViewModel,
		const TSharedRef<FFriendsChatMarkupService>& InMarkupService,
		const TSharedRef<FMessageService>& InMessageService)
		: NavigationService(InNavigationService)
		, GamePartyService(InGamePartyService)
		, ChatDisplayService(InChatDisplayService)
		, ChatSettingsService(InChatSettingsService)
		, ChatEntryViewModel(InChatEntryViewModel)
		, MarkupService(InMarkupService)
		, MessageService(InMessageService)
		, HasPendingMessage(false)
		, RegenerateChat(false)
	{};

private:

	FActiveTabChangedEvent ActiveTabChangedEvent;
	FVisibleTabsChangedEvent VisibleTabsChangedEvent;
	FOnOpenSettingsEvent OpenSettingsEvent;
	FOnRegenerateChatEvent RegenerateChatEvent;
	FPlayNotificationEvent PlayNotificationEvent;

	TSharedPtr<FChatChromeTabViewModel> ActiveTab;
	TSharedPtr<FChatChromeTabViewModel> WhisperTab;
	TArray<TSharedRef<FChatChromeTabViewModel>> Tabs;
	TArray<TSharedRef<FChatChromeTabViewModel>> VisibleTabs;
	TSharedRef<FFriendsNavigationService> NavigationService;
	TSharedRef<FGameAndPartyService> GamePartyService;
	TSharedRef<FChatDisplayService> ChatDisplayService;
	TSharedRef<IChatSettingsService> ChatSettingsService;
	TSharedPtr<FChatSettingsViewModel> ChatSettingsViewModel;
	TSharedRef<FChatEntryViewModel> ChatEntryViewModel;
	TSharedRef<FFriendsChatMarkupService> MarkupService;
	TSharedRef<FMessageService> MessageService;
	bool HasPendingMessage;
	bool RegenerateChat;

	friend FChatChromeViewModelFactory;
};

TSharedRef< FChatChromeViewModel > FChatChromeViewModelFactory::Create(
	const TSharedRef<FFriendsNavigationService>& NavigationService,
	const TSharedRef<FGameAndPartyService>& GamePartyService,
	const TSharedRef<FChatDisplayService> & ChatDisplayService,
	const TSharedRef<IChatSettingsService>& InChatSettingsService,
	const TSharedRef<FChatEntryViewModel>& InChatEntryViewModel,
	const TSharedRef<FFriendsChatMarkupService>& InMarkupService,
	const TSharedRef<FMessageService>& InMessageService
)
{
	TSharedRef< FChatChromeViewModelImpl > ViewModel(new FChatChromeViewModelImpl(NavigationService, GamePartyService, ChatDisplayService, InChatSettingsService, InChatEntryViewModel, InMarkupService, InMessageService));
	ViewModel->Initialize();
	return ViewModel;
}