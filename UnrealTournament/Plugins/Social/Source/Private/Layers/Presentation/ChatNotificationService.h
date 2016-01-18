// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IChatNotificationService.h"

class FChatNotificationService
	: public IChatNotificationService
	, public TSharedFromThis<FChatNotificationService>
{
public:

	virtual ~FChatNotificationService() {}

	// Friend Notifications
	virtual void SendFriendInviteSentNotification(const FString& FriendDisplayName) = 0;
	virtual void SendFriendInviteNotification(const TSharedPtr<IFriendItem>& Invite, IChatNotificationService::FOnNotificationResponseDelegate ResponceDelegate) = 0;
	virtual void SendAcceptInviteNotification(const TSharedPtr< IFriendItem >& Friend) = 0;


	// Game Notifications
	virtual void SendGameInviteNotification(const TSharedPtr< IFriendItem >& Friend, IChatNotificationService::FOnNotificationResponseDelegate ResponceDelegate) = 0;
	
	// Clan Notifications
	virtual void SendNewClanMemberNotification(const TSharedPtr< class IClanInfo >& Clan, const TSharedPtr< IFriendItem >& Friend) = 0;
	virtual void SendClanMemberLeftNotification(const TSharedPtr< class IClanInfo >& Clan, const TSharedPtr< IFriendItem >& Friend) = 0;
	virtual void SendClanInviteReceived(const TSharedPtr< class IClanInfo >& Clan, const TSharedPtr< IFriendItem >& Friend, IChatNotificationService::FOnNotificationResponseDelegate ResponceDelegate) = 0;
	virtual void SendClanJoinRequest(const TSharedPtr< class IClanInfo >& Clan, const TSharedPtr< IFriendItem >& Friend, IChatNotificationService::FOnNotificationResponseDelegate ResponceDelegate) = 0;
};

/**
 * Creates the implementation for an FChatDisplayService.
 *
 * @return the newly created FChatDisplayService implementation.
 */
FACTORY(TSharedRef< FChatNotificationService >, FChatNotificationService);