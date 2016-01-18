// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HTML5PlatformEditorPrivatePCH.h"
#include "ModuleInterface.h"
#include "ISettingsModule.h"
#include "ModuleManager.h"
#include "HTML5SDKSettings.h"


#define LOCTEXT_NAMESPACE "FHTML5PlatformEditorModule"


/**
 * Module for Android platform editor utilities
 */
class FHTML5PlatformEditorModule
	: public IModuleInterface
{
	// IModuleInterface interface

	virtual void StartupModule() override
	{
#if PLATFORM_WINDOWS
		FString SDKPath = FPaths::EngineDir() / TEXT("Source") / TEXT("ThirdParty") / TEXT("HTML5") / TEXT("emsdk") / TEXT("Win64");
#elif PLATFORM_MAC
		FString SDKPath = FPaths::EngineDir() / TEXT("Source") / TEXT("ThirdParty") / TEXT("HTML5") / TEXT("emsdk") / TEXT("Mac");
#elif PLATFORM_LINUX
		FString SDKPath = FPaths::EngineDir() / TEXT("Source") / TEXT("ThirdParty") / TEXT("HTML5") / TEXT("emsdk") / TEXT("Linux");
#else 
		return; 
#endif 
		// we don't have the SDK, don't bother setting this up. 
		if (!IFileManager::Get().DirectoryExists(*SDKPath))
		{
			return; 
		}

		// register settings6
		static FName PropertyEditor("PropertyEditor");
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "HTML5",
				LOCTEXT("TargetSettingsName", "HTML5"),
				LOCTEXT("SDKSettingsDescription", "Settings for HTML5"),
				GetMutableDefault<UHTML5TargetSettings>()
			);

 			SettingsModule->RegisterSettings("Project", "Platforms", "HTML5SDK",
 				LOCTEXT("SDKSettingsName", "HTML5 SDK"),
 				LOCTEXT("SDKSettingsDescription", "Settings for HTML5 SDK (system wide)"),
 				GetMutableDefault<UHTML5SDKSettings>()
			);
		}
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "HTML5");
			SettingsModule->UnregisterSettings("Project", "Platforms", "HTML5SDK");
		}
	}
};


IMPLEMENT_MODULE(FHTML5PlatformEditorModule, HTML5PlatformEditor);

#undef LOCTEXT_NAMESPACE
