// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PortalMessagesPrivatePCH.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"


/**
 * Implements the module.
 */
class FPortalMessagesModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }

	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}
};


IMPLEMENT_MODULE(FPortalMessagesModule, PortalMessages);
