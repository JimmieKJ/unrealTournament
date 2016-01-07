// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "FriendsFontStyleService.h"
#include "ChatItemViewModel.h"
#include "ChatViewModel.h"
#include "FriendViewModel.h"
#include "OnlineChatInterface.h"
#include "FriendsNavigationService.h"
#include "FriendsService.h"
#include "ChatTipViewModel.h"
#include "ChatSettingsService.h"
#include "GameAndPartyService.h"
#include "FriendsAndChatAnalytics.h"
#include "FriendRecentPlayerItems.h"
#include "ChatDisplayService.h"
#include "ChatTextLayoutMarshaller.h"

#define LOCTEXT_NAMESPACE ""

#define UE_LOG_SOCIALCHAT(Verbosity, Format, ...) \
{ \
	UE_LOG(LogSocialChat, Verbosity, TEXT("%s"), *FString::Printf(Format, ##__VA_ARGS__)); \
}

class FChatViewModelImpl
	: public FChatViewModel
{
public:

	// Begin FChatViewModel interface

	virtual TSharedRef<FChatViewModel> Clone(const TSharedRef<FChatDisplayService>& InChatDisplayService) override
	{
		TSharedRef< FChatViewModelImpl > ViewModel(new FChatViewModelImpl(FriendViewModelFactory, MessageService, NavigationService, InChatDisplayService, FriendsService, GamePartyService));
		ViewModel->Initialize(true);
		ViewModel->SetFilteredMessages(FilteredMessages);
		ViewModel->SetChannelFlags(ChatChannelFlags);
		ViewModel->DefaultChannel = DefaultChannel;
		ViewModel->DefaultChatChannelFlags = DefaultChatChannelFlags;
		if(SelectedFriend.IsValid())
		{
			ViewModel->SetWhisperFriend(SelectedFriend);
		}
		return ViewModel;
	}

	virtual FText GetChatGroupText(bool ShowWhisperFriendsName) const override
	{
		if(OutgoingMessageChannel == EChatMessageType::Whisper)
		{
			return SelectedFriend.IsValid() && ShowWhisperFriendsName ? SelectedFriend->DisplayName : EChatMessageType::ToText(OutgoingMessageChannel);
		}
		return EChatMessageType::ToText(OutgoingMessageChannel);
	}

	virtual TArray< TSharedRef<FChatItemViewModel > >& GetMessages() override
	{
		return FilteredMessages;
	}

	virtual int32 GetMessageCount() const override
	{
		return FilteredMessages.Num();
	}

	virtual bool HasDefaultChannelMessage() const override
	{
		for (const auto& Message : FilteredMessages)
		{
			if (Message->GetMessageType() == GetDefaultChannelType())
			{
				return true;
			}
		}
		return false;
	}

	virtual TSharedPtr<FFriendViewModel> GetFriendViewModel(const TSharedPtr<const FUniqueNetId> InUserID, const FText Username) override
	{
		TSharedPtr<IFriendItem> FoundFriend = FriendsService->FindUser(InUserID.ToSharedRef());

		if (!FoundFriend.IsValid())
		{
			FoundFriend = FriendsService->FindBlockedPlayer(*InUserID.Get());
		}
		if (!FoundFriend.IsValid())
		{
			FoundFriend = FriendsService->FindRecentPlayer(*InUserID.Get());
		}
		if (!FoundFriend.IsValid())
		{
			FoundFriend = MakeShareable(new FFriendRecentPlayerItem(InUserID, Username));
		}
		if (FoundFriend.IsValid())
		{
			return FriendViewModelFactory->Create(FoundFriend.ToSharedRef());
		}
		return nullptr;
	}

	virtual TSharedPtr<FFriendViewModel> GetFriendViewModel(const FString InUserID, const FText Username) override
	{
		TSharedPtr<const FUniqueNetId> NetID = FriendsService->CreateUniquePlayerId(InUserID);
		return GetFriendViewModel(NetID, Username);
	}

	virtual void SetDefaultOutgoingChannel(EChatMessageType::Type InChannel)
	{
		DefaultChannel = InChannel;
		SetOutgoingMessageChannel(DefaultChannel);
	}

	virtual void SetDefaultChannelFlags(uint16 ChatFlags) override
	{
		DefaultChatChannelFlags = ChatFlags;
		SetChannelFlags(DefaultChatChannelFlags);
	}

	virtual void ResetToDefaultChannel() override
	{
		SetChannelFlags(DefaultChatChannelFlags);
		SetOutgoingMessageChannel(DefaultChannel);
	}

	virtual void SetChannelFlags(uint16 ChatFlags) override
	{
		ChatChannelFlags = ChatFlags;
		bHasActionPending = false;
		if (!IsChannelSet(EChatMessageType::Whisper))
		{
			SelectedFriend.Reset();
		}
	}

	virtual uint16 GetChannelFlags() override
	{
		return ChatChannelFlags;
	}

	virtual bool IsChannelSet(const EChatMessageType::Type InChannel) override
	{
		return (InChannel & ChatChannelFlags) != 0;
	}

	virtual void ToggleChannel(const EChatMessageType::Type InChannel) override
	{
		ChatChannelFlags ^= InChannel;
	}

	virtual void SetOutgoingMessageChannel(const EChatMessageType::Type InChannel) override
	{
		UE_LOG_SOCIALCHAT(Verbose, TEXT("Changing outgoing message channel to %s"), *EChatMessageType::ToText(InChannel).ToString());
		OutgoingMessageChannel = InChannel;
		if(InChannel == EChatMessageType::Whisper && IsWhisperFriendSet())
		{
			OutGoingChannelText = FText::Format(LOCTEXT("WhisperChannelText", "{0} {1}"), EChatMessageType::ToText(OutgoingMessageChannel), SelectedFriend->DisplayName);
		}
		else
		{
			OutGoingChannelText = EChatMessageType::ToText(OutgoingMessageChannel);
		}
	}

	virtual void NavigateToChannel(const EChatMessageType::Type InChannel) override
	{
		UE_LOG_SOCIALCHAT(Verbose, TEXT("Changing view channel to %s"), *EChatMessageType::ToText(InChannel).ToString());
		if(InChannel == EChatMessageType::Party || InChannel == EChatMessageType::Game)
		{
			// ToDo - move this logic into the navigation system rather than checking party membership here
			if (GamePartyService->IsInGameChat() || GamePartyService->IsInPartyChat())
			{
				NavigationService->ChangeViewChannel(InChannel);
			}
		}
		else
		{
			NavigationService->ChangeViewChannel(InChannel);
		}
	}

	virtual EChatMessageType::Type GetOutgoingChatChannel() const override
	{
		return OutgoingMessageChannel;
	}
	
	virtual FText GetOutgoingChannelText() const override
	{
		return OutGoingChannelText;
	}

	virtual FText GetChannelErrorText() const override
	{
		if(OutgoingMessageChannel == EChatMessageType::Whisper && !IsWhisperFriendSet())
		{
			return NSLOCTEXT("ChatViewModel", "NoFriendSet", "No Friend Set.");
		}
		return FText::GetEmpty();
	}

	virtual void SetWhisperFriend(const TSharedPtr<FSelectedFriend> InFriend) override
	{
		SelectedFriend = InFriend;
		if (SelectedFriend->UserID.IsValid())
		{
			SelectedFriend->ViewModel = GetFriendViewModel(SelectedFriend->UserID, SelectedFriend->DisplayName);
		}
		SelectedFriend->MessageType = EChatMessageType::Whisper;
		bHasActionPending = false;

		// Re set the channel to rebuild the channel text
		if(OutgoingMessageChannel == EChatMessageType::Whisper || GetChatChannelType() == EChatMessageType::Combined)
		{
			SetOutgoingMessageChannel(EChatMessageType::Whisper);
		}
	}

	virtual bool IsWhisperFriendSet() const override
	{
		return SelectedFriend.IsValid();
	}

	virtual TSharedPtr<FFriendViewModel> GetSelectedFriendViewModel() const override
	{
		if(SelectedFriend.IsValid() && SelectedFriend->UserID.IsValid())
		{
			return SelectedFriend->ViewModel;
		}
		return nullptr;
	}

	virtual bool IsInPartyChat() const override
	{
		return GamePartyService->IsInPartyChat();
	}

	virtual bool IsInTeamChat() const override
	{
		return GamePartyService->IsInTeamChat();
	}

	virtual bool IsInGameChat() const override
	{
		return GamePartyService->IsInGameChat();
	}

	virtual bool IsLiveStreaming() const override
	{
		return GamePartyService->IsLiveStreaming();
	}

	virtual EChatMessageType::Type GetChatChannelType() const override
	{
		if ((ChatChannelFlags ^ EChatMessageType::Global) == 0)
		{
			return EChatMessageType::Global;
		}
		if ((ChatChannelFlags ^ EChatMessageType::Whisper) == 0)
		{
			return EChatMessageType::Whisper;
		}
		if ((ChatChannelFlags ^ EChatMessageType::Party) == 0)
		{
			return EChatMessageType::Party;
		}
		if ((ChatChannelFlags ^ EChatMessageType::Game) == 0)
		{
			return EChatMessageType::Game;
		}
		if ((ChatChannelFlags ^ EChatMessageType::Team) == 0)
		{
			return EChatMessageType::Team;
		}
		if ((ChatChannelFlags ^ EChatMessageType::Combined) == 0)
		{
			return EChatMessageType::Combined;
		}
		if ((ChatChannelFlags ^ EChatMessageType::Empty) == 0)
		{
			return EChatMessageType::Empty;
		}
		return EChatMessageType::Custom;
	}

	virtual EChatMessageType::Type GetDefaultChannelType() const override
	{
		return DefaultChannel;
	}

	virtual bool DisplayChatOption(TSharedRef<FFriendViewModel> FriendViewModel) override
	{
		return GetOutgoingChatChannel() != EChatMessageType::Whisper || !SelectedFriend.IsValid() || SelectedFriend->ViewModel->GetFriendItem() != FriendViewModel->GetFriendItem();
	}

	virtual bool IsOverrideDisplaySet() override
	{
		return OverrideDisplayVisibility;
	}

	virtual bool IsGlobalEnabled() override
	{
		return !ChatDisplayService->HideChatChrome();
	}

	// End FChatViewModel interface

	/*
	 * Set Selected Friend that we want outgoing messages to whisper to
	 * @param Username Friend Name
	 * @param UniqueID Friends Network ID
	 */
	void SetWhisperFriend(const FText Username, TSharedPtr<const FUniqueNetId> UniqueID)
	{
		TSharedPtr<FSelectedFriend> NewFriend = FindFriend(UniqueID);
		if (!NewFriend.IsValid())
		{
			NewFriend = MakeShareable(new FSelectedFriend());
			NewFriend->DisplayName = Username;
			NewFriend->UserID = UniqueID;
			SetWhisperFriend(NewFriend);
		}
	}

	/*
	 * Get Selected whisper friend
	 * @return FSelectedFriend item
	 */
	const TSharedPtr<FSelectedFriend> GetWhisperFriend() const
	{
		return SelectedFriend;
	}

protected:

	/*
	* Filter received message to see if this chat should display it
	* @param Message The message to filter
	* @return True if passed filter check
	*/
	virtual bool FilterMessage(const TSharedRef<FFriendChatMessage>& Message)
	{
		bool Changed = false;

		// Set outgoing whisper recipient if not set
		if (GetDefaultChannelType() == EChatMessageType::Whisper && Message->MessageType == EChatMessageType::Whisper && !SelectedFriend.IsValid())
		{
			if (Message->bIsFromSelf)
			{
				SetWhisperFriend(Message->ToName, Message->RecipientId);
			}
			else
			{ 
				SetWhisperFriend(Message->FromName, Message->SenderId);
			}
		}

		// Always show an outgoing message on the channel it was sent from
		if (Message->bIsFromSelf && IsActive())
		{
			AggregateMessage(Message);
			Changed = true;
		}
		else if (IsChannelSet(Message->MessageType))
		{
			AggregateMessage(Message);
			Changed = true;
		}
		return Changed;
	}

	/*
	 * Add Message to chat
	 * @param Message The message to filter
	 */
	void AggregateMessage(const TSharedRef<FFriendChatMessage>& Message)
	{
		FilteredMessages.Add(FChatItemViewModelFactory::Create(Message));
	}

	/*
	 * Check for new messages
	 */
	void RefreshMessages() override
	{
		const TArray<TSharedRef<FFriendChatMessage>>& Messages = MessageService->GetMessages();
		FilteredMessages.Empty();

		bool AddedItem = false;
		for (const auto& Message : Messages)
		{
			AddedItem |= FilterMessage(Message);
		}

		if (AddedItem)
		{
			OnChatListUpdated().Broadcast();
		}
	}

private:	

	/*
	 * Find a friend
	 * @param UniqueID Friends Network ID
	 * @return FSelectedFriend item
	 */
	TSharedPtr<FSelectedFriend> FindFriend(TSharedPtr<const FUniqueNetId> UniqueID)
	{
		// ToDo - Make this nicer NickD
		for( const auto& ExistingFriend : RecentPlayerList)
		{
			if(ExistingFriend->UserID == UniqueID)
			{
				return	ExistingFriend;
			}
		}
		return nullptr;
	}

	/*
	 * Deal with new message arriving
	 * @param NewMessage Message
	 */
	void HandleMessageAdded(const TSharedRef<FFriendChatMessage> NewMessage)
	{
		if (FilterMessage(NewMessage))
		{
			OnChatListUpdated().Broadcast();
		}
	}

	void HandleScrollToEnd()
	{
		OnScrollToEnd().Broadcast();
	}

	void HandleChatFriendSelected(TSharedRef<IFriendItem> ChatFriend)
	{
		if(GetOutgoingChatChannel() == EChatMessageType::Whisper)
		{
			SetWhisperFriend(FText::FromString(ChatFriend->GetName()), ChatFriend->GetUniqueID());
			OnChatListUpdated().Broadcast();
		}
	}

public:

	// Begin ChatDisplayOptionsViewModel interface
	virtual void SetCaptureFocus(bool bInCaptureFocus) override
	{
		bCaptureFocus = bInCaptureFocus;
	}

	virtual void SetCurve(FCurveHandle InFadeCurve) override
	{
		FadeCurve = InFadeCurve;
	}

	virtual bool ShouldCaptureFocus() const override
	{
		return bCaptureFocus;
	}

	virtual float GetTimeTransparency() const override
	{
		return FadeCurve.GetLerp();
	}

	virtual bool IsFadeBackgroundEnabled() const override
	{
		return ChatDisplayService->IsFadeBackgroundEnabled();
	}

	virtual EVisibility GetChatListVisibility() const override
	{
		return ChatDisplayService->GetChatListVisibility();
	}

	virtual EVisibility GetChatMaximizeVisibility() const override
	{
		return ChatDisplayService->IsChatMinimizeEnabled() && ChatDisplayService->IsChatMinimized() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	virtual bool IsChatMinimized() const override
	{
		return ChatDisplayService->IsChatMinimized();
	}

	virtual void SetIsActive(bool InIsActive)
	{
		bIsActive = InIsActive;
		OverrideDisplayVisibility = true;

		// If active, ensure we have a valid chat channel
		if(bIsActive)
		{
			if(GetDefaultChannelType() == EChatMessageType::Party)
			{
				if (GamePartyService->IsInGameChat())
				{
					SetOutgoingMessageChannel(EChatMessageType::Game);
				}
				else if(GamePartyService->IsInPartyChat())
				{
					SetOutgoingMessageChannel(EChatMessageType::Party);
				}
			}
			else if(GetDefaultChannelType() == EChatMessageType::Combined)
			{
				SetOutgoingMessageChannel(EChatMessageType::Whisper);
			}
			else
			{
				// Reset the chat channel to default option when opened
				SetOutgoingMessageChannel(GetDefaultChannelType());
			}
			ChatDisplayService->SetActiveTab(SharedThis(this));

			OnScrollToEnd().Broadcast();
		}
	}

	virtual bool MultiChat() override
	{
		return true;
	}

	virtual void SetInteracted() override
	{
		ChatDisplayService->ChatEntered();
	}

	virtual float GetWindowOpacity() override
	{
		return WindowOpacity;
	}

	virtual bool IsActive() const override
	{
		return bIsActive;
	}

	virtual void SetChatSettingsService(TSharedPtr<IChatSettingsService> InChatSettingsService) override
	{
		ChatSettingsService = InChatSettingsService;
		bAllowFade = ChatSettingsService->GetFlagOption(EChatSettingsType::NeverFadeMessages);
		WindowOpacity = ChatSettingsService->GetSliderValue(EChatSettingsType::WindowOpacity);
		uint8 FontSize = ChatSettingsService->GetRadioOptionIndex(EChatSettingsType::FontSize);
//		FFriendsAndChatModuleStyle::GetStyleService()->SetUserFontSize(EChatFontType::Type(FontSize));
		ChatSettingsService->OnChatSettingStateUpdated().AddSP(this, &FChatViewModelImpl::HandleSettingsChanged);
		ChatSettingsService->OnChatSettingRadioStateUpdated().AddSP(this, &FChatViewModelImpl::HandleRadioSettingsChanged);
		ChatSettingsService->OnChatSettingValueUpdated().AddSP(this, &FChatViewModelImpl::HandleValueSettingsChanged);
	}

	virtual void HandleSettingsChanged(EChatSettingsType::Type OptionType, bool NewState)
	{
		if (OptionType == EChatSettingsType::NeverFadeMessages)
		{
			bAllowFade = !NewState;
		}
	}

	virtual void HandleRadioSettingsChanged(EChatSettingsType::Type OptionType, uint8 Index)
	{
		if (OptionType == EChatSettingsType::FontSize)
		{
//			FFriendsAndChatModuleStyle::GetStyleService()->SetUserFontSize(EChatFontType::Type(Index));
			OnSettingsUpdated().Broadcast();
		}
	}

	virtual void HandleValueSettingsChanged(EChatSettingsType::Type OptionType, float Value)
	{
		if (OptionType == EChatSettingsType::WindowOpacity)
		{
			WindowOpacity = Value;
		}
	}

	virtual TSharedPtr<class FChatTextLayoutMarshaller> GetRichTextMarshaller(const FFriendsAndChatStyle* Style, const struct FTextBlockStyle& TextStyle) override
	{
		TSharedRef< FSlateFontMeasure > FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		FDateTime TestDatetime = FDateTime(2000, 1, 1, 12, 55, 55, 55);
		const FString TimeString = FText::AsTime(TestDatetime, EDateTimeStyle::Short, "GMT").ToString();
		FVector2D TimestampMeasure = FontMeasure->Measure(TimeString, Style->FriendsChatStyle.TimeStampTextStyle.Font);
		const FString ShortTimestamp = TEXT("5 mins");
		FVector2D ShortTimestampMeasure = FontMeasure->Measure(ShortTimestamp, Style->FriendsChatStyle.TimeStampTextStyle.Font);

		TimestampMeasure.X = FMath::Max(TimestampMeasure.X, ShortTimestampMeasure.X);

		return FChatTextLayoutMarshallerFactory::Create(Style, TextStyle, MultiChat(), TimestampMeasure, ChatDisplayService);
	}

	DECLARE_DERIVED_EVENT(FChatViewModelImpl, FChatViewModel::FChatListUpdated, FChatListUpdated)
	virtual FChatListUpdated& OnChatListUpdated() override
	{
		return ChatListUpdatedEvent;
	}

	DECLARE_DERIVED_EVENT(FChatViewModelImpl, FChatViewModel::FChatSettingsUpdated, FChatSettingsUpdated)
	virtual FChatSettingsUpdated& OnSettingsUpdated() override
	{
		return ChatSettingsUpdatedEvent;
	}

	DECLARE_DERIVED_EVENT(FChatViewModelImpl, FChatViewModel::FScrollToEnd, FScrollToEnd)
	virtual FScrollToEnd& OnScrollToEnd() override
	{
		return ScrollToEndEvent;
	}

	// End ChatDisplayOptionsViewModel Interface

private:

	void HandleChatLisUpdated()
	{
		OnChatListUpdated().Broadcast();
	}

	void SetFilteredMessages(TArray<TSharedRef<FChatItemViewModel> > InFilteredMessages)
	{
		FilteredMessages = InFilteredMessages;
	}

protected:

	FChatViewModelImpl(
		const TSharedRef<IFriendViewModelFactory>& InFriendViewModelFactory,
		const TSharedRef<FMessageService>& InMessageService,
		const TSharedRef<FFriendsNavigationService>& InNavigationService,
		const TSharedRef<FChatDisplayService>& InChatDisplayService,
		const TSharedRef<FFriendsService>& InFriendsService,
		const TSharedRef<FGameAndPartyService>& InGamePartyService)
		: ChatChannelFlags(0)
		, DefaultChannel(EChatMessageType::Custom)
		, DefaultChatChannelFlags(0)
		, OutgoingMessageChannel(EChatMessageType::Custom)
		, FriendViewModelFactory(InFriendViewModelFactory)
		, MessageService(InMessageService)
		, NavigationService(InNavigationService)
		, ChatDisplayService(InChatDisplayService)
		, FriendsService(InFriendsService)
		, GamePartyService(InGamePartyService)
		, bIsActive(false)
		, bInGame(true)
		, bInParty(false)
		, bHasActionPending(false)
		, bAllowJoinGame(false)
		, OverrideDisplayVisibility(false)
		, bAllowFade(true)
		, bUseOverrideColor(false)
		, bAllowGlobalChat(true)
		, bCaptureFocus(false)
		, bDisplayChatTip(false)
		, WindowOpacity(1.0f)
	{
	}

	void Initialize(bool bBindNavService)
	{
		if (bBindNavService)
		{
			NavigationService->OnChatFriendSelected().AddSP(this, &FChatViewModelImpl::HandleChatFriendSelected);
		}
		MessageService->OnChatMessageAdded().AddSP(this, &FChatViewModelImpl::HandleMessageAdded);
		ChatDisplayService->OnChatMessageCommitted().AddSP(this, &FChatViewModelImpl::HandleScrollToEnd);
		ChatDisplayService->OnChatMinimized().AddSP(this, &FChatViewModelImpl::HandleScrollToEnd);
		RefreshMessages();
	}

protected:

	// The Channels we are subscribed too
	uint16 ChatChannelFlags;

	// The Channel we default too when resetting;
	EChatMessageType::Type DefaultChannel;
	uint16 DefaultChatChannelFlags;

	// The Current outgoing channel we are sending messages on
	EChatMessageType::Type OutgoingMessageChannel;

	TSharedRef<IFriendViewModelFactory> FriendViewModelFactory;
	TSharedRef<FMessageService> MessageService;
	TSharedRef<FFriendsNavigationService> NavigationService;
	TSharedRef<FChatDisplayService> ChatDisplayService;	
	TSharedRef<FFriendsService> FriendsService;
	TSharedRef<FGameAndPartyService> GamePartyService;
	TSharedPtr<IChatSettingsService> ChatSettingsService;

	bool bIsActive;
	bool bInGame;
	bool bInParty;
	bool bHasActionPending;
	bool bAllowJoinGame;
	bool OverrideDisplayVisibility;

	TArray<TSharedRef<FChatItemViewModel> > FilteredMessages;
	TArray<TSharedPtr<FSelectedFriend> > RecentPlayerList;
	TSharedPtr<FSelectedFriend> SelectedFriend;

	// Display Options
	bool bAllowFade;
	bool bUseOverrideColor;
	bool bAllowGlobalChat;
	bool bCaptureFocus;
	bool bDisplayChatTip;
	float WindowOpacity;

	FText OutGoingChannelText;
	// Holds the fade in curve
	FCurveHandle FadeCurve;
	FChatListUpdated ChatListUpdatedEvent;
	FChatSettingsUpdated ChatSettingsUpdatedEvent;
	FScrollToEnd ScrollToEndEvent;

	EVisibility ChatEntryVisibility;
	FSlateColor OverrideColor;

private:
	friend FChatViewModelFactory;
};


// Windowed Mode Impl
class FWindowedChatViewModelImpl : public FChatViewModelImpl
{
private:

	FWindowedChatViewModelImpl(
			const TSharedRef<IFriendViewModelFactory>& FriendViewModelFactory, 
			const TSharedRef<FMessageService>& MessageService,
			const TSharedRef<FFriendsNavigationService>& NavigationService,
			const TSharedRef<FChatDisplayService>& ChatDisplayService,
			const TSharedRef<FFriendsService>& FriendsService,
			const TSharedRef<FGameAndPartyService>& GamePartyService)
		: FChatViewModelImpl(FriendViewModelFactory, MessageService, NavigationService, ChatDisplayService, FriendsService, GamePartyService)
	{
	}

	// Being FChatViewModelImpl 
	virtual bool FilterMessage(const TSharedRef<FFriendChatMessage>& Message) override
	{
		const TSharedPtr<FSelectedFriend> WhisperFriend = GetWhisperFriend();
		bool Changed = false;

		// Always show an outgoing message on the channel it was sent from
		if (Message->bIsFromSelf)
		{
			AggregateMessage(Message);
			Changed = true;
		}
		else if (GetChatChannelType() == EChatMessageType::Whisper && WhisperFriend.IsValid())
		{
			// Make sure whisper is for this window
			if ((Message->bIsFromSelf && Message->RecipientId == WhisperFriend->UserID) || Message->SenderId == WhisperFriend->UserID)
			{
				AggregateMessage(Message);
				Changed = true;
			}
		}
		else
		{
			if (IsChannelSet(Message->MessageType))
			{
				AggregateMessage(Message);
				Changed = true;
			}
		}
		return Changed;
	}

	virtual bool MultiChat() override
	{
		return false;
	}
	
	// End FChatViewModelImpl 


	friend FChatViewModelFactory;
};

// Factory
TSharedRef< FChatViewModel > FChatViewModelFactory::Create(
	const TSharedRef<IFriendViewModelFactory>& FriendViewModelFactory,
	const TSharedRef<FMessageService>& MessageService,
	const TSharedRef<FFriendsNavigationService>& NavigationService,
	const TSharedRef<FChatDisplayService>& ChatDisplayService,
	const TSharedRef<FFriendsService>& FriendsService,
	const TSharedRef<FGameAndPartyService>& GamePartyService,
	EChatViewModelType::Type ChatType)
{
	if (ChatType == EChatViewModelType::Windowed)
	{
		TSharedRef< FWindowedChatViewModelImpl > ViewModel(new FWindowedChatViewModelImpl(FriendViewModelFactory, MessageService, NavigationService, ChatDisplayService, FriendsService, GamePartyService));
		ViewModel->Initialize(false);
		return ViewModel;
	}
	TSharedRef< FChatViewModelImpl > ViewModel(new FChatViewModelImpl(FriendViewModelFactory, MessageService, NavigationService, ChatDisplayService, FriendsService, GamePartyService));
	ViewModel->Initialize(true);
	return ViewModel;
}

#undef LOCTEXT_NAMESPACE