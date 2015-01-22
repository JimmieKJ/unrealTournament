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
	: public ITargetPlatformModule
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


public:

	// Begin IModuleInterface interface

	virtual void StartupModule() override
	{
		TargetSettings = ConstructObject<UHTML5TargetSettings>(UHTML5TargetSettings::StaticClass(), GetTransientPackage(), "HTML5TargetSettings", RF_Standalone);
		TargetSettings->AddToRoot();

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "HTML5",
				LOCTEXT("TargetSettingsName", "HTML5"),
				LOCTEXT("TargetSettingsDescription", "HTML5 platform settings description text here"),
				TargetSettings
			);
		}
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "HTML5");
		}

		if (!GExitPurge)
		{
			// If we're in exit purge, this object has already been destroyed
			TargetSettings->RemoveFromRoot();
		}
		else
		{
			TargetSettings = NULL;
		}
	}

	// End IModuleInterface interface


private:

	// Holds the target settings.
	UHTML5TargetSettings* TargetSettings;
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FHTML5TargetPlatformModule, HTML5TargetPlatform);
