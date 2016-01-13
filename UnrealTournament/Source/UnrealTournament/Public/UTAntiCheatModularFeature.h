// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "Runtime/Core/Public/Features/IModularFeature.h"

class UNREALTOURNAMENT_API UTAntiCheatModularFeature : public IModularFeature
{
public:
	/**
	 * Called when a new player has joined the game server.
	 *
	 * @param	[in]		Player		Target PlayerController.
	 * @param	[in]		Options		Game options passed in by the client at login.
	 * @param	[in]		UniqueId	Platform specific unique identifier for the logging in player.
	 *
	 * @return	The function does not return value.
	 */
	virtual void OnPlayerLogin(APlayerController *Player, const FString& Options, const TSharedPtr<const FUniqueNetId>& UniqueId) = 0;

	/**
	 * Called when a player disconnects from the game server.
	 *
	 * @param	[in]		Player		Target PlayerController.
	 *
	 * @return	The function does not return value.
	 */
	virtual void OnPlayerLogout(APlayerController *Player) = 0;
};