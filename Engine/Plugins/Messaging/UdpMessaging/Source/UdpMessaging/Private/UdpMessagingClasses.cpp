// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UdpMessagingPrivatePCH.h"


UUdpMessagingSettings::UUdpMessagingSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, EnableTransport(true)
	, MulticastTimeToLive(1)
	, EnableTunnel(false)
{ }
