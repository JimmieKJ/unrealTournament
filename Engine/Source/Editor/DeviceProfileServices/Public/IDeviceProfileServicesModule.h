// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Device Profile Editor module
 */
class IDeviceProfileServicesModule 
	: public IModuleInterface
{
public:

	/**
	 * Gets the profile services manager.
	 *
	 * @return The profile services manager.
	 */
	virtual IDeviceProfileServicesUIManagerRef GetProfileServicesManager( ) = 0;

public:

	/** Virtual destructor. */
	virtual ~IDeviceProfileServicesModule( ) { }
};
