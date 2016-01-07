// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
/*=============================================================================
	IGameAndPartyService.h: Interface for game and party services.
	The game and party service is used to inform when a party action has been performed.
	e.g. friend joined your party
=============================================================================*/

#pragma once

class IGameAndPartyService
{
public:

	// Get if we are able to party chat
	virtual bool IsInPartyChat() const = 0;

	// Get if we are able to game chat
	virtual bool IsInGameChat() const = 0;

	// Enable use of party chat over XMPP
	virtual void EnablePartyChat(FString ChatRoomID) = 0;

	// Disable use of party chat over XMPP
	virtual void DisablePartyChat() = 0;

	// Enable use of game chat over the game server
	virtual void EnableGameChat() = 0;
	
	// Disable use of game chat over the game server
	virtual void DisableGameChat() = 0;

	// Enable use of live streaming
	virtual void EnableLiveStreaming() = 0;
	
	// Disable use of live streaming
	virtual void DisableLiveStreaming() = 0;

	// Enable use of Team chat over XMPP
	virtual void EnableTeamChat(FString ChatRoomID) = 0;

	// Disable use of team chat over XMPP
	virtual void DisableTeamChat() = 0;

	DECLARE_EVENT_ThreeParams(IGameAndPartyService, FOnFriendsJoinPartyEvent, const FUniqueNetId& /*SenderId*/, const TSharedRef<class IOnlinePartyJoinInfo>& /*PartyJoinInfo*/, bool /*bIsFromInvite*/)
	virtual FOnFriendsJoinPartyEvent& OnFriendsJoinParty() = 0;
};
