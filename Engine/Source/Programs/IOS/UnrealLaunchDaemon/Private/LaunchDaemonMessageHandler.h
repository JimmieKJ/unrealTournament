// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Messaging.h"
#include "LaunchDaemonMessages.h"

class FLaunchDaemonMessageHandler
{
public:

	void Init();
	void Shutdown();

	void Launch(const FString& LaunchURL);

private:
	void HandlePingMessage(const FIOSLaunchDaemonPing& Message, const IMessageContextRef& Context);
	void HandleLaunchRequest(const FIOSLaunchDaemonLaunchApp& Message, const IMessageContextRef& Context);

	FMessageEndpointPtr MessageEndpoint;
	FString AppId;
};
