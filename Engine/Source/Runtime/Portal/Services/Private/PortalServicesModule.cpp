// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PortalServicesPrivatePCH.h"
#include "IPortalServicesModule.h"
#include "PortalServiceLocator.h"
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

	// IPortalServicesModule interface

	virtual TSharedRef<IPortalServiceLocator> CreateLocator(const TSharedRef<FTypeContainer>& ServiceDependencies) override
	{
		return FPortalServiceLocatorFactory::Create(ServiceDependencies);
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
	virtual bool SupportsDynamicReloading() override { return true; }
};


IMPLEMENT_MODULE(FPortalServicesModule, PortalServices);
