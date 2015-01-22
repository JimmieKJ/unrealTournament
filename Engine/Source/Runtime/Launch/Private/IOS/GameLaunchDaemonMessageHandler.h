// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Messaging.h"
#include "LaunchDaemonMessages.h"

class FGameLaunchDaemonMessageHandler
{
public:

	void Init();
	void Shutdown();

private:
	void HandlePingMessage(const FIOSLaunchDaemonPing& Message, const IMessageContextRef& Context);
	void HandleLaunchRequest(const FIOSLaunchDaemonLaunchApp& Message, const IMessageContextRef& Context);


	FMessageEndpointPtr MessageEndpoint;
	FString AppId;
};
