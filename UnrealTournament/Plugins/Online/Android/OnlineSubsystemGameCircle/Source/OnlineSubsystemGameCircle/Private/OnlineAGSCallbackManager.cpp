// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemGameCirclePrivatePCH.h"

#include "OnlineAGSCallbackManager.h"


void FOnlineAGSCallbackManager::AddActiveCallback(AmazonGames::ICallback * InCallback)
{
	ActiveCallbacks.Add(InCallback);
}

void FOnlineAGSCallbackManager::CallbackCompleted(AmazonGames::ICallback * InCallback)
{
	ActiveCallbacks.Remove(InCallback);
	CompletedCallbacks.Add(InCallback);
}

void FOnlineAGSCallbackManager::Tick()
{
	while(CompletedCallbacks.Num())
	{
		delete CompletedCallbacks[0];
		CompletedCallbacks.RemoveAt(0);
	}
}
