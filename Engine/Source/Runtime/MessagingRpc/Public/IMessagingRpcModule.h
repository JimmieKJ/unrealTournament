// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "IMessageRpcClient.h"
#include "IMessageRpcServer.h"

/**
 * Interface for the MessagingRpc module.
 */
class IMessagingRpcModule
	: public IModuleInterface
{
public:

	/**
	 * Create a client for remote procedure calls.
	 *
	 * @return The RPC client.
	 */
	virtual TSharedRef<IMessageRpcClient> CreateRpcClient() = 0;

	/**
	 * Create a server for remote procedure calls.
	 *
	 * @return The RPC server.
	 */
	virtual TSharedRef<IMessageRpcServer> CreateRpcServer() = 0;

public:

	/** Virtual destructor. */
	virtual ~IMessagingRpcModule() { }
};
