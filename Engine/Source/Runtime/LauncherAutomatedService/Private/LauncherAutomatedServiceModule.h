// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the launchers automated service.
 */
class FLauncherAutomatedServiceModule
	: public ILauncherAutomatedServiceModule
{
public:

	// ILauncherAutomatedServiceModule interface

	virtual ILauncherAutomatedServiceProviderPtr CreateAutomatedServiceProvider() override;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~FLauncherAutomatedServiceModule( ) { }
};
