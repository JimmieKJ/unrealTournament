// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IChatCommunicationService.h"
#include "FriendsMessageType.h"

// Struct for holding chat message information. Will probably be replaced by OSS version
struct FFriendChatMessage
{
	EChatMessageType::Type MessageType;
	FText FromName;
	FText ToName;
	FText Message;
	FDateTime MessageTime;
	FDateTime ExpireTime;
	TSharedPtr<class FChatMessage> MessageRef;
	TSharedPtr<const FUniqueNetId> SenderId;
	TSharedPtr<const FUniqueNetId> RecipientId;
	bool bIsFromSelf;
};

/**
 * Implement the Friend and Chat manager
 */
class FMessageService
	: public TSharedFromThis<FMessageService>
	, public IChatCommunicationService
{
public:

	/** Destructor. */
	virtual ~FMessageService() {};

	virtual void Login() = 0;
	virtual void Logout() = 0;

	/**
	 * Get messages 
	 * @return array of messages
	 */
	virtual const TArray<TSharedRef<FFriendChatMessage> >& GetMessages() const = 0;

	/**
	 * Join a public chat room
	 * @param RoomName Name of room to join
	 */
	virtual void JoinPublicRoom(const FString& RoomName) = 0;

	/**
	 * Inform Message service we have opened a global chat window
	 */
	virtual bool OpenGlobalChat() = 0;

	/**
	 * returns true if we have joined a public chat room
	 */
	virtual bool IsInGlobalChat() const = 0;

	/**
	 * Get our online status
	 */
	virtual EOnlinePresenceState::Type GetOnlineStatus() = 0;

	/**
	 * Is the chat system currently throttled
	 */
	virtual bool IsThrottled() = 0;

	/**
	 * Register a network message has been sent. Used for throttling
	 */
	virtual void RegisterGameMessageSent() = 0;

	/**
	 * Get analytics service
	 */
	virtual TSharedPtr<class FFriendsAndChatAnalytics> GetAnalytics() const = 0;

	DECLARE_EVENT_OneParam(FMessageService, FOnChatPublicRoomJoinedEvent, const FString& /*RoomName*/)
	virtual FOnChatPublicRoomJoinedEvent& OnChatPublicRoomJoined() = 0;

	DECLARE_EVENT_OneParam(FMessageService, FOnChatPublicRoomExitedEvent, const FString& /*RoomName*/)
	virtual FOnChatPublicRoomExitedEvent& OnChatPublicRoomExited() = 0;
};

/**
 * Creates the implementation of a chat manager.
 *
 * @return the newly created FriendViewModel implementation.
 */
FACTORY(TSharedRef< FMessageService >, FMessageService,
	const TSharedRef<class FOSSScheduler>& OSSScheduler, 
	const TSharedRef<class FFriendsService>& FriendsService,
	const TSharedRef<class FGameAndPartyService>& GamePartyService);
