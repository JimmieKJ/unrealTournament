// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AGS/AGSClientCommonInterface.h"

class FOnlineAGSCallbackManager
{
public:

	void AddActiveCallback(AmazonGames::ICallback * InCallback);

	void CallbackCompleted(AmazonGames::ICallback * InCallback);

	void Tick();

private:

	TArray<AmazonGames::ICallback *> ActiveCallbacks;
	TArray<AmazonGames::ICallback *> CompletedCallbacks;
};

typedef TSharedPtr<FOnlineAGSCallbackManager, ESPMode::ThreadSafe> FOnlineAGSCallbackManagerPtr;
