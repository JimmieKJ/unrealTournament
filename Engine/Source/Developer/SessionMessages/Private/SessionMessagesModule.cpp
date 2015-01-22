// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SessionMessagesPrivatePCH.h"


/**
 * Implements the SessionMessages module.
 */
class FSessionMessagesModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule( ) override { }
	virtual void ShutdownModule( ) override { }

	virtual bool SupportsDynamicReloading( ) override
	{
		return true;
	}
};


IMPLEMENT_MODULE(FSessionMessagesModule, SessionMessages);
