// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GameCircleRuntimeSettingsPrivatePCH.h"
#include "GameCircleRuntimeSettings.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "GameCircleRuntimeSettings"

class FGameCircleRuntimeSettingsModule : public IModuleInterface
{
public:

#if WITH_EDITOR
	/**
	* Called right after the module DLL has been loaded and the module object has been created
	*/
	virtual void StartupModule()
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "GameCircle",
				LOCTEXT("RuntimeSettingsName", "Amazon GameCircle"),
				LOCTEXT("RuntimeSettingsDescription", "Configure the Amazon GameCircle plugin"),
				GetMutableDefault<UGameCircleRuntimeSettings>());
		}
	}

	/**
	* Called before the module has been unloaded
	*/
	virtual void ShutdownModule()
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "GameCircle");
		}
	}
#endif

};

IMPLEMENT_MODULE(FGameCircleRuntimeSettingsModule, GameCircleRuntimeSettings)
