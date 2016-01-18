// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FriendViewModel.h"

class FChatItemViewModel;
namespace EChatMessageType
{
	enum Type : uint16;
}

namespace EChatViewModelType
{
	enum Type : uint8
	{
		Base = 0,
		Windowed
	};
}


struct FSelectedFriend
{
	TSharedPtr<const FUniqueNetId> UserID;
	FText DisplayName;
	EChatMessageType::Type MessageType;
	TSharedPtr<FFriendViewModel> ViewModel;
	TSharedPtr<FChatItemViewModel> SelectedMessage;
};

class FChatViewModel
	:public TSharedFromThis<FChatViewModel>
{
public:

	virtual ~FChatViewModel() {}

	// Clone the view model. Used for Chrome tabs. Keeps existing chat messages, but adds new Display service
	virtual TSharedRef<FChatViewModel> Clone(const TSharedRef<class FChatDisplayService>& InChatDisplayService) = 0;

	virtual FText GetChatGroupText(bool ShowWhisperFriendsName = true) const = 0;

	// Message
	virtual TArray<TSharedRef<FChatItemViewModel > >& GetMessages() = 0;
	virtual int32 GetMessageCount() const = 0;
	virtual bool HasDefaultChannelMessage() const = 0;

	// Friend Actions
	virtual TSharedPtr<FFriendViewModel> GetFriendViewModel(const TSharedPtr<const FUniqueNetId> UniqueID, const FText Username) = 0;
	virtual TSharedPtr<FFriendViewModel> GetFriendViewModel(const FString InUserID, const FText Username) = 0;
	
	// Channel
	virtual void SetChannelFlags(uint16 ChannelFlags) = 0;
	virtual uint16 GetChannelFlags() = 0;
	virtual void SetDefaultChannelFlags(uint16 ChannelFlags) = 0;
	virtual void SetDefaultOutgoingChannel(EChatMessageType::Type InChannel) = 0;
	virtual void ResetToDefaultChannel() = 0;
	virtual bool IsChannelSet(const EChatMessageType::Type InChannel) = 0;
	virtual void ToggleChannel(const EChatMessageType::Type InChannel) = 0;
	virtual void SetOutgoingMessageChannel(const EChatMessageType::Type InChannel) = 0;
	virtual void NavigateToChannel(const EChatMessageType::Type InChannel) = 0;
	virtual EChatMessageType::Type GetOutgoingChatChannel() const = 0;
	virtual FText GetOutgoingChannelText() const = 0;
	virtual FText GetChannelErrorText() const = 0;
	virtual void SetWhisperFriend(const TSharedPtr<FSelectedFriend> InFriend) = 0;
	virtual bool IsWhisperFriendSet() const = 0;
	virtual TSharedPtr<class FFriendViewModel> GetSelectedFriendViewModel() const = 0;

	virtual bool IsInPartyChat() const = 0;
	virtual bool IsInGameChat() const = 0;
	virtual bool IsInTeamChat() const = 0;
	virtual bool IsLiveStreaming() const = 0;
	virtual EChatMessageType::Type GetChatChannelType() const = 0;
	virtual EChatMessageType::Type GetDefaultChannelType() const = 0;
	virtual bool DisplayChatOption(TSharedRef<FFriendViewModel> FriendViewModel ) = 0;
	virtual bool IsOverrideDisplaySet() = 0;
	virtual bool IsGlobalEnabled() = 0;
	virtual bool IsActive() const = 0;

	// Display options
	virtual void SetCaptureFocus(bool bCaptureFocus) = 0;
	virtual void SetCurve(FCurveHandle InFadeCurve) = 0;
	virtual bool ShouldCaptureFocus() const = 0;
	virtual float GetTimeTransparency() const = 0;
	virtual bool IsFadeBackgroundEnabled() const = 0;
	virtual EVisibility GetChatListVisibility() const = 0;
	virtual EVisibility GetChatMaximizeVisibility() const = 0;
	virtual bool IsChatMinimized() const = 0;
	virtual void SetIsActive(bool IsActive) = 0;
	virtual void RefreshMessages() = 0;
	virtual bool MultiChat() = 0;
	virtual void SetInteracted() = 0;
	virtual float GetWindowOpacity() = 0;
	virtual TSharedPtr<class FChatTextLayoutMarshaller> GetRichTextMarshaller(const struct FFriendsAndChatStyle* Style, const struct FTextBlockStyle& TextStyle) = 0;

	// Settings
	virtual void SetChatSettingsService(TSharedPtr<IChatSettingsService> ChatSettingsService) = 0;

	DECLARE_EVENT(FChatViewModel, FChatListUpdated)
	virtual FChatListUpdated& OnChatListUpdated() = 0;

	DECLARE_EVENT(FChatViewModel, FChatSettingsUpdated)
	virtual FChatSettingsUpdated& OnSettingsUpdated() = 0;

	DECLARE_EVENT(FChatViewModel, FScrollToEnd)
	virtual FScrollToEnd& OnScrollToEnd() = 0;
};

/**
 * Creates the implementation for an ChatViewModel.
 *
 * @return the newly created ChatViewModel implementation.
 */
FACTORY(TSharedRef< FChatViewModel >, FChatViewModel,
	const TSharedRef<class IFriendViewModelFactory>& FriendViewModelFactory,
	const TSharedRef<class FMessageService>& MessageService,
	const TSharedRef<class FFriendsNavigationService>& NavigationService,
	const TSharedRef<class FChatDisplayService>& ChatDisplayService,
	const TSharedRef<class FFriendsService>& FriendsService,
	const TSharedRef<class FGameAndPartyService>& GamePartyService,
	EChatViewModelType::Type ChatType);