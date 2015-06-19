// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SessionServicesPrivatePCH.h"
#include "ISessionServicesModule.h"
#include "ModuleManager.h"


/**
 * Implements the SessionServices module.
 *
 * @todo gmp: needs better error handling in case MessageBusPtr is invalid
 */
class FSessionServicesModule
	: public ISessionServicesModule
{
public:

	/** Default constructor. */
	FSessionServicesModule()
		: SessionManager(nullptr)
		, SessionService(nullptr)
	{ }

public:

	// ISessionServicesModule interface

	virtual ISessionManagerRef GetSessionManager() override
	{
		if (!SessionManager.IsValid())
		{
			SessionManager = MakeShareable(new FSessionManager(MessageBusPtr.Pin().ToSharedRef()));
		}

		return SessionManager.ToSharedRef();
	}

	virtual ISessionServiceRef GetSessionService() override
	{
		if (!SessionService.IsValid())
		{
			SessionService = MakeShareable(new FSessionService(MessageBusPtr.Pin().ToSharedRef()));
		}

		return SessionService.ToSharedRef();
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		MessageBusPtr = IMessagingModule::Get().GetDefaultBus();
		check(MessageBusPtr.IsValid());
	}

	virtual void ShutdownModule() override
	{
		SessionManager.Reset();
		SessionService.Reset();
	}

private:
	
	/** Holds a weak pointer to the message bus. */
	IMessageBusWeakPtr MessageBusPtr;

	/** Holds the session manager singleton. */
	ISessionManagerPtr SessionManager;

	/** Holds the session service singleton. */
	ISessionServicePtr SessionService;
};


IMPLEMENT_MODULE(FSessionServicesModule, SessionServices);
