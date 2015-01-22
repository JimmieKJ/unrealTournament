// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ProfilerClientModule.cpp: Implements the FProfilerClientModule class.
=============================================================================*/

#include "ProfilerClientPrivatePCH.h"


/**
 * Implements the ProfilerClient module
 */
class FProfilerClientModule
	: public IProfilerClientModule
{
public:

	// Begin IModuleInterface interface

	virtual void StartupModule() override
	{
		MessageBusPtr = IMessagingModule::Get().GetDefaultBus();
	}

	virtual void ShutdownModule() override
	{
	}

	// End IModuleInterface interface

public:

	// Begin IProfilerClientModule interface

	virtual IProfilerClientPtr CreateProfilerClient() override
	{
		IMessageBusPtr MessageBus = MessageBusPtr.Pin();

		if (!MessageBus.IsValid())
		{
			return nullptr;
		}
		
		return MakeShareable(new FProfilerClientManager(MessageBus.ToSharedRef()));
	}

	// End IProfilerClientModule interface

private:

	// Holds a weak pointer to the message bus.
	IMessageBusWeakPtr MessageBusPtr;
};


/* Static initialization
 *****************************************************************************/


IMPLEMENT_MODULE(FProfilerClientModule, ProfilerClient);
