// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Result of find friend attempt
 */
namespace EFindFriendResult
{
	enum Type
	{
		Success,
		NotFound,
		AlreadyFriends,
		FriendsPending,
		AddingSelfFail
	};

	static const TCHAR* ToString(const EFindFriendResult::Type& Type)
	{
		switch (Type)
		{
		case Success:
			return TEXT("Success");
		case NotFound:
			return TEXT("NotFound");
		case AlreadyFriends:
			return TEXT("AlreadyFriends");
		case FriendsPending:
			return TEXT("FriendsPending");
		case AddingSelfFail:
			return TEXT("AddingSelfFail");
		default:
			return TEXT("");
		};
	}
};

class FFriendsAndChatOptions
{
public:
	bool Game;
	IOnlineSubsystem* ExternalOnlineSub;
};

/**
 * Implement the Friend and Chat manager
 */
class FFriendsAndChatManager
	: public IFriendsAndChatManager
	, public TSharedFromThis<FFriendsAndChatManager>
{
public:

	/** Default constructor. */
	FFriendsAndChatManager( );

	/** Destructor. */
	~FFriendsAndChatManager( );

	void Initialize(const FFriendsAndChatOptions& InOptions);

public:

	// IFriendsAndChatManager
	virtual void Logout() override;
	virtual void Login(IOnlineSubsystem* InOnlineSub, bool bInIsGame = false, int32 LocalUserID = 0) override;
	virtual bool IsLoggedIn() override;
	virtual void SetOnline() override;
	virtual void SetAway() override;
	virtual void SetOverridePresence(const FString& PresenceID) override;
	virtual void ClearOverridePresence() override;
	virtual void SetAllowTeamChat() override;
	virtual EOnlinePresenceState::Type GetOnlineStatus() override;
	virtual void AddApplicationViewModel(const FString AppId, TSharedPtr<IFriendsApplicationViewModel> ApplicationViewModel) override;
	virtual void AddRecentPlayerNamespace(const FString& Namespace) override;
	virtual void ClearApplicationViewModels() override;
	virtual TSharedRef< IChatDisplayService > GenerateChatDisplayService() override;
	virtual TSharedPtr< IChatSettingsService > GetChatSettingsService() override;
	virtual TSharedPtr< SWidget > GenerateChromeWidget(const struct FFriendsAndChatStyle* InStyle,
														TSharedRef<IChatDisplayService> ChatViewModel, 
														TSharedRef<IChatSettingsService> ChatSettingsService, 
														TArray<TSharedRef<class ICustomSlashCommand> >* CustomSlashCommands = nullptr,
														bool CombineChatAndPartyChat = false,
														bool bDisplayGlobalChat = false) override;
	virtual void SetAnalyticsProvider(const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider) override;
	virtual TSharedPtr< SWidget > GenerateFriendsListWidget( const FFriendsAndChatStyle* InStyle ) override;
	virtual TSharedPtr< SWidget > GenerateStatusWidget(const FFriendsAndChatStyle* InStyle, bool ShowStatusOptions) override;
	virtual TSharedPtr< SWidget > GenerateChatWidget(const FFriendsAndChatStyle* InStyle, TAttribute<FText> ActivationHintDelegate, EChatMessageType::Type MessageType, TSharedPtr<IFriendItem> FriendItem, TSharedPtr< SWindow > WindowPtr) override;
	virtual TSharedPtr<SWidget> GenerateFriendUserHeaderWidget(TSharedPtr<IFriendItem> FriendItem) override;
	virtual TSharedPtr<class IFriendsNavigationService> GetNavigationService() override;
	virtual TSharedPtr<class IChatNotificationService> GetNotificationService() override;
	virtual TSharedPtr<class IChatCommunicationService> GetCommunicationService() override;
	virtual TSharedPtr<class IGameAndPartyService> GetGameAndPartyService() override;
	virtual void InsertNetworkChatMessage(const FString& InMessage) override;
	virtual void InsertNetworkAdminMessage(const FString& InMessage) override;
	virtual void InsertLiveStreamMessage(const FString& InMessage) override;
	virtual void JoinGlobalChatRoom(const FString& RoomName) override;
	virtual void OnGlobalChatRoomJoined(const FString& ChatRoomID) override;

	// External events
	DECLARE_DERIVED_EVENT(IFriendsAndChatManager, IFriendsAndChatManager::FOnSendPartyInvitationCompleteEvent, FOnSendPartyInvitationCompleteEvent);
	virtual FOnSendPartyInvitationCompleteEvent& OnSendPartyInvitationComplete() override
	{
		return SendPartyInviteCompleteEvent;
	}

	DECLARE_DERIVED_EVENT(IFriendsAndChatManager, IFriendsAndChatManager::FOnSendFriendRequestCompleteEvent, FOnSendFriendRequestCompleteEvent);
	virtual FOnSendFriendRequestCompleteEvent& OnSendFriendRequestComplete() override
	{
		return SendFriendRequestCompleteEvent;
	}

	virtual FAllowFriendsJoinGame& AllowFriendsJoinGame() override
	{
		return AllowFriendsJoinGameDelegate;
	}

private:

	/** Called when singleton is released. */
	void ShutdownManager();

	/**
	 * Notification when an invite list has changed for a party
 	 * @param LocalUserId - user that is associated with this notification
	 */
	void OnPartyInvitesChanged(const FUniqueNetId& LocalUserId);

	/**
	 * Delegate used when an party invite is sent
	 * 
	 * @param LocalUserId	The user ID.
	 * @param PartyId		Party ID.
	 * @param Result Result of send invite action.
	 * @param RecipientId	The friend ID.
	 */
	void OnSendPartyInvitationCompleteInternal(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& RecipientId, const ESendPartyInvitationCompletionResult Result);

	/**
	 * A ticker used to perform updates on the main thread.
	 * @param Delta The tick delta.
	 * @return true to continue ticking.
	 */
	bool Tick( float Delta );

	/** Add a friend toast. */
	void AddFriendsToast(const FText Message);

	/**
	 * Get the best spawn position for the next chat window
	 *
	 * @return Spawn position
	 */
	FVector2D GetWindowSpawnPosition() const;

private:

	// Holds the list messages sent out to be responded to
	TArray<TSharedPtr< FFriendsAndChatMessage > > NotificationMessages;

	// Holds the external online subsystem
	IOnlineSubsystem* ExternalOnlineSubsystem;

	/* Delegates
	*****************************************************************************/
	// Holds the On send party invite complete delegate
	FOnSendPartyInvitationCompleteEvent SendPartyInviteCompleteEvent;
	// Holds the on send friend request complete delegate
	FOnSendFriendRequestCompleteEvent SendFriendRequestCompleteEvent;
	FAllowFriendsJoinGame AllowFriendsJoinGameDelegate;

	/* Services
	*****************************************************************************/
	// Manages private/public chat messages 
	TSharedPtr<class FMessageService> MessageService;
	// Manages navigation services
	TSharedPtr<class FFriendsNavigationService> NavigationService;
	// Manages notification services
	TSharedPtr<class FChatNotificationService> NotificationService;
	// Holds the friends view model facotry
	TSharedPtr<class IFriendViewModelFactory> FriendViewModelFactory;
	// Holds the friends list provider
	TSharedPtr<class IFriendListFactory> FriendsListFactory;
	// Holds the Markup service provider
	TSharedPtr<class IFriendsChatMarkupServiceFactory> MarkupServiceFactory;
	// Holds the Game and Party service
	TSharedPtr<class FGameAndPartyService> GameAndPartyService;
	// Holds the friends service
	TSharedPtr<class FFriendsService> FriendsService;
	// Holds the OSSScheduler 
	TSharedPtr<class FOSSScheduler> OSSScheduler;
	// Holds the Web Image Service
	TSharedPtr<class FWebImageService> WebImageService;
	// Holds the style service
	TSharedPtr<class FFriendsFontStyleService> FontService;
	// Holds the chat settings service
	TSharedPtr<class FChatSettingsService> SettingsService;


	/* Chat stuff
	*****************************************************************************/

	// Keeps track of global chat rooms that have been requested to join
	TArray<FString> ChatRoomstoJoin;
	// Holds the clan repository
	TSharedPtr<class IClanRepository> ClanRepository;
	// Allow team chat
	bool bAllowTeamChat;

	/* Manger state
	*****************************************************************************/

	// Holds the Friends List widget
	TSharedPtr< SWidget > FriendListWidget;
	
	// Holds the style used to create the Friends List widget
	FFriendsAndChatStyle Style;
	// Holds the toast notification
	TSharedPtr<SNotificationList> FriendsNotificationBox;
	// Holds the last created chrome view model
	TSharedPtr<class FChatChromeViewModel> CachedViewModel;

private:

	TSharedPtr<class FFriendsAndChatAnalytics> Analytics;
};
