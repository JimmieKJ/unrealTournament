// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DesktopTargetPlatformPrivatePCH.h"

/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* Singleton = NULL;


/**
 * Module for a generic target platform for desktop platforms
 */
class FDesktopTargetPlatformModule : public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FDesktopTargetPlatformModule()
	{
		Singleton = NULL;
	}


public:

	// Begin ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform() override
	{
		if (Singleton == NULL)
		{
			Singleton = new FDesktopTargetPlatform();
		}

		return Singleton;
	}

	// End ITargetPlatformModule interface


public:

	// Begin IModuleInterface interface
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
	// End IModuleInterface interface
};

IMPLEMENT_MODULE( FDesktopTargetPlatformModule, DesktopTargetPlatform);
