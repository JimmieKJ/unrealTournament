// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "InternationalizationPrivatePCH.h"
#include "ModuleManager.h"


/**
 * Implements the Internationalization module.
 */
class FInternationalizationModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule( ) override { }
	virtual void ShutdownModule( ) override { }

	virtual bool SupportsDynamicReloading( ) override
	{
		return false;
	}
};


IMPLEMENT_MODULE(FInternationalizationModule, Internationalization);
