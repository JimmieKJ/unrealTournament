// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "IOSTargetPlatform.h"
#include "Interfaces/ITargetPlatformModule.h"

/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* Singleton = NULL;


/**
 * Module for TVOS as a target platform
 */
class FTVOSTargetPlatformModule : public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FTVOSTargetPlatformModule()
	{
		Singleton = NULL;
	}


public:

	// Begin ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform()
	{
		if (Singleton == NULL)
		{
			Singleton = new FIOSTargetPlatform(true);
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

IMPLEMENT_MODULE( FTVOSTargetPlatformModule, TVOSTargetPlatform);
