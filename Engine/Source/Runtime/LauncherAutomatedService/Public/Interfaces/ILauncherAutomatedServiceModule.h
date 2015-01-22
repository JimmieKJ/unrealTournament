// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Interface for launcher tools modules.
 */
class ILauncherAutomatedServiceModule
	: public IModuleInterface
{
public:
	
	/**
	 * Creates a service for running the launcher as an automated process.
	 *
	 * @return The automation service provider which runs the launcher commands automatically.
	 */
	virtual ILauncherAutomatedServiceProviderPtr CreateAutomatedServiceProvider() = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~ILauncherAutomatedServiceModule( ) { }
};
