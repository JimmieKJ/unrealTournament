// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OnlineSession.cpp: Online session related implementations
	(creating/joining/leaving/destroying sessions)
=============================================================================*/

#include "EnginePrivate.h"
#include "GameFramework/OnlineSession.h"

UOnlineSession::UOnlineSession(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UOnlineSession::HandleDisconnect(UWorld *World, UNetDriver *NetDriver)
{
	// Let the engine cleanup this disconnect
	GEngine->HandleDisconnect(World, NetDriver);
}
