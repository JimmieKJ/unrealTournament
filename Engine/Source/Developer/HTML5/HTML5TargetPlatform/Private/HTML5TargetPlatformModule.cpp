// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HTML5TargetPlatformPrivatePCH.h"
#include "ISettingsModule.h"
#include "ModuleManager.h"


#define LOCTEXT_NAMESPACE "FHTML5TargetPlatformModule"


// Holds the target platform singleton.
static ITargetPlatform* Singleton = NULL;

/**
 * Module for the HTML5 target platform.
 */
class FHTML5TargetPlatformModule
	: public IHTML5TargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FHTML5TargetPlatformModule( )
	{
		Singleton = NULL;
	}
	

public:

	// Begin ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform( ) override
	{
		if (Singleton == NULL)
		{
			// finally, make the interface object
			Singleton = new FHTML5TargetPlatform();
			FString OutPath;
			if (!Singleton->IsSdkInstalled(false, OutPath))
			{
				delete Singleton;
				Singleton = NULL;
			}
		}

		return Singleton;
	}

	// End ITargetPlatformModule interface

	virtual void RefreshAvailableDevices() override
	{
		if (Singleton)
		{
			((FHTML5TargetPlatform*)Singleton)->RefreshHTML5Setup();
		}
	}

public:

	// Begin IModuleInterface interface

	virtual void StartupModule() override
	{

	}

	virtual void ShutdownModule() override
	{

	}

	// End IModuleInterface interface


private:

};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FHTML5TargetPlatformModule, HTML5TargetPlatform);
