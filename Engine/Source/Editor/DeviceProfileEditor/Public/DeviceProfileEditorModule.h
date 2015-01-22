// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

/**
 * Device Profile Editor module
 */
class FDeviceProfileEditorModule : public IModuleInterface
{

public:

	// Begin IModuleInterface interface
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;
	// End IModuleInterface interface

public:
	/**
	 * Create the slate UI for the Device Profile Editor
	 */
	static TSharedRef<SDockTab> SpawnDeviceProfileEditorTab( const FSpawnTabArgs& SpawnTabArgs );
};