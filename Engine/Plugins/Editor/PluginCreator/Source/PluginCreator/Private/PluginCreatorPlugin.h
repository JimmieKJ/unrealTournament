// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
 
#include "ModuleManager.h"
#include "LevelEditor.h"
#include "SlateBasics.h"


DECLARE_LOG_CATEGORY_EXTERN(PluginCreatorPluginLog, Log, All);
DEFINE_LOG_CATEGORY(PluginCreatorPluginLog);

class FPluginCreatorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void MyButtonClicked();
	

private: 
	void AddMenuExtension(FMenuBuilder& Builder);

	TSharedRef<SDockTab> HandleSpawnPluginCreatorTab(const FSpawnTabArgs& SpawnTabArgs);

	TSharedPtr<FTabManager> TabManager;

	TSharedPtr<FUICommandList> MyPluginCommands;
	TSharedPtr<FExtensibilityManager> MyExtensionManager;
	TSharedPtr< const FExtensionBase > ToolbarExtension;
	TSharedPtr<FExtender> ToolbarExtender;
};

