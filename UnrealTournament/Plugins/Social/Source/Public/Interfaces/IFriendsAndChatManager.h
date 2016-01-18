// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IFriendItem;
enum class ESendPartyInvitationCompletionResult;
namespace EChatMessageType
{
	enum Type : uint16;
}
namespace EOnlinePresenceState
{
	enum Type : uint8;
}

enum class ESendPartyInvitationCompletionResult;

class IChatDisplayService;
class IChatSettingsService;

/**
 * Interface for the Friends and chat manager.
 */
class IFriendsAndChatManager
{
public:

	/** Log out and kill the friends list window. */
	virtual void Logout() = 0;

	/** Log in and start checking for Friends. */
	virtual void Login(IOnlineSubsystem* InOnlineSub, bool bInIsGame = false, int32 LocalUserID = 0) = 0;

	/** Is the chat manager logged in. */
	virtual bool IsLoggedIn() = 0;

	/** Set the user to an online state. */
	virtual void SetOnline() = 0;

	/** Set the user to an away state. */
	virtual void SetAway() = 0;

	/** Set allow team chat in game. */
	virtual void SetAllowTeamChat() = 0;
	/**
	 * Set the override presence .
	 *
	 * @param PresenceID the override presence ID
	 */
	virtual void SetOverridePresence(const FString& PresenceID) = 0;

	/** Clear the override presence . */
	virtual void ClearOverridePresence() = 0;

	/**
	 * Create the chat Settings service used to customize chat
	 * @return The chat settings.
	 */
	virtual TSharedPtr<IChatSettingsService > GetChatSettingsService() = 0;

	/**
	 * Create the display servies used to customize chat widgets
	 * @return The display service.
	 */
	virtual TSharedRef< IChatDisplayService > GenerateChatDisplayService() = 0;

	/**
	 * Create the a chrome widget
	 * @param InStyle The style to use to create the widgets.
	 * @param InStyle The display service.
	 * @return The Chrome chat widget.
	 */
	virtual TSharedPtr< SWidget > GenerateChromeWidget(const struct FFriendsAndChatStyle* InStyle,
		TSharedRef<IChatDisplayService> ChatDisplayService,
		TSharedRef<IChatSettingsService> InChatSettingsService,
		TArray<TSharedRef<class ICustomSlashCommand> >* CustomSlashCommands = nullptr,
		bool CombineChatAndPartyChat = false,
		bool bDisplayGlobalChat = false) = 0;

	/**
	 * Set the analytics provider for capturing friends/chat events
	 *
	 * @param AnalyticsProvider the provider to use
	 */
	virtual void SetAnalyticsProvider(const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider) = 0;

	/**
	 * Create the a friends list widget without a container.
	 * @param InStyle The style to use to create the widgets.
	 * @return The Friends List widget.
	 */
	virtual TSharedPtr< SWidget > GenerateFriendsListWidget( const struct FFriendsAndChatStyle* InStyle ) = 0;

	/**
	 * Create a status widget for the current user.
	 * @param InStyle The style to use to create the widgets.
	 * @param ShowStatusOptions If we should display widget to change user status
	 * @return The Statuswidget.
	 */
	virtual TSharedPtr< SWidget > GenerateStatusWidget( const FFriendsAndChatStyle* InStyle, bool ShowStatusOptions) = 0;

	/**
	 * Generate a chat widget.
	 * @param InStyle The style to use to create the widgets.
	 * @param The hint that shows what key activates chat
	 * @param The type of chat widget to make
	 * @param The friend if its a whisper chat
	 * @param The window pointer for this widget, can be null
	 * @return The chat widget.
	 */
	virtual TSharedPtr< SWidget > GenerateChatWidget(const FFriendsAndChatStyle* InStyle, TAttribute<FText> ActivationHintDelegate, EChatMessageType::Type MessageType, TSharedPtr<IFriendItem> FriendItem, TSharedPtr< SWindow > WindowPtr) = 0;


	/**
	 * Generate a chat widget.
	 * @param The friend for this header
	 * @return The chat widget.
	 */
	virtual TSharedPtr<SWidget> GenerateFriendUserHeaderWidget(TSharedPtr<IFriendItem> FriendItem) = 0;

	/**
	 * Get the navigation service.
	 * @return The navigation service
	 */
	virtual TSharedPtr<class IFriendsNavigationService> GetNavigationService() = 0;

	/**
	* Get the notification service.
	* @return The notification service
	*/
	virtual TSharedPtr<class IChatNotificationService> GetNotificationService() = 0;

	/**
	* Get the Message service.
	* @return The message service
	*/
	virtual TSharedPtr<class IChatCommunicationService> GetCommunicationService() = 0;

	/**
	* Get the GameAndParty service.
	* @return The GameAndParty service
	*/
	virtual TSharedPtr<class IGameAndPartyService> GetGameAndPartyService() = 0;

	/**
	 * Insert a network chat message.
	 * @param InMessage The chat message.
	 */
	virtual void InsertNetworkChatMessage(const FString& InMessage) = 0;

	/**
	 * Insert an admin message.
	 * @param InMessage The message.
	 */
	virtual void InsertNetworkAdminMessage(const FString& InMessage) = 0;

	/**
	 * Insert a live stream message.
	 * @param InMessage The message.
	 */
	virtual void InsertLiveStreamMessage(const FString& InMessage) = 0;

	/**
	 * Join a global chat room
	 * @param RoomName The name of the room
	 */
	virtual void JoinGlobalChatRoom(const FString& RoomName) = 0;

	/**
	 * Delegate when the global chat room has been joined
	 */
	virtual void OnGlobalChatRoomJoined(const FString& ChatRoomID) = 0;
	

	/**
	 * Get the online status
	 *
	 * @return EOnlinePresenceState
	 */
	virtual EOnlinePresenceState::Type GetOnlineStatus() = 0;

	/** 
	 * Set the application view model to query and perform actions on.
	 * @param AppId The client service name of the application
	 * @param ApplicationViewModel The view model.
	 */
	virtual void AddApplicationViewModel(const FString AppId, TSharedPtr<class IFriendsApplicationViewModel> ApplicationViewModel) = 0;

	/**
	 * Add a namespace to use when querying for recent players
	 */
	virtual void AddRecentPlayerNamespace(const FString& Namespace) = 0;

	virtual void ClearApplicationViewModels() = 0;

	DECLARE_EVENT_FourParams(IFriendsAndChatManager, FOnSendPartyInvitationCompleteEvent, const FUniqueNetId& /*LocalUserId*/, const FOnlinePartyId& /*PartyId*/, const FUniqueNetId& /*RecipientId*/, ESendPartyInvitationCompletionResult /*Result*/);
	virtual FOnSendPartyInvitationCompleteEvent& OnSendPartyInvitationComplete() = 0;

	DECLARE_EVENT_FiveParams(IFriendsAndChatManager, FOnSendFriendRequestCompleteEvent, const int /* Local Player */, bool /* bWasSuccessful */, const FUniqueNetId& /* FriendId*/, const FString& /*ListName*/, const FString& /*ErrorStr*/);
	virtual FOnSendFriendRequestCompleteEvent& OnSendFriendRequestComplete() = 0;

	DECLARE_DELEGATE_RetVal(bool, FAllowFriendsJoinGame);
	virtual FAllowFriendsJoinGame& AllowFriendsJoinGame() = 0;

public:

	/** Virtual destructor. */
	virtual ~IFriendsAndChatManager() { }
};
