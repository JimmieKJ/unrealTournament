// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AndroidTargetPlatformPrivatePCH.h"

#define LOCTEXT_NAMESPACE "FAndroidTargetPlatformModule"

/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* AndroidTargetSingleton = NULL;

/**
 * Module for the Android target platform.
 */
class FAndroidTargetPlatformModule : public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FAndroidTargetPlatformModule( )
	{
		AndroidTargetSingleton = NULL;
	}


public:
	
	// Begin ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform() override
	{
		if (AndroidTargetSingleton == NULL)
		{
			AndroidTargetSingleton = new FAndroidTargetPlatform<FAndroidPlatformProperties>();	
		}
		
		return AndroidTargetSingleton;
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


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FAndroidTargetPlatformModule, AndroidTargetPlatform);
