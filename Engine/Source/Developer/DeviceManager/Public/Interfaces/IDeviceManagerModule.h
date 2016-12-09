// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Modules/ModuleInterface.h"
#include "Interfaces/ITargetDeviceServiceManager.h"

class SWindow;

/**
 * Interface for device manager modules.
 */
class IDeviceManagerModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a device manager widget.
	 *
	 * @param DeviceServiceManager The target device manager to use.
	 * @return The new widget.
	 */
	virtual TSharedRef<class SWidget> CreateDeviceManager( const ITargetDeviceServiceManagerRef& DeviceServiceManager, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow ) = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~IDeviceManagerModule( ) { }
};
