// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IDeviceProfileServicesModule.h"
#include "DeviceProfileServicesUIManager.h"


/**
 * Implements the DeviceProfileServices module.
 */
class FDeviceProfileServicesModule
	: public IDeviceProfileServicesModule
{
public:

	// IDeviceProfileServicesModule interface

	virtual IDeviceProfileServicesUIManagerRef GetProfileServicesManager( ) override
	{
		if (!DeviceProfileServicesUIManagerSingleton.IsValid())
		{
			DeviceProfileServicesUIManagerSingleton = MakeShareable(new FDeviceProfileServicesUIManager());
		}

		return DeviceProfileServicesUIManagerSingleton.ToSharedRef();
	}

protected:

	// Holds the session manager singleton.
	static IDeviceProfileServicesUIManagerPtr DeviceProfileServicesUIManagerSingleton;
};


/* Static initialization
 *****************************************************************************/

IDeviceProfileServicesUIManagerPtr FDeviceProfileServicesModule::DeviceProfileServicesUIManagerSingleton = NULL;

IMPLEMENT_MODULE(FDeviceProfileServicesModule, DeviceProfileServices);
