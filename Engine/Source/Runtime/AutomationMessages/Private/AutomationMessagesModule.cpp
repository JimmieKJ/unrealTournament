// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AutomationMessagesPrivatePCH.h"


/**
 * Implements the AutomationMessages module.
 */
class FAutomationMessagesModule
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


IMPLEMENT_MODULE(FAutomationMessagesModule, AutomationMessages);
