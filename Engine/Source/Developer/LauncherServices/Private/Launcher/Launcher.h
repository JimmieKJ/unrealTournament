// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the game launcher.
 */
class FLauncher
	: public ILauncher
{
public:

	// Begin ILauncher interface

	virtual ILauncherWorkerPtr Launch( const ITargetDeviceProxyManagerRef& DeviceProxyManager, const ILauncherProfileRef& Profile ) override;

	// End ILauncher interface
};
