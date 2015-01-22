// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenShotToolsModule.cpp: Implements the FScreenShotToolsModule class.
=============================================================================*/

#include "ScreenShotComparisonToolsPrivatePCH.h"

#include "ModuleManager.h"


/**
 * Implements the ScreenshotTools module.
 */
class FScreenShotToolsModule
	: public IScreenShotToolsModule
{
public:

	// Begin IScreenShotToolsModule interface

	virtual IScreenShotManagerPtr GetScreenShotManager( ) override
	{
		return ScreenShotManager;
	}

	virtual void UpdateScreenShotData( ) override
	{
		if (ScreenShotManager.IsValid())
		{
			// Create the screen shot data
			ScreenShotManager->GenerateLists();
		}
	}

	// End IScreenShotToolsModule interface

public:

	/**
	 * Shutdown the screen shot manager tools module
	 */
	virtual void ShutdownModule( ) override
	{
		ScreenShotManager.Reset();
	}

	/**
	* Startup the screen shot manager tools module
	*/
	void StartupModule() override
	{
		IMessageBusPtr MessageBus = IMessagingModule::Get().GetDefaultBus();
		check(MessageBus.IsValid());

		// Create the screen shot manager
		if (!ScreenShotManager.IsValid())
		{
			ScreenShotManager = MakeShareable(new FScreenShotManager(MessageBus.ToSharedRef()));
		}

		UpdateScreenShotData();
	}

private:
	
	// Holds the screen shot manager.
	IScreenShotManagerPtr ScreenShotManager;
};


IMPLEMENT_MODULE(FScreenShotToolsModule, ScreenShotComparisonTools);
