// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "ModuleInterface.h"

/**
 * Online subsystem module class  (Null Implementation)
 * Code related to the loading of the Null module
 */
class FOnlineSubsystemNullModule : public IModuleInterface
{
private:

	/** Class responsible for creating instance(s) of the subsystem */
	class FOnlineFactoryNull* NullFactory;

public:

	FOnlineSubsystemNullModule() : 
		NullFactory(NULL)
	{}

	virtual ~FOnlineSubsystemNullModule() {}

	// IModuleInterface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

	virtual bool SupportsAutomaticShutdown() override
	{
		return false;
	}
};
