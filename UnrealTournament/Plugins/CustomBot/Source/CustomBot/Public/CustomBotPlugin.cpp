// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CustomBotPrivatePCH.h"

#include "Core.h"
#include "Engine.h"
#include "ModuleManager.h"
#include "ModuleInterface.h"
#if WITH_EDITOR
#include "Settings/EditorExperimentalSettings.h"
#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY(LogUTCustomBot	);

class FCustomBotPlugin : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FCustomBotPlugin, CustomBot)

void FCustomBotPlugin::StartupModule()
{
#if WITH_EDITOR
	FModuleManager::Get().LoadModule(TEXT("EnvironmentQueryEditor"));
	// force-enabling EQS editor 
	GetMutableDefault<UEditorExperimentalSettings>()->bEQSEditor = true;
#endif // WITH_EDITOR
}


void FCustomBotPlugin::ShutdownModule()
{

}



