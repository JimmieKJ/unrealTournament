// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


// Enum holding the state of the Friends manager
namespace EFriendsAndManagerState
{
	enum Type
	{
		Idle,								// Idle - can accept requests
		RequestingFriendsList,				// Requesting a list refresh
		RequestFriendsListRefresh,			// List request in progress
		RequestingRecentPlayersIDs,			// Requesting recent player ids
		RequestRecentPlayersListRefresh,	// Recent players request in progress
		RequestGameInviteRefresh,			// Game invites can be from non-friends. refresh non-friend user info
		ProcessFriendsList,					// Process the Friends List after a list refresh
		RequestingFriendName,				// Requesting a friend add
		DeletingFriends,					// Deleting a friend
		AcceptingFriendRequest,				// Accepting a friend request
		OffLine,							// No logged in
	};
};

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

// Analytics

class FFriendsAndChatAnalytics
{
public:
	FFriendsAndChatAnalytics() {}
	/**
	 * Update provider to use for capturing events
	 */
	void SetProvider(const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider) {	Provider = AnalyticsProvider; }
	/**
	 * Record a game invite event
 	 */
	void RecordGameInvite(const FUniqueNetId& ToUser, const FString& EventStr) const;
	/**
	 * Record a friend action event
	 */
	void RecordFriendAction(const IFriendItem& Friend, const FString& EventStr) const;
	/**
	 * Record a add friend action event
	 */
	void RecordAddFriend(const FString& FriendName, const FUniqueNetId& FriendId, EFindFriendResult::Type Result, bool bRecentPlayer, const FString& EventStr) const;
	/**
	 * Record chat option toggle
	 */
	void RecordToggleChat(const FString& Channel, bool bEnabled, const FString& EventStr) const;
	/**
	 * Record a private chat to a user (aggregates pending FlushChat)
	 */
	void RecordPrivateChat(const FString& ToUser);
	/**
	 * Record a public chat to a channel (aggregates pending FlushChat)
	 */
	void RecordChannelChat(const FString& ToChannel);
	/**
	 * Flush any aggregated chat stats
	 */
	void FlushChatStats();

private:
	void AddPresenceAttributes(const FUniqueNetId& UserId, TArray<FAnalyticsEventAttribute>& Attributes) const;

	// cached analytics provider for pushing events
	TSharedPtr<IAnalyticsProvider> Provider;
	// map of chat id to # of messages sent
	TMap<FString, int32> PrivateChatCounts;
	TMap<FString, int32> ChannelChatCounts;
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

public:

	// IFriendsAndChatManager
	virtual void Logout() override;
	virtual void Login() override;
	virtual bool IsLoggedIn() override;
	virtual void SetOnline() override;
	virtual void SetAway() override;
	virtual EOnlinePresenceState::Type GetOnlineStatus() override;
	virtual void AddApplicationViewModel(const FString ClientID, TSharedPtr<IFriendsApplicationViewModel> ApplicationViewModel) override;
	virtual void CreateFriendsListWindow(const FFriendsAndChatStyle* InStyle) override;
	virtual void CreateChatWindow(const struct FFriendsAndChatStyle* InStyle, EChatMessageType::Type ChatType, TSharedPtr<IFriendItem> FriendItem, bool BringToFront = false) override;
	virtual void OpenWhisperChatWindows(const FFriendsAndChatStyle* InStyle) override;
	virtual void SetUserSettings(const FFriendsAndChatSettings& UserSettings) override;
	virtual void SetAnalyticsProvider(const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider) override;
	virtual TSharedPtr< SWidget > GenerateFriendsListWidget( const FFriendsAndChatStyle* InStyle ) override;
	virtual TSharedPtr< SWidget > GenerateStatusWidget( const FFriendsAndChatStyle* InStyle ) override;
	virtual TSharedPtr< SWidget > GenerateChatWidget(const FFriendsAndChatStyle* InStyle, TSharedRef<IChatViewModel> ViewModel, TAttribute<FText> ActivationHintDelegate) override;
	virtual TSharedPtr<IChatViewModel> GetChatViewModel() override;
	virtual void InsertNetworkChatMessage(const FString& InMessage) override;
	virtual void JoinPublicChatRoom(const FString& RoomName) override;
	virtual void OnChatPublicRoomJoined(const FString& ChatRoomID) override;

	/**
	 * Get the analytics for recording friends chat events
	 */
	FFriendsAndChatAnalytics& GetAnalytics()
	{
		return Analytics;
	}

	/**
	 * Get session id if the current player is in a session.
	 *
	 * @return True id of game session we are in.
	 */
	TSharedPtr<FUniqueNetId> GetGameSessionId() const;

	/**
	 * Get if the current player is in a session.
	 *
	 * @return True if we are in a game session.
	 */
	bool IsInGameSession() const;

	/**
	 * Is this friend in the same session as I am
	 *
	 * @param reference to a friend item
	 */
	bool IsFriendInSameSession( const TSharedPtr< const IFriendItem >& FriendItem ) const;

	/**
	 * Get if the current player is in a session and that game is joinable.
	 * The context here is from the local perspective, it should not be interpreted by or given to external recipients
	 * (ie, the game is invite only and therefore joinable via invite, but it is not "joinable" from another user's perspective)
	 *
	 * @return true if the local user considers their game joinable, otherwise false
	 */
	bool IsInJoinableGameSession() const;

	/**
	 * @param ClientID ID of the Game we want to join
	 * @return true if joining a game is allowed
	 */
	bool JoinGameAllowed(FString ClientID);

	/**
	 * @return true if in the launcher
	 */
	const bool IsInLauncher() const;

	/** 
	 * @return if we are logged into global chat
	 */
	bool IsInGlobalChat() const;

	/**
	 * Set the chat friend.
	 *
	 * @param FriendItem The friend to start a chat with.
	 */
	void SetChatFriend( TSharedPtr< IFriendItem > FriendItem );

	/**
	 * Open the global chat window
	 */
	void OpenGlobalChat();

	/**
	 * Set the chat widget contents.
	 * @param Window		 The Window to set conent on
	 * @param FriendItem	 The Friend if its a whisper window
	 */
	void SetChatWindowContents(TSharedPtr<SWindow> Window, TSharedPtr< IFriendItem > FriendItem, const FFriendsAndChatStyle& FriendStyle);

	/**
	 * Accept a friend request.
	 *
	 * @param FriendItem The friend to accept.
	 */
	void AcceptFriend( TSharedPtr< IFriendItem > FriendItem );

	/**
	 * Reject a friend request.
	 *
	 * @param FriendItem The friend to reject.
	 */
	void RejectFriend( TSharedPtr< IFriendItem > FriendItem );

	/**
	 * Get the friends filtered list of friends.
	 *
	 * @param OutFriendsList  Array of friends to fill in.
	 * @return the friend list count.
	 */
	int32 GetFilteredFriendsList( TArray< TSharedPtr< IFriendItem > >& OutFriendsList );

	/**
	 * Get the recent players list.
	 * @return the list.
	 */
	TArray< TSharedPtr< IFriendItem > >& GetRecentPlayerList();

	/**
	 * Get outgoing request list.
	 *
	 * @param OutFriendsList  Array of friends to fill in.
	 * @return The friend list count.
	 */
	int32 GetFilteredOutgoingFriendsList( TArray< TSharedPtr< IFriendItem > >& OutFriendsList );

	/**
	 * Request a friend be added.
	 *
	 * @param FriendName The friend name.
	 */
	void RequestFriend( const FText& FriendName );

	/**
	 * Request a friend to be deleted.
	 *
	 * @param FriendItem The friend item to delete.
	 * @param DeleteReason The reason the friend is being deleted.
	 */
	void DeleteFriend(TSharedPtr< IFriendItem > FriendItem, const FString& Action);

	/**
	 * Get incoming game invite list.
	 *
	 * @param OutFriendsList  Array of friends to fill in.
	 * @return The friend list count.
	 */
	int32 GetFilteredGameInviteList(TArray< TSharedPtr< IFriendItem > >& OutFriendsList);

	/**
	 * Reject a game invite
	 *
	 * @param FriendItem friend with game invite info
	 */
	void RejectGameInvite(const TSharedPtr<IFriendItem>& FriendItem);

	/**
	 * Accept a game invite
	 *
	 * @param FriendItem friend with game invite info
	 */
	void AcceptGameInvite(const TSharedPtr<IFriendItem>& FriendItem);

	/**
	 * Send a game invite to a friend
	 *
	 * @param FriendItem friend to send game invite to
	 */
	void SendGameInvite(const TSharedPtr<IFriendItem>& FriendItem);

	/**
	* Send a game invite to a user by id
	*
	* @param UserId user to send game invite to
	*/
	void SendGameInvite(const FUniqueNetId& ToUser);

	/** Send a game invite notification. */
	void SendGameInviteNotification(const TSharedPtr<IFriendItem>& FriendItem);

	/** 
	 * Broadcast when a chat message is received - opens the chat window in the launcher. \
	 *
	 * @param ChatType	The type of chat message received
	 * @param FriendItem The friend item if this chat type is whisper
	 */
	void SendChatMessageReceivedEvent(EChatMessageType::Type ChatType, TSharedPtr<IFriendItem> FriendItem);

	/**
	 * Find a user ID.
	 *
	 * @param InUserName The user name to find.
	 * @return The unique ID.
	 */
	TSharedPtr< FUniqueNetId > FindUserID( const FString& InUsername );

	/**
	 * Set the user online status
	 *
	 * @param OnlineState - the online state
	 */
	void SetUserIsOnline(EOnlinePresenceState::Type OnlineState);

	/**
	* Get the owner's client id
	*
	* @return client id string (or an empty string if it fails)
	*/
	FString GetUserClientId() const;

	/**
	* Get the owner's display name
	*
	* @return display/nickname string (or an empty string if it fails)
	*/
	FString GetUserNickname() const;

	/**
	 * Find a recent player.
	 *
	 * @param InUserId The user id to find.
	 * @return The recent player ID.
	 */
	TSharedPtr< IFriendItem > FindRecentPlayer(const FUniqueNetId& InUserID);

	/**
	 * Find a user.
	 *
	 * @param InUserName The user name to find.
	 * @return The Friend ID.
	 */
	TSharedPtr< IFriendItem > FindUser(const FUniqueNetId& InUserID);
	TSharedPtr< IFriendItem > FindUser(const TSharedRef<FUniqueNetId>& InUserID);

	TSharedPtr<class FFriendViewModel> GetFriendViewModel(const FUniqueNetId& InUserID);

	// External events
	DECLARE_DERIVED_EVENT(FFriendsAndChatManager, IFriendsAndChatManager::FOnFriendsNotificationEvent, FOnFriendsNotificationEvent)
	virtual FOnFriendsNotificationEvent& OnFriendsNotification() override
	{
		return FriendsListNotificationDelegate;
	}

	DECLARE_DERIVED_EVENT(FFriendsAndChatManager, IFriendsAndChatManager::FOnFriendsNotificationActionEvent, FOnFriendsNotificationActionEvent)
	virtual FOnFriendsNotificationActionEvent& OnFriendsActionNotification() override
	{
		return FriendsListActionNotificationDelegate;
	}

	DECLARE_DERIVED_EVENT(IFriendsAndChatManager, IFriendsAndChatManager::FOnFriendsUserSettingsUpdatedEvent, FOnFriendsUserSettingsUpdatedEvent)
	virtual FOnFriendsUserSettingsUpdatedEvent& OnFriendsUserSettingsUpdated() override
	{
		return FriendsUserSettingsUpdatedDelegate;
	}

	DECLARE_DERIVED_EVENT(IFriendsAndChatManager, IFriendsAndChatManager::FOnFriendsJoinGameEvent, FOnFriendsJoinGameEvent)
	virtual FOnFriendsJoinGameEvent& OnFriendsJoinGame() override
	{
		return FriendsJoinGameEvent;
	}

	DECLARE_DERIVED_EVENT(IFriendsAndChatManager, IFriendsAndChatManager::FChatMessageReceivedEvent, FChatMessageReceivedEvent);
	virtual FChatMessageReceivedEvent& OnChatMessageRecieved() override
	{
		return ChatMessageReceivedEvent;
	}

	virtual FAllowFriendsJoinGame& AllowFriendsJoinGame() override
	{
		return AllowFriendsJoinGameDelegate;
	}

	// Internal events

	DECLARE_EVENT(FFriendsAndChatManager, FOnFriendsUpdated)
	virtual FOnFriendsUpdated& OnFriendsListUpdated()
	{
		return OnFriendsListUpdatedDelegate;
	}

	DECLARE_EVENT(FFriendsAndChatManager, FOnGameInvitesUpdated)
	virtual FOnGameInvitesUpdated& OnGameInvitesUpdated()
	{
		return OnGameInvitesUpdatedDelegate;
	}

	DECLARE_EVENT_OneParam(FFriendsAndChatManager, FOnChatFriendSelected, TSharedPtr<IFriendItem> /*ChatFriend*/)
	virtual FOnChatFriendSelected& OnChatFriendSelected()
	{
		return OnChatFriendSelectedDelegate;
	}

private:

	/** Process the friends list to send out invites */
	void SendFriendRequests();

	/**
	 * Request a list read
	 */
	void RequestListRefresh();

	/**
	 * Request recent player list
	 */
	void RequestRecentPlayersListRefresh();

	/**
	 * Pre process the friends list - find missing names etc
	 *
	 * @param ListName - the list name
	 */
	void PreProcessList(const FString& ListName);

	/** Refresh the data lists */
	void RefreshList();

	/** Build the friends UI. */
	void BuildFriendsUI(TSharedPtr< SWindow > WindowPtr);

	/**
	 * Set the manager state.
	 *
	 * @param NewState The new manager state.
	 */
	void SetState( EFriendsAndManagerState::Type NewState );

	/** Send a friend invite notification. */
	void SendFriendInviteNotification();

	/** Send a friend invite accepted notification. */
	void SendInviteAcceptedNotification(TSharedPtr< IFriendItem > Friend);

	/** Get Invite Notification Text */
	const FText GetInviteNotificationText(TSharedPtr< IFriendItem > Friend) const;

	/** Called when singleton is released. */
	void ShutdownManager();

	/**
	 * Delegate used when the friends read request has completed.
	 *
	 * @param LocalPlayer		The controller number of the associated user that made the request.
	 * @param bWasSuccessful	true if the async action completed without error, false if there was an error.
	 * @param ListName			Name of the friends list that was operated on.
	 * @param ErrorStr			String representing the error condition.
	 */
	void OnReadFriendsListComplete( int32 LocalPlayer, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr );

	/**
	 * Delegate used when the query for recent players has completed
	 *
	 * @param UserId the id of the user that made the request
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 * @param Error string representing the error condition
	 */
	void OnQueryRecentPlayersComplete(const FUniqueNetId& UserId, bool bWasSuccessful, const FString& ErrorStr);

	/**
	 * Delegate used when an invite send request has completed.
	 *
	 * @param LocalPlayer		The controller number of the associated user that made the request.
	 * @param bWasSuccessful	true if the async action completed without error, false if there was an error.
	 * @param FriendId			Player that was invited.
	 * @param ListName			Name of the friends list that was operated on.
	 * @param ErrorStr			String representing the error condition.
	 */
	void OnSendInviteComplete( int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& FriendId, const FString& ListName, const FString& ErrorStr );

	/**
	 * Delegate used when an invite accept request has completed.
	 *
	 * @param LocalPlayer		The controller number of the associated user that made the request
	 * @param bWasSuccessful	true if the async action completed without error, false if there was an error
	 * @param FriendId			Player that invited us
	 * @param ListName			Name of the friends list that was operated on
	 * @param ErrorStr			String representing the error condition
	 */
	void OnAcceptInviteComplete( int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& FriendId, const FString& ListName, const FString& ErrorStr );

	/**
	 * Delegate used when the friends delete request has completed.
	 *
	 * @param LocalPlayer		The controller number of the associated user that made the request.
	 * @param bWasSuccessful	true if the async action completed without error, false if there was an error.
	 * @param DeletedFriendID	The ID of the deleted friend.
	 * @param ListName			Name of the friends list that was operated on.
	 * @param ErrorStr			String representing the error condition.
	 */
	void OnDeleteFriendComplete( int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& DeletedFriendID, const FString& ListName, const FString& ErrorStr );

	/**
	 * Delegate used when a friend ID request has completed.
	 *
	 * @param bWasSuccessful		true if the async action completed without error, false if there was an error.
	 * @param RequestingUserId		The ID of the user making the request.
	 * @param DisplayName			The string ID of the friend found.
	 * @param IdentifiedUserId		The net ID of the found friend.
	 * @param ErrorStr				String representing the error condition.
	 */
	void OnQueryUserIdMappingComplete(bool bWasSuccessful, const FUniqueNetId& RequestingUserId, const FString& DisplayName, const FUniqueNetId& IdentifiedUserId, const FString& Error);

	/**
	 * Delegate used when a query user info request has completed.
	 *
	 * @param LocalPlayer		The controller number of the associated user that made the request.
	 * @param bWasSuccessful	true if the async action completed without error, false if there was an error.
	 * @param UserIds			An array of user IDs filled out with info.
	 * @param ErrorStr			String representing the error condition.
	 */
	void OnQueryUserInfoComplete( int32 LocalPlayer, bool bWasSuccessful, const TArray< TSharedRef<class FUniqueNetId> >& UserIds, const FString& ErrorStr );

	/**
	 * Delegate used when a query presence info request has completed.
	 *
	 * @param UserId		The user ID.
	 * @param Presence	The user presence.
	 */
	void OnPresenceReceived(const class FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& NewPresence);

	/**
	 * Delegate used when a users presence is updated.
	 *
	 * @param UserId		The user ID.
	 * @param Presence	The user presence.
	 */
	void OnPresenceUpdated(const class FUniqueNetId& UserId, const bool bWasSuccessful);

	/**
	 * Delegate called when the friends list changes.
	 */
	void OnFriendsListChanged();

	/**
	 * Delegate called when an invite is received.
	 *
	 * @param UserId		The user ID.
	 * @param FriendId	The friend ID.
	 */
	void OnFriendInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FriendId);

	/**
	 * Delegate called when an invite is received to a game session
	 *
	 * @param UserId user that received the game invite
	 * @param FromId user that send the game invite
	 * @param InviteResult info about the game session that can be joined
	 */
	void OnGameInviteReceived(const FUniqueNetId& UserId, const FUniqueNetId& FromId, const FString& ClientId, const FOnlineSessionSearchResult& InviteResult);

	/**
	 * Process any invites that were received and are pending. 
	 * Will remove entries that are processed
	 */
	void ProcessReceivedGameInvites();
	/**
	 * Query for user info associated with user invites
	 *
	 * @return true if request is pending
	 */
	bool RequestGameInviteUserInfo();

	void OnGameDestroyed(const FName SessionName, bool bWasSuccessful);

	/**
	 * Delegate used when a friend is removed.
	 *
	 * @param UserId		The user ID.
	 * @param FriendId	The friend ID.
	 */
	void OnFriendRemoved(const FUniqueNetId& UserId, const FUniqueNetId& FriendId);

	/**
	 * Delegate used when an invite is rejected.
	 *
	 * @param UserId		The user ID.
	 * @param FriendId	The friend ID.
	 */
	void OnInviteRejected(const FUniqueNetId& UserId, const FUniqueNetId& FriendId);

	/**
	 * Delegate used when an invite is accepted.
	 *
	 * @param UserId		The user ID.
	 * @param FriendId	The friend ID.
	 */
	void OnInviteAccepted(const FUniqueNetId& UserId, const FUniqueNetId& FriendId);

	/**
	 * Handle the Chat window closed.
	 *
	 * @param InWindow - the window closed.
	 */
	void HandleChatWindowClosed(const TSharedRef<SWindow>& InWindow);

	/**
	 * Handle the Friends window closed.
	 *
	 * @param InWindow - the window closed.
	 */
	void HandleFriendsWindowClosed(const TSharedRef<SWindow>& InWindow);

	/**
	 * Handle an accept message accepted from a notification.
	 *
	 * @param MessageNotification The message responded to.
 	 * @param ResponseTpe The response from the message request.
	 */
	FReply HandleMessageAccepted( TSharedPtr< FFriendsAndChatMessage > MessageNotification, EFriendsResponseType::Type ResponseType );

	/**
	 * Validate friend list after a refresh. Check if the list has changed.
	 *
	 * @return True if the list has changes and the UI needs refreshing.
	 */
	bool ProcessFriendsList();

	/**
	 * A ticker used to perform updates on the main thread.
	 *
	 * @param Delta The tick delta.
	 * @return true to continue ticking.
	 */
	bool Tick( float Delta );

	/** Add a friend toast. */
	void AddFriendsToast(const FText Message);

	/**
	 * Get the best spawn position for the next chat windo
	 *
	 * @return Spawn position
	 */
	FVector2D GetWindowSpawnPosition() const;

private:

	/* Lists used to keep track of Friends
	*****************************************************************************/

	// Holds the list of user names to query add as friends 
	TArray< FString > FriendByNameRequests;
	// Holds the list of user names to send invites to. 
	TArray< FString > FriendByNameInvites;
	// Holds the list of Unique Ids found for user names to add as friends
	TArray< TSharedRef< FUniqueNetId > > QueryUserIds;
	// Holds the full friends list used to build the UI
	TArray< TSharedPtr< IFriendItem > > FriendsList;
	// Holds the recent players list
	TArray< TSharedPtr< IFriendItem > > RecentPlayersList;
	// Holds the filtered friends list used in the UI
	TArray< TSharedPtr< IFriendItem > > FilteredFriendsList;
	// Holds the list of old presence data for comparison to new presence updates
	TMap< FUniqueNetIdString, FOnlineUserPresence > OldUserPresenceMap;
	// Holds the outgoing friend request list used in the UI
	TArray< TSharedPtr< IFriendItem > > FilteredOutgoingList;
	// Holds the unprocessed friends list generated from a friends request update
	TArray< TSharedPtr< IFriendItem > > PendingFriendsList;
	// Holds the list of incoming invites that need to be responded to
	TArray< TSharedPtr< IFriendItem > > PendingIncomingInvitesList;
	// Holds the list of incoming game invites that need to be responded to
	TMap< FString, TSharedPtr< IFriendItem > > PendingGameInvitesList;
	// Holds the list of invites we have already responded to
	TArray< TSharedPtr< FUniqueNetId > > NotifiedRequest;
	// Holds the list messages sent out to be responded to
	TArray<TSharedPtr< FFriendsAndChatMessage > > NotificationMessages;
	// Holds an array of outgoing invite friend requests
	TArray< TSharedRef< FUniqueNetId > > PendingOutgoingFriendRequests;
	// Holds an array of outgoing delete friend requests
	TArray< FUniqueNetIdString > PendingOutgoingDeleteFriendRequests;
	// Holds an array of outgoing accept friend requests
	TArray< FUniqueNetIdString > PendingOutgoingAcceptFriendRequests;

	/**
	 * Game invite that needs to be processed before being displayed
	 */
	class FReceivedGameInvite
	{
	public:
		FReceivedGameInvite(
			const TSharedRef<FUniqueNetId>& InFromId,
			const FOnlineSessionSearchResult& InInviteResult,
			const FString& InClientId)
			: FromId(InFromId)
			, InviteResult(new FOnlineSessionSearchResult(InInviteResult))
			, ClientId(InClientId)
		{}
		// who sent the invite (could be non-friend)
		TSharedRef<FUniqueNetId> FromId;
		// session info needed to join the invite
		TSharedRef<FOnlineSessionSearchResult> InviteResult;
		// Clietn id for user that sent hte invite
		FString ClientId;
		// equality check
		bool operator==(const FReceivedGameInvite& Other) const
		{
			return Other.FromId == FromId && Other.ClientId == ClientId;
		}
	};
	// List of invites that need to be processed
	TArray<FReceivedGameInvite> ReceivedGameInvites;

	/* Delegates
	*****************************************************************************/

	// Holds the ticker delegate
	FTickerDelegate UpdateFriendsTickerDelegate;
	// Delegate to use for querying for recent players 
	FOnQueryRecentPlayersCompleteDelegate OnQueryRecentPlayersCompleteDelegate;
	// Delegate to use for deleting a friend
	FOnDeleteFriendCompleteDelegate OnDeleteFriendCompleteDelegate;
	// Delegate for querying user id from a name string
	IOnlineUser::FOnQueryUserMappingComplete OnQueryUserIdMappingCompleteDelegate;
	// Delegate to use for querying user info list
	FOnQueryUserInfoCompleteDelegate OnQueryUserInfoCompleteDelegate;
	// Delegate to use for querying user presence
	FOnPresenceReceivedDelegate OnPresenceReceivedCompleteDelegate;
	// Delegate to owner presence updated
	IOnlinePresence::FOnPresenceTaskCompleteDelegate OnPresenceUpdatedCompleteDelegate;
	// Delegate for friends list changing
	FOnFriendsChangeDelegate OnFriendsListChangedDelegate;
	// Delegate for an invite received
	FOnInviteReceivedDelegate OnFriendInviteReceivedDelegate;
	// Delegate for a game invite received
	FOnSessionInviteReceivedDelegate OnGameInviteReceivedDelegate;
	// Delegate for a game session being destroyed
	FOnDestroySessionCompleteDelegate OnDestroySessionCompleteDelegate;
	// Delegate for friend removed
	FOnFriendRemovedDelegate OnFriendRemovedDelegate;
	// Delegate for friend invite rejected
	FOnInviteRejectedDelegate	OnFriendInviteRejected;
	// Delegate for friend invite accepted
	FOnInviteAcceptedDelegate	OnFriendInviteAccepted;
	// Delegate for chat message received
	FChatMessageReceivedEvent OnChatmessageRecieved;

	// Holds the Friends list notification delegate
	FOnFriendsNotificationEvent FriendsListNotificationDelegate;
	// Holds the Friends Action notification delegate
	FOnFriendsNotificationActionEvent FriendsListActionNotificationDelegate;
	// Holds the Options Updated event notification delegate
	FOnFriendsUserSettingsUpdatedEvent FriendsUserSettingsUpdatedDelegate;
	// Holds the join game request delegate
	FOnFriendsJoinGameEvent FriendsJoinGameEvent;
	// Holds the chat message received delegate
	FChatMessageReceivedEvent ChatMessageReceivedEvent;
	// Delegate callback for determining if joining games functionality should be allowed
	FAllowFriendsJoinGame AllowFriendsJoinGameDelegate;

	// Internal events
	// Holds the delegate to call when the friends list gets updated - refresh the UI
	FOnFriendsUpdated OnFriendsListUpdatedDelegate;
	// Delegate for when list of active game invites updates
	FOnGameInvitesUpdated OnGameInvitesUpdatedDelegate;
	// Delegate for when a friend is selected for chat
	FOnChatFriendSelected OnChatFriendSelectedDelegate;

	/* Identity stuff
	*****************************************************************************/

	// Holds the Online identity
	IOnlineIdentityPtr OnlineIdentity;
	// Holds the Friends Interface
	IOnlineFriendsPtr FriendsInterface;
	// Holds the Online Subsystem
	IOnlineSubsystem* OnlineSub;

	/* Chat stuff
	*****************************************************************************/

	// Keeps track of global chat rooms that have been requested to join
	TArray<FString> ChatRoomstoJoin;
	// Manages private/public chat messages 
	TSharedPtr<class FFriendsMessageManager> MessageManager;
	// Holds the chat view model
	TSharedPtr<class FChatViewModel> ChatViewModel;
	// Joined Global Chat
	bool bJoinedGlobalChat;

	enum class EChatWindowMode : uint8
	{
		Widget,
		SingleWindow,
		MultiWindow
	};

	// Use chat window, one window for each chat, non-windowed widgets?
	EChatWindowMode ChatWindowMode;

	// Holds the clan repository
	TSharedPtr<class IClanRepository> ClanRepository;
	// Holds the friends list provider
	TSharedPtr<class IFriendListFactory> FriendsListFactory;

	/* Manger state
	*****************************************************************************/

	// Holds the manager state
	EFriendsAndManagerState::Type ManagerState;

	/* UI
	*****************************************************************************/
	// Holds the parent window
	TWeakPtr<SWindow> ParentWidget;
	// Holds the main Friends List window
	TSharedPtr< SWindow > FriendWindow;
	// Holds the Friends List widget
	TSharedPtr< SWidget > FriendListWidget;
	// Holds the Global chat window
	TSharedPtr< SWindow > GlobalChatWindow;	

	// Holds the whisper chat window
	struct WhisperChat
	{
		TSharedPtr< SWindow > ChatWindow;
		TSharedPtr< IFriendItem > FriendItem;
	};
	TArray<WhisperChat> WhisperChatWindows;


	// Holds the application view models - used for launching and querying the games
	TMap<FString, TSharedPtr<IFriendsApplicationViewModel>> ApplicationViewModels;
	// Holds the style used to create the Friends List widget
	FFriendsAndChatStyle Style;
	// Holds if the Friends list is inited
	bool bIsInited;
	// Holds the Friends system user settings
	FFriendsAndChatSettings UserSettings;
	// Holds if we need a list refresh
	bool bRequiresListRefresh;
	// Holds if we need a recent player list refresh
	bool bRequiresRecentPlayersRefresh;
	// Holds the toast notification
	TSharedPtr<SNotificationList> FriendsNotificationBox;

public:

	static TSharedRef< FFriendsAndChatManager > Get();
	static void Shutdown();

private:

	FFriendsAndChatAnalytics Analytics;
	float FlushChatAnalyticsCountdown;
	static TSharedPtr< FFriendsAndChatManager > SingletonInstance;

	/** Handle to various registered delegates */
	FDelegateHandle OnQueryRecentPlayersCompleteDelegateHandle;
	FDelegateHandle OnFriendsListChangedDelegateHandle;
	FDelegateHandle OnFriendInviteReceivedDelegateHandle;
	FDelegateHandle OnFriendRemovedDelegateHandle;
	FDelegateHandle OnFriendInviteRejectedHandle;
	FDelegateHandle OnFriendInviteAcceptedHandle;
	FDelegateHandle OnReadFriendsCompleteDelegateHandle;
	FDelegateHandle OnDeleteFriendCompleteDelegateHandle;
	FDelegateHandle OnQueryUserInfoCompleteDelegateHandle;
	FDelegateHandle OnPresenceReceivedCompleteDelegateHandle;
	FDelegateHandle OnGameInviteReceivedDelegateHandle;
	FDelegateHandle OnDestroySessionCompleteDelegateHandle;
	FDelegateHandle UpdateFriendsTickerDelegateHandle;
};
