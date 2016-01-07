// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Enum to list message flags
namespace EMessageType
{
	enum Type
	{
		// A chat message
		ChatMessage,
		// Friend came online/in-game
		PresenceChange,
		// Request to join a game
		JoinGame,
		// Invite someone to join your game
		GameInvite,
		// Friend request sent to somebody
		FriendInviteSent,
		// Friend invite received
		FriendInvite,
		// Request accepted
		FriendAccepted,
		// The message is handled. Clear the notification if present
		MessageHandled,
		// Invite someone to join your clan
		ClanInvite,
	};
};

// Enum to list message response types
namespace EResponseType
{
	enum Type
	{
		// Accept a request
		Response_Accept,
		// Reject a request
		Response_Reject,
		// Ignore a request
		Response_Ignore,
	};
};

class FUniqueNetId;

// Enum holding the display list selection
namespace EFriendsDisplayLists
{
	enum Type
	{
		SuggestedFriendsDisplay,		// friends import suggestions from external online platform
		GameInviteDisplay,				// GameInviteDisplay
		FriendRequestsDisplay,			// Friend request display
		DefaultDisplay, 				// Default friend list display
		OutgoingFriendInvitesDisplay,	// Outgoing friends invites
		RecentPlayersDisplay,			// Recent Players display
		BlockedPlayersDisplay,			// Blocked players display
		ClanMemberDisplay,				// Clan member
		MAX_None
	};

	inline const FText ToFText(EFriendsDisplayLists::Type EnumVal)
	{
		static const FText FriendsList = NSLOCTEXT("FriendsListTypes", "FriendsList", "ONLINE");
		static const FText RecentPlayersList = NSLOCTEXT("FriendsListTypes", "RecentPlayersList", "RECENT PLAYERS");
		static const FText BlockedPlayersList = NSLOCTEXT("FriendsListTypes", "BlockedPlayersList", "BLOCKED PLAYERS");
		static const FText FriendRequestList = NSLOCTEXT("FriendsListTypes", "InvitesList", "FRIEND REQUESTS");
		static const FText OutgoingFriendRequestList = NSLOCTEXT("FriendsListTypes", "OutgoingInvites", "OUTGOING");
		static const FText GameInviteList = NSLOCTEXT("FriendsListTypes", "GameInvites", "INVITATIONS");
		static const FText ClanMemberList = NSLOCTEXT("FriendsListTypes", "ClanMember", "MEMBERS");
		static const FText SuggestedList = NSLOCTEXT("FriendsListTypes", "SuggestedList", "SUGGESTIONS");

		switch (EnumVal)
		{
			case DefaultDisplay: return FriendsList;
			case RecentPlayersDisplay : return RecentPlayersList;
			case BlockedPlayersDisplay : return BlockedPlayersList;
			case FriendRequestsDisplay: return FriendRequestList;
			case OutgoingFriendInvitesDisplay : return OutgoingFriendRequestList;
			case GameInviteDisplay : return GameInviteList;
			case ClanMemberDisplay : return ClanMemberList;
			case SuggestedFriendsDisplay: return SuggestedList;
		}

		return FText::GetEmpty();
	}
};


// Class containing the friend message information
class FFriendsAndChatMessage
{
public:
	/**
	 * Constructor.
	 *
	 * @param InMessage The message content.
	 */
	FFriendsAndChatMessage( const FString& InMessage )
		: MessageConent( InMessage )
		, bAutoAccept( false )
	{ }

	/**
	 * Constructor.
	 *
	 * @param InMessage The message content.
	 * @param InUniqueFriendID The Friend ID.
	 */
	FFriendsAndChatMessage( const FString& InMessage, const TSharedRef<const FUniqueNetId> InUniqueFriendID )
		: MessageConent( InMessage )
		, UniqueFriendID( InUniqueFriendID )
		, bAutoAccept( false )
	{ }

	/**
	 * Get the message content.
	 *
	 * @return The message.
	 */
	FString GetMessage()
	{
		return MessageConent;
	}

	/**
	 * Get the Net ID for the Friend this message is sent to.
	 *
	 * @return The net ID.
	 */
	const TSharedRef<const FUniqueNetId> GetUniqueID()
	{
		return UniqueFriendID.ToSharedRef();
	}

	/**
	* Get the Net ID for the clan this message is sent to.
	*
	* @return The net ID.
	*/
	const TSharedRef<const FUniqueNetId> GetClanID()
	{
		return UniqueClanID.ToSharedRef();
	}

	/**
	* Set the Net ID for the clan this message is sent to.
	*
	* @return The net ID.
	*/
	void SetClanID(TSharedRef<const FUniqueNetId> ClanID)
	{
		UniqueClanID = ClanID;
	}

	/**
	 * Set a button callbacks.
	 *
	 * @param InCallback The button callback.
	 */
	void SetButtonCallback( FOnClicked InCallback )
	{
		ButtonCallbacks.Add( InCallback );
	}

	/**
	 * Set a button callbacks.
	 *
	 * @param InCallback The button callback.
	 */
	void SetButtonDescription( FText Description )
	{
		MessageDescriptions.Add( Description );
	}

	/**
	 * Set a button style.
	 *
	 * @param ButtonStyle The button callback.
	 */
	void SetButtonStyle(FName ButtonStyle)
	{
		ButtonStyles.Add(ButtonStyle);
	}

	
	/**
	 * Set a button response type.
	 * Purely for the games that need to know the button types
	 *
	 * @param InType button response type
	 */
	void SetButtonResponseType(EResponseType::Type InType)
	{
		ButtonResponseTypes.Add(InType);
	}

	/**
	 * Set the message type.
	 *
	 * @param InType The message type.
	 */
	void SetMessageType(EMessageType::Type InType)
	{
		MessageType = InType;
	}

	/**
	 * Sets the ID to use when joining a game from an invite. Passed through the command line.
	 *
	 * @param MessageID The string representation of the NetID.
	 */
	void SetLaunchGameID( const FString& MessageID )
	{
		LaunchGameID = MessageID;
	}
	
	/**
	 * Gets the ID to use when joining a game from an invite. Passed through the command line.
	 *
	 * @return string representation of the message ID.
	 */
	const FString& GetLauchGameID() const
	{
		return LaunchGameID;
	}

	/**
	 * Get the message type.
	 *
	 * @return The message type.
	 */
	EMessageType::Type GetMessageType()
	{
		return MessageType;
	}

	/**
	 * Set the requester name.
	 *
	 * @param InName The requesters name.
	 */
	void SetRequesterName( const FString& InName )
	{
		RequesterName = InName;
	}

	/**
	 * Get the requester name.
	 *
	 * @return The requesters name.
	 */
	const FString& GetRequesterName() const
	{
		return RequesterName;
	}

	/**
	 * Set if this message should be handled by the caller, not the Friends Module.
	 *
	 * @param bInSelfHandle True if the caller handles this message themselves.
	 */
	void SetSelfHandle( bool bInSelfHandle )
	{
		bSelfHandle = bInSelfHandle;
	}

	/** Set if this message should accepted automatically. */
	void SetAutoAccept()
	{
		bAutoAccept = true;
	}

	/**
	 * Should the caller auto accept this message.
	 *
	 * @return True if the user should handle auto accept this message.
	 */
	const bool IsAutoAccepted() const
	{
		return bAutoAccept;
	}

	/**
	 * Should the caller handle this message themselves.
	 *
	 * @return True if the user should handle the message themselves.
	 */
	const bool IsSelfHandle() const
	{
		return bSelfHandle;
	}

	/**
	 * Get a button callbacks.
	 *
	 * @return The button callback array.
	 */
	const TArray< FOnClicked >& GetCallbacks() const
	{
		return ButtonCallbacks;
	}

	/**
	 * Get a button description.
	 *
	 * @return The button callback array.
	 */
	const TArray< FText >& GetButtonDescriptions() const
	{
		return MessageDescriptions;
	}

	/**
	* Get button response types.
	*
	* @return The button callback array.
	*/
	const TArray<EResponseType::Type>& GetButtonResponseTypes() const
	{
		return ButtonResponseTypes;
	}


	/**
	* Get button styles.
	*
	* @return The button style array.
	*/
	const TArray< FName >& GetButtonStyles() const
	{
		return ButtonStyles;
	}

	/** Set this message into a handled state. */
	void SetHandled()
	{
		MessageType = EMessageType::MessageHandled;
	}

private:

	// Holds the button callbacks
	TArray< FOnClicked > ButtonCallbacks;

	// Used to determine the response associated with a button
	TArray< EResponseType::Type > ButtonResponseTypes;

	// Holds the button styles
	TArray< FName > ButtonStyles;

	// Holds the Action Descriptions
	TArray< FText > MessageDescriptions;	
	
	// Holds the message content
	FString MessageConent;
	
	// Holds the Unique Friend Net ID
	const TSharedPtr<const FUniqueNetId> UniqueFriendID;

	// Holds the Unique clan Net ID
	TSharedPtr<const FUniqueNetId> UniqueClanID;
	
	// Holds the message type
	EMessageType::Type MessageType;
	
	// Holds the requester name.
	FString RequesterName;
	
	// Holds if the message should be self handled.
	bool bSelfHandle;
	
	// Holds if this message should be auto accepted.
	bool bAutoAccept;
	
	// Holds the launch game ID
	FString LaunchGameID;
};
