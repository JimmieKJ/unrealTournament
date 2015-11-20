// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the Linux Device Profile Selector module.
 */
class FLinuxDeviceProfileSelectorModule
	: public IDeviceProfileSelectorModule
{
public:

	// Begin IDeviceProfileSelectorModule interface
	virtual const FString GetRuntimeDeviceProfileName() override;
	// End IDeviceProfileSelectorModule interface


	// Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface interface

	
	/**
	 * Virtual destructor.
	 */
	virtual ~FLinuxDeviceProfileSelectorModule()
	{
	}
};