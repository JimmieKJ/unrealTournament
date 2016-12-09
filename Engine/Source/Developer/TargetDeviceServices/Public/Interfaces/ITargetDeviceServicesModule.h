// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Interfaces/ITargetDeviceProxyManager.h"
#include "Interfaces/ITargetDeviceServiceManager.h"

/**
 * Interface for target device services modules.
 */
class ITargetDeviceServicesModule
	: public IModuleInterface
{
public:

	/**
	 * Gets the target device proxy manager.
	 *
	 * @return The device proxy manager.
	 * @see GetDeviceServiceManager
	 */
	virtual ITargetDeviceProxyManagerRef GetDeviceProxyManager() = 0;

	/**
	 * Gets the target device service manager.
	 *
	 * @return The device service manager.
	 * @see GetDeviceProxyManager
	 */
	virtual ITargetDeviceServiceManagerRef GetDeviceServiceManager() = 0;

public:

	/** Virtual destructor. */
	virtual ~ITargetDeviceServicesModule() { }
};
