// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginCreatorPrivatePCH.h"

#include "PluginCreatorPlugin.h"
#include "SPluginCreatorTabContent.h"
#include "SlateExtras.h"
#include "PluginCreatorStyle.h"

#include "SLayoutCreator.h"

#include "MultiBoxExtender.h"

static const FName PCPluginTabName("PluginCreatorPlugin");

#define LOCTEXT_NAMESPACE "PluginCreatorPluginModule"

#include "PluginCreatorCommands.h"

void FPluginCreatorModule::StartupModule()
{
	FPluginCreatorStyle::Initialize();
	FPluginCreatorStyle::ReloadTextures();

	FPluginCreatorCommands::Register();
	

	MyPluginCommands = MakeShareable(new FUICommandList);

	MyPluginCommands->MapAction(
		FPluginCreatorCommands::Get().OpenPluginCreator,
		FExecuteAction::CreateRaw(this, &FPluginCreatorModule::MyButtonClicked),
		FCanExecuteAction());

	TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
	MenuExtender->AddMenuExtension("FileProject", EExtensionHook::After, MyPluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FPluginCreatorModule::AddMenuExtension));

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		PCPluginTabName,
		FOnSpawnTab::CreateRaw(this, &FPluginCreatorModule::HandleSpawnPluginCreatorTab))
		.SetDisplayName(LOCTEXT("PluginCreatorTabTitle", "Plugin Creator"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FPluginCreatorModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FPluginCreatorCommands::Get().OpenPluginCreator);
}

TSharedRef<SDockTab> FPluginCreatorModule::HandleSpawnPluginCreatorTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedRef<SDockTab> ResultTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);

	TSharedRef<SWidget> TabContentWidget = SNew(SPluginCreatorTabContent, ResultTab);
	ResultTab->SetContent(TabContentWidget);

	return ResultTab;
}

void FPluginCreatorModule::MyButtonClicked() 
{
	FGlobalTabmanager::Get()->InvokeTab(PCPluginTabName);
}

void FPluginCreatorModule::ShutdownModule()
{
	FPluginCreatorStyle::Shutdown();

	FPluginCreatorCommands::Unregister();


	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PCPluginTabName);

}

FString GetPluginCreatorRootPath()
{
	return FPaths::EnginePluginsDir() / TEXT("Editor/PluginCreator");
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPluginCreatorModule, PluginCreatorPlugin)