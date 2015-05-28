// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "LogVisualizerStyle.h"
#include "SDockTab.h"
#include "VisualLoggerRenderingActor.h"
#include "LogVisualizerSettings.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "ISettingsModule.h"
#include "LevelEditor.h"
#include "WorkspaceMenuStructureModule.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "FLogVisualizerModule"

static const FName VisualLoggerTabName("VisualLogger");

//DEFINE_LOG_CATEGORY(LogLogVisualizer);

class FLogVisualizerModule : public ILogVisualizer
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface

	virtual void Goto(float Timestamp, FName LogOwner) override;
	virtual void GotoNextItem() override;
	virtual void GotoPreviousItem() override;

private:
	TSharedRef<SDockTab> SpawnLogVisualizerTab(const FSpawnTabArgs& SpawnTabArgs);
};

void FLogVisualizerModule::StartupModule()
{
	FLogVisualizerStyle::Initialize();
	FLogVisualizer::Initialize();

	FVisualLoggerCommands::Register();
	IModularFeatures::Get().RegisterModularFeature(VisualLoggerTabName, this);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		VisualLoggerTabName, 
		FOnSpawnTab::CreateRaw(this, &FLogVisualizerModule::SpawnLogVisualizerTab))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory())
		.SetDisplayName(NSLOCTEXT("LogVisualizerApp", "TabTitle", "Visual Logger"))
		.SetTooltipText(NSLOCTEXT("LogVisualizerApp", "TooltipText", "Opens Visual Logger tool."))
		.SetIcon(FSlateIcon(FLogVisualizerStyle::GetStyleSetName(), "LogVisualizerApp.TabIcon"));

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule != nullptr)
	{
		SettingsModule->RegisterSettings("Editor", "General", "VisualLogger",
			LOCTEXT("AIToolsSettingsName", "Visual Logger"),
			LOCTEXT("AIToolsSettingsDescription", "General settings for UE4 AI Tools."),
			ULogVisualizerSettings::StaticClass()->GetDefaultObject()
			);
	}
}

void FLogVisualizerModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterTabSpawner(VisualLoggerTabName);
	FVisualLoggerCommands::Unregister();
	IModularFeatures::Get().UnregisterModularFeature(VisualLoggerTabName, this);
	
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule != nullptr)
	{
		SettingsModule->UnregisterSettings("Editor", "General", "VisualLogger");
	}

	FLogVisualizer::Shutdown();
	FLogVisualizerStyle::Shutdown();
}

TSharedRef<SDockTab> FLogVisualizerModule::SpawnLogVisualizerTab(const FSpawnTabArgs& SpawnTabArgs)
{
	const TSharedRef<SDockTab> MajorTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);

	TSharedPtr<SWidget> TabContent;

	TabContent = SNew(SVisualLogger, MajorTab, SpawnTabArgs.GetOwnerWindow());

	MajorTab->SetContent(TabContent.ToSharedRef());

	return MajorTab;
}

void FLogVisualizerModule::Goto(float Timestamp, FName LogOwner)
{
	FLogVisualizer::Get().Goto(Timestamp, LogOwner);
}

void FLogVisualizerModule::GotoNextItem()
{
	FLogVisualizer::Get().GotoNextItem();
}

void FLogVisualizerModule::GotoPreviousItem()
{
	FLogVisualizer::Get().GotoPreviousItem();
}


IMPLEMENT_MODULE(FLogVisualizerModule, LogVisualizer);
#undef LOCTEXT_NAMESPACE
