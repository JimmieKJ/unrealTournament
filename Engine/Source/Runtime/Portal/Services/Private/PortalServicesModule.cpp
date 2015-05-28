// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PortalServicesPrivatePCH.h"
#include "TypeContainer.h"
#include "ModuleManager.h"


/**
 * Implements the PortalServices module.
 */
class FPortalServicesModule
	: public IPortalServicesModule
{
public:

	/** Virtual destructor. */
	virtual ~FPortalServicesModule() { }

public:

	// IPortalServicesModule

	virtual FTypeContainer& GetServiceContainer() override
	{
		return ServiceContainer;
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

private:

	/** Holds registered service instances. */
	FTypeContainer ServiceContainer;
};


IMPLEMENT_MODULE(FPortalServicesModule, PortalServices);
