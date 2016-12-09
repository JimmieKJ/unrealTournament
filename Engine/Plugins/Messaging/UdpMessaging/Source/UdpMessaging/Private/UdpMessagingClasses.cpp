// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Shared/UdpMessagingSettings.h"


UUdpMessagingSettings::UUdpMessagingSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, EnableTransport(true)
	, MulticastTimeToLive(1)
	, EnableTunnel(false)
{ }
