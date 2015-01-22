// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Player: Corresponds to a real player (a local camera or remote net player).
 */

#pragma once
#include "Player.generated.h"

UCLASS(transient, config=Engine)
class UPlayer : public UObject, public FExec
{
	GENERATED_UCLASS_BODY()

	/** The actor this player controls. */
	UPROPERTY(transient)
	class APlayerController* PlayerController;

	// Net variables.
	/** the current speed of the connection */
	UPROPERTY()
	int32 CurrentNetSpeed;

	/** @todo document */
	UPROPERTY(globalconfig)
	int32 ConfiguredInternetSpeed;
	
	/** @todo document */
	UPROPERTY(globalconfig)
	int32 ConfiguredLanSpeed;

public:
	// Begin FExec interface.
	ENGINE_API virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd,FOutputDevice& Ar) override;
	// End FExec interface.

	/**
	 * Dynamically assign Controller to Player and set viewport.
	 *
	 * @param    PC - new player controller to assign to player
	 */
	ENGINE_API virtual void SwitchController( class APlayerController* PC );

	/**
	 * Return the main menu after graceful cleanup
	 */
	virtual void HandleDisconnect(class UWorld *World, class UNetDriver *NetDriver) {}
};
