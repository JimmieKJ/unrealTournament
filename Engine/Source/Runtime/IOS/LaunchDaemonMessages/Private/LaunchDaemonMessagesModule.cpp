// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LaunchDaemonMessagesPrivatePCH.h"


/**
 * Implements the EngineMessages module.
 */
class FLaunchDaemonMessagesModule
	: public IModuleInterface
{
public:

	virtual void StartupModule( ) override { }

	virtual void ShutdownModule( ) override { }

	virtual bool SupportsDynamicReloading( ) override
	{
		return true;
	}
};


IMPLEMENT_MODULE(FLaunchDaemonMessagesModule, LaunchDaemonMessages);
