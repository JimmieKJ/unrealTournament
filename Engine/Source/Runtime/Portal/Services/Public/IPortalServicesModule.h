// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"


class FTypeContainer;
class IPortalService;


/**
 * Interface for the PortalServices module.
 */
class IPortalServicesModule
	: public IModuleInterface
{
public:

	/**
	 * Gets a container with registered Portal services.
	 *
	 * @return The type container.
	 */
	virtual FTypeContainer& GetServiceContainer() = 0;
};
