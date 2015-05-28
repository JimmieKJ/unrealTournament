// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MacTargetPlatformPrivatePCH.h"
#include "ModuleManager.h"
#include "ISettingsModule.h"


#define LOCTEXT_NAMESPACE "FMacTargetPlatformModule"


/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* Singleton = NULL;


/**
 * Module for Mac as a target platform
 */
class FMacTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FMacTargetPlatformModule()
	{
		Singleton = NULL;
	}


public:

	// Begin ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform() override
	{
		if (Singleton == NULL)
		{
			Singleton = new TGenericMacTargetPlatform<true, false, false>();
		}

		return Singleton;
	}

	// End ITargetPlatformModule interface


public:

	// Begin IModuleInterface interface

	virtual void StartupModule() override
	{
		TargetSettings = NewObject<UMacTargetSettings>(GetTransientPackage(), "MacTargetSettings", RF_Standalone);
		TargetSettings->AddToRoot();

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "Mac",
				LOCTEXT("TargetSettingsName", "Mac"),
				LOCTEXT("TargetSettingsDescription", "Settings and resources for Mac platform"),
				TargetSettings
			);
		}
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "Mac");
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
	UMacTargetSettings* TargetSettings;
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FMacTargetPlatformModule, MacTargetPlatform);
