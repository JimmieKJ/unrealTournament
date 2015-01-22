// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Everything a local player will use to manage an online session.
 */

#pragma once
#include "OnlineSession.generated.h"

UCLASS(config=Game)
class ENGINE_API UOnlineSession : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Register all delegates needed to manage online sessions.  */
	virtual void RegisterOnlineDelegates(class UWorld* InWorld) {};

	/** Tear down all delegates used to manage online sessions.	 */
	virtual void ClearOnlineDelegates(class UWorld* InWorld) {};

	/** Called to tear down any online sessions and return to main menu	 */
	virtual void HandleDisconnect(UWorld *World, class UNetDriver *NetDriver);
};



