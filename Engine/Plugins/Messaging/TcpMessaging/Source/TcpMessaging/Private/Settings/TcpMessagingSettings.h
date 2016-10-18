// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TcpMessagingSettings.generated.h"


UCLASS(config=Engine)
class UTcpMessagingSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Whether the TCP transport channel is enabled */
	UPROPERTY(config, EditAnywhere, Category = Transport)
	bool EnableTransport;

	/**
	 * The IP endpoint to listen for incoming connections.
	 *
	 * The format is IP_ADDRESS:PORT_NUMBER or blank to disable listening.
	 */
	UPROPERTY(config, EditAnywhere, Category=Transport)
	FString ListenEndpoint;

	/**
	 * The IP endpoints to try to establish outgoing connection to.
	 *
	 * Use this setting to connect to a remote peer.
	 * The format is IP_ADDRESS:PORT_NUMBER.
	 */
	UPROPERTY(config, EditAnywhere, Category=Transport)
	TArray<FString> ConnectToEndpoints;

	/**
	 * Delay time between attempts to re-establish outgoing connections that become disconnected or fail to connect
	 * 0 disables reconnection
	 */
	UPROPERTY(config, EditAnywhere, Category = Transport)
	int32 ConnectionRetryDelay;
};
