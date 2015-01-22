// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IDeviceProfileSelectorModule.h: Declares the IDeviceProfileSelectorModule interface.
=============================================================================*/

#pragma once

#include "ModuleInterface.h"


/**
 * Device Profile Selector module
 */
class IDeviceProfileSelectorModule
	: public IModuleInterface
{
public:

	/**
	 * Run the logic to choose an appropriate device profile for this session
	 *
	 * @return The name of the device profile to use for this session
	 */
	virtual const FString GetRuntimeDeviceProfileName() = 0;


	/**
	 * Virtual destructor.
	 */
	virtual ~IDeviceProfileSelectorModule()
	{
	}
};
