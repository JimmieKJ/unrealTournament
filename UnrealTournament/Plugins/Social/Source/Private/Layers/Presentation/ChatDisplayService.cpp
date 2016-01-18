// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "ChatDisplayService.h"
#include "ChatViewModel.h"

class FChatDisplayServiceImpl
	: public FChatDisplayService
{
public:

	~FChatDisplayServiceImpl()
	{
		if (TickerHandle.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		}
	}

	virtual void SetMinimizeEnabled(bool InMinimizeEnabled) override
	{
		ChatMinimizeEnabled = InMinimizeEnabled;
	}

	virtual void SetListFadeTime(float InListFadeTime) override
	{
		ChatFadeInterval = InListFadeTime;
	}

	virtual void SetMinimizedChatMessageNum(int32 InMinimizedChatMessageNum) override
	{
		MinimizedChatMessageNum = InMinimizedChatMessageNum;
	}

	virtual void SetHideChrome(bool bInHideChrome) override
	{
		bHideChrome = bInHideChrome;
		if(bHideChrome)
		{
			ChatEntryVisibility = EVisibility::Hidden;
			ChatAutoFaded = true;
		}
	}

	virtual void SetChatOrientation(EChatOrientationType::Type InChatOrientation) override
	{
		ChatOrientation = InChatOrientation;
	}

	virtual EChatOrientationType::Type GetChatOrientation() override
	{
		return ChatOrientation;
	}

	virtual void SetFocus() override
	{
		SetChatListVisibility(true);
		SetChatEntryVisibility(true);
		ChatMinimized = false;
		ChatAutoFaded = false;
		OnChatListSetFocus().Broadcast();
	}

	virtual void ChatEntered() override
	{
		SetChatEntryVisibility(true);
		SetChatListVisibility(true);
	}

	virtual void MessageCommitted() override
	{
		OnChatMessageCommitted().Broadcast();

		if (bHideChrome)
		{
			SetChatEntryVisibility(false);
		}
		else if(AutoReleaseFocus)
		{
			OnFocuseReleasedEvent().Broadcast();
		}

		SetChatListVisibility(true);

		if (IsChatMinimized())
		{
			ChatListVisibility = EVisibility::HitTestInvisible;
		}
	}

	virtual EVisibility GetEntryBarVisibility() const override
	{
		if(EntryVisibilityOverride.IsSet())
		{
			return EntryVisibilityOverride.GetValue();
		}
		return ChatEntryVisibility;
	}

	virtual EVisibility GetChatHeaderVisibiliy() const override
	{
		// Don't show the header when the chat list is hidden
		if(TabVisibilityOverride.IsSet())
		{
			return TabVisibilityOverride.GetValue();
		}

		return IsChatMinimized() ? EVisibility::Collapsed : ChatEntryVisibility;
	}

	virtual EVisibility GetChatWindowVisibiliy() const override
	{
		return IsChatMinimized() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible;
	}

	virtual EVisibility GetChatMinimizedVisibility() const override
	{
		return IsChatMinimized() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
	}

	virtual EVisibility GetChatListVisibility() const override
	{
		if(ChatListVisibilityOverride.IsSet())
		{
			return ChatListVisibilityOverride.GetValue();
		}
		return ChatListVisibility;
	}

	virtual int32 GetMinimizedChatMessageNum() const override
	{
		return MinimizedChatMessageNum;
	}

	virtual void SetFadeBackgroundEnabled(bool bEnabled) override
	{
		FadeBackgroundEnabled = bEnabled;
	}

	virtual bool IsFadeBackgroundEnabled() const override
	{
		return FadeBackgroundEnabled;
	}

	virtual EVisibility GetBackgroundVisibility() const override
	{
		if(BackgroundVisibilityOverride.IsSet())
		{
			return BackgroundVisibilityOverride.GetValue();
		}
		return IsChatMinimized() ? EVisibility::Hidden : EVisibility::Visible;
	}

	virtual bool HideChatChrome() const override
	{
		return bHideChrome;
	}

	virtual void SetActive(bool bIsActive) override
	{
		IsChatActive = bIsActive;
		if(IsChatActive)
		{
			SetFocus();
		}
	}

	virtual bool IsActive() const override
	{
		return IsChatActive;
	}

	virtual bool IsChatMinimizeEnabled() override
	{
		return ChatMinimizeEnabled;
	}

	virtual void SetChatMinimized(bool bIsMinimized) override
	{
		if (!IsChatMinimizeEnabled())
		{
			return;
		}

		ChatMinimized = bIsMinimized;
		if(ChatMinimized)
		{
			ChatListVisibility = EVisibility::HitTestInvisible;
			RequestFadeChat();
		}
		else
		{
			RequestChatFade = false;
			ChatAutoFaded = false;
			SetChatListVisibility(true);
		}
	}

	virtual bool IsChatMinimized() const override
	{
		return (ChatMinimizeEnabled && ChatMinimized) || bHideChrome;
	}

	virtual bool IsAutoFaded() const override
	{
		return ChatMinimized && ChatAutoFaded;
	}

	virtual void SetTabVisibilityOverride(EVisibility InTabVisibilityOverride) override
	{
		TabVisibilityOverride = InTabVisibilityOverride;
	}

	virtual void ClearTabVisibilityOverride() override
	{
		TabVisibilityOverride.Reset();
	}

	virtual void SetEntryVisibilityOverride(EVisibility InEntryVisibilityOverride) override
	{
		EntryVisibilityOverride = InEntryVisibilityOverride;
	}

	virtual void ClearEntryVisibilityOverride() override
	{
		EntryVisibilityOverride.Reset();
	}

	virtual void SetBackgroundVisibilityOverride(EVisibility InBackgroundVisibilityOverride) override
	{
		BackgroundVisibilityOverride = InBackgroundVisibilityOverride;
	}

	virtual void ClearBackgroundVisibilityOverride() override
	{
		BackgroundVisibilityOverride.Reset();
	}

	virtual void SetChatListVisibilityOverride(EVisibility InChatVisibilityOverride) override
	{
		ChatListVisibilityOverride = InChatVisibilityOverride; 
	}

	virtual void ClearChatListVisibilityOverride() override
	{
		ChatListVisibilityOverride.Reset();
	}

	virtual void SetActiveTab(TWeakPtr<FChatViewModel> InActiveTab) override
	{
		ActiveTab = InActiveTab;
	}

	virtual void SendLiveStreamMessage(const FString& LiveStreamMessage) override
	{
		OnLiveStreamMessageSentEvent().Broadcast(LiveStreamMessage);
	}

	virtual bool ShouldAutoRelease() override
	{
		return AutoReleaseFocus;
	}

	virtual void SetAutoReleaseFocus(bool InAutoRelease) override
	{
		AutoReleaseFocus = InAutoRelease;
	}

	DECLARE_DERIVED_EVENT(FChatDisplayServiceImpl, IChatDisplayService::FChatListUpdated, FChatListUpdated);
	virtual FChatListUpdated& OnChatListUpdated() override
	{
		return ChatListUpdatedEvent;
	}

	DECLARE_DERIVED_EVENT(FChatDisplayServiceImpl, IChatDisplayService::FOnFriendsChatMessageCommitted, FOnFriendsChatMessageCommitted);
	virtual FOnFriendsChatMessageCommitted& OnChatMessageCommitted() override
	{
		return ChatMessageCommittedEvent;
	}

	DECLARE_DERIVED_EVENT(FChatDisplayServiceImpl, IChatDisplayService::FOnSendLiveSteamMessageEvent, FOnSendLiveSteamMessageEvent);
	virtual FOnSendLiveSteamMessageEvent& OnLiveStreamMessageSentEvent() override
	{
		return SendLiveStreamkMessageEvent;
	}

	DECLARE_DERIVED_EVENT(FChatDisplayServiceImpl, IChatDisplayService::FOnFocusReleasedEvent, FOnFocusReleasedEvent);
	virtual FOnFocusReleasedEvent& OnFocuseReleasedEvent() override
	{
		return OnFocusReleasedEvent;
	}

	DECLARE_DERIVED_EVENT(FChatDisplayServiceImpl, FChatDisplayService::FChatListSetFocus, FChatListSetFocus);
	virtual FChatListSetFocus& OnChatListSetFocus() override
	{
		return ChatSetFocusEvent;
	}

	DECLARE_DERIVED_EVENT(FChatDisplayServiceImpl, FChatDisplayService::FChatListMinimized, FChatListMinimized);
	virtual FChatListMinimized& OnChatMinimized() override
	{
		return ChatMinimizedEvent;
	}
	
private:

	void RequestFadeChat()
	{
		if(ChatFadeInterval > -1)
		{
			RequestChatFade = true;
			ChatFadeDelay = ChatFadeInterval;
		}
		ChatAutoFaded = false;
		OnChatMinimized().Broadcast();
	}

	void SetChatEntryVisibility(bool Visible)
	{
		if(Visible)
		{
			ChatEntryVisibility = EVisibility::Visible;
			ChatAutoFaded = false;
			RequestChatFade = false;
		}
		else
		{
			ChatEntryVisibility = EVisibility::Hidden;
			OnFocuseReleasedEvent().Broadcast();
			RequestFadeChat();
		}
	}

	void SetChatListVisibility(bool Visible)
	{
	}

	void HandleChatMessageReceived(TSharedRef< FFriendChatMessage > NewMessage)
	{
		if(IsChatMinimizeEnabled())
		{
			ChatFadeDelay = ChatFadeInterval;
			ChatAutoFaded = false;
			if(bHideChrome)
			{
				RequestFadeChat();
			}
		}
	}

	bool HandleTick(float DeltaTime)
	{
		if (!ChatAutoFaded && RequestChatFade && ChatFadeDelay > 0)
		{
			ChatFadeDelay -= DeltaTime;
			if(ChatFadeDelay <= 0)
			{
				ChatAutoFaded = true;
			}
		}

		return true;
	}

	void Initialize()
	{
		ChatService->OnChatMessageAdded().AddSP(this, &FChatDisplayServiceImpl::HandleChatMessageReceived);

		TickDelegate = FTickerDelegate::CreateSP(this, &FChatDisplayServiceImpl::HandleTick);
		TickerHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate);
	}

	FChatDisplayServiceImpl(const TSharedRef<IChatCommunicationService>& InChatService)
		: ChatService(InChatService)
		, bHideChrome(false)
		, ChatFadeInterval(-1.0f)
		, MinimizedChatMessageNum(3)
		, ChatOrientation(EChatOrientationType::COT_Horizontal)
		, ChatEntryVisibility(EVisibility::Visible)
		, ChatListVisibility(EVisibility::Visible)
		, IsChatActive(true)
		, ChatMinimizeEnabled(true)
		, ChatMinimized(true)
		, RequestChatFade(false)
		, ChatAutoFaded(false)
		, AutoReleaseFocus(false)
		, FadeBackgroundEnabled(true)
	{
	}

private:

	TSharedRef<IChatCommunicationService> ChatService;
	bool bHideChrome;
	float ChatFadeDelay;
	float ChatFadeInterval;
	int32 MinimizedChatMessageNum;
	EChatOrientationType::Type ChatOrientation;
	EVisibility ChatEntryVisibility;
	EVisibility ChatListVisibility;
	bool IsChatActive;
	bool ChatMinimizeEnabled;
	bool ChatMinimized;
	bool RequestChatFade;
	bool ChatAutoFaded;
	TOptional<bool> CachedMinimize;
	bool AutoReleaseFocus;
	bool FadeBackgroundEnabled;

	TOptional<EVisibility> TabVisibilityOverride;
	TOptional<EVisibility> EntryVisibilityOverride;
	TOptional<EVisibility> BackgroundVisibilityOverride;
	TOptional<EVisibility> ChatListVisibilityOverride;

	FChatListUpdated ChatListUpdatedEvent;
	FOnFriendsChatMessageCommitted ChatMessageCommittedEvent;
	FOnSendLiveSteamMessageEvent SendLiveStreamkMessageEvent;
	FOnFocusReleasedEvent OnFocusReleasedEvent;

	FChatListSetFocus ChatSetFocusEvent;
	FChatListMinimized ChatMinimizedEvent;

	TWeakPtr<FChatViewModel> ActiveTab;

	// Delegate for which function we should use when we tick
	FTickerDelegate TickDelegate;
	// Handler for the tick function when active
	FDelegateHandle TickerHandle;

	friend FChatDisplayServiceFactory;
};

TSharedRef< FChatDisplayService > FChatDisplayServiceFactory::Create(const TSharedRef<IChatCommunicationService>& ChatService)
{
	TSharedRef< FChatDisplayServiceImpl > DisplayService = MakeShareable(new FChatDisplayServiceImpl(ChatService));
	DisplayService->Initialize();
	return DisplayService;
}