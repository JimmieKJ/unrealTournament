// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/ITargetDeviceServicesModule.h"
#include "Interfaces/ILauncherServicesModule.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Textures/SlateIcon.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "Interfaces/IProjectLauncherModule.h"
#include "Models/ProjectLauncherModel.h"
#include "Widgets/SProjectLauncher.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

static const FName ProjectLauncherTabName("ProjectLauncher");

/**
 * Implements the SessionSProjectLauncher module.
 */
class FProjectLauncherModule
	: public IProjectLauncherModule
{
public:

	// IModuleInterface interface
	
	virtual void StartupModule( ) override
	{
#if WITH_EDITOR
		FGlobalTabmanager::Get()->RegisterTabSpawner(ProjectLauncherTabName, FOnSpawnTab::CreateRaw(this, &FProjectLauncherModule::SpawnProjectLauncherTab));
#else
		// This is still experimental in the editor, so it'll be invoked specifically in FMainMenu if the experimental settings flag is set.
		//@todo Enable this in the editor when no longer experimental
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ProjectLauncherTabName, FOnSpawnTab::CreateRaw(this, &FProjectLauncherModule::SpawnProjectLauncherTab))
			.SetDisplayName(NSLOCTEXT("FProjectLauncherModule", "ProjectLauncherTabTitle", "Project Launcher"))
			.SetTooltipText(NSLOCTEXT("FProjectLauncherModule", "ProjectLauncherTooltipText", "Open the Project Launcher tab."))
			.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Launcher.TabIcon"))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
#endif
	}

	virtual void ShutdownModule( ) override
	{
#if WITH_EDITOR
		FGlobalTabmanager::Get()->UnregisterTabSpawner(ProjectLauncherTabName);
#else
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ProjectLauncherTabName);
#endif
	}

private:

	/**
	 * Creates a new session launcher tab.
	 *
	 * @param SpawnTabArgs The arguments for the tab to spawn.
	 * @return The spawned tab.
	 */
	TSharedRef<SDockTab> SpawnProjectLauncherTab(const FSpawnTabArgs& SpawnTabArgs)
	{
		const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
			.Icon(FEditorStyle::GetBrush("Launcher.TabIcon"))
			.TabRole(ETabRole::NomadTab);

		ILauncherServicesModule& ProjectLauncherServicesModule = FModuleManager::LoadModuleChecked<ILauncherServicesModule>("LauncherServices");
		ITargetDeviceServicesModule& TargetDeviceServicesModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>("TargetDeviceServices");

		FProjectLauncherModelRef Model = MakeShareable(new FProjectLauncherModel(
			TargetDeviceServicesModule.GetDeviceProxyManager(),
			ProjectLauncherServicesModule.CreateLauncher(),
			ProjectLauncherServicesModule.GetProfileManager()
		));

		DockTab->SetContent(SNew(SProjectLauncher, DockTab, SpawnTabArgs.GetOwnerWindow(), Model));

		return DockTab;
	}
};


IMPLEMENT_MODULE(FProjectLauncherModule, ProjectLauncher);
