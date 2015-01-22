// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "JsonPrivatePCH.h"
#include "ModuleManager.h"


DEFINE_LOG_CATEGORY(LogJson);


/**
 * Implements the Json module.
 */
class FJsonModule
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


IMPLEMENT_MODULE(FJsonModule, Json);
