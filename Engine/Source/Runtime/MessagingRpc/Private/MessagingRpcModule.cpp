// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IMessageRpcClient.h"
#include "MessageRpcClient.h"
#include "IMessageRpcServer.h"
#include "MessageRpcServer.h"
#include "IMessagingRpcModule.h"


/**
 * Implements the MessagingRpc module.
 */
class FMessagingRpcModule
	: public IMessagingRpcModule
{
public:

	// IMessagingRpcModule interface

	virtual TSharedRef<IMessageRpcClient> CreateRpcClient() override
	{
		return MakeShareable(new FMessageRpcClient);
	}

	virtual TSharedRef<IMessageRpcServer> CreateRpcServer() override
	{
		return MakeShareable(new FMessageRpcServer);
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

};


IMPLEMENT_MODULE(FMessagingRpcModule, MessagingRpc);
