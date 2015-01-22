// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UdpMessagingSettings.generated.h"


UCLASS(config=Engine)
class UUdpMessagingSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Whether the UDP transport channel is enabled. */
	UPROPERTY(config, EditAnywhere, Category=Transport)
	bool EnableTransport;

	/** The IP endpoint to listen to and send packets from. */
	UPROPERTY(config, EditAnywhere, Category=Transport)
	FString UnicastEndpoint;

	/** The IP endpoint to send multicast packets to. */
	UPROPERTY(config, EditAnywhere, Category=Transport)
	FString MulticastEndpoint;

	/** The time-to-live (TTL) for sent multicast packets. */
	UPROPERTY(config, EditAnywhere, Category=Transport)
	uint8 MulticastTimeToLive;

	/**
	 * The IP endpoints of static devices.
	 *
	 * Use this setting to list devices on other subnets, such as mobile phones on a WiFi network.
	 */
	UPROPERTY(config, EditAnywhere, Category=Transport, AdvancedDisplay)
	TArray<FString> StaticEndpoints;

public:

	/** Whether the UDP tunnel is enabled. */
	UPROPERTY(config, EditAnywhere, Category=Tunnel)
	bool EnableTunnel;

	/** The IP endpoint to listen to and send packets from. */
	UPROPERTY(config, EditAnywhere, Category=Tunnel)
	FString TunnelUnicastEndpoint;

	/** The IP endpoint to send multicast packets to. */
	UPROPERTY(config, EditAnywhere, Category=Tunnel)
	FString TunnelMulticastEndpoint;

	/**
	 * The IP endpoints of remote tunnel nodes.
	 *
	 * Use this setting to connect to remote tunnel services.
	 */
	UPROPERTY(config, EditAnywhere, Category=Tunnel, AdvancedDisplay)
	TArray<FString> RemoteTunnelEndpoints;
};
