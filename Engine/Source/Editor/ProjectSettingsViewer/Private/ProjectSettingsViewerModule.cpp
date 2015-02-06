// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProjectSettingsViewerPrivatePCH.h"
#include "Engine/Console.h"
#include "ProjectTargetPlatformEditor.h"
#include "ISettingsCategory.h"
#include "ISettingsContainer.h"
#include "ISettingsEditorModel.h"
#include "ISettingsEditorModule.h"
#include "ISettingsModule.h"
#include "ISettingsViewer.h"
#include "ModuleManager.h"
#include "SDockTab.h"

#include "CookerSettings.h"
#include "Runtime/Engine/Classes/Engine/RendererSettings.h"
#include "Runtime/Engine/Classes/Engine/UserInterfaceSettings.h"
#include "Runtime/Engine/Classes/Sound/AudioSettings.h"
#include "Runtime/AIModule/Classes/Navigation/CrowdManager.h"
#include "AISystem.h"

#define LOCTEXT_NAMESPACE "FProjectSettingsViewerModule"

static const FName ProjectSettingsTabName("ProjectSettings");


/**
 * Implements the ProjectSettingsViewer module.
 */
class FProjectSettingsViewerModule
	: public IModuleInterface
	, public ISettingsViewer
{
public:

	// ISettingsViewer interface

	virtual void ShowSettings( const FName& CategoryName, const FName& SectionName ) override
	{
		FGlobalTabmanager::Get()->InvokeTab(ProjectSettingsTabName);
		ISettingsEditorModelPtr SettingsEditorModel = SettingsEditorModelPtr.Pin();

		if (SettingsEditorModel.IsValid())
		{
			ISettingsCategoryPtr Category = SettingsEditorModel->GetSettingsContainer()->GetCategory(CategoryName);

			if (Category.IsValid())
			{
				SettingsEditorModel->SelectSection(Category->GetSection(SectionName));
			}
		}
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			RegisterEngineSettings(*SettingsModule);
			RegisterProjectSettings(*SettingsModule);

			SettingsModule->RegisterViewer("Project", *this);
		}

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ProjectSettingsTabName, FOnSpawnTab::CreateRaw(this, &FProjectSettingsViewerModule::HandleSpawnSettingsTab))
			.SetDisplayName(LOCTEXT("ProjectSettingsTabTitle", "Project Settings"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);
	}

	virtual void ShutdownModule() override
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ProjectSettingsTabName);
		UnregisterSettings();
	}

	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}

protected:

	/**
	 * Registers Engine settings.
	 *
	 * @param SettingsModule A reference to the settings module.
	 */
	void RegisterEngineSettings( ISettingsModule& SettingsModule )
	{
		// startup settings
		SettingsModule.RegisterSettings("Project", "Engine", "General",
			LOCTEXT("GeneralEngineSettingsName", "General Settings"),
			LOCTEXT("ProjectGeneralSettingsDescription", "General options and defaults for the game engine."),
			GetMutableDefault<UEngine>()
		);

		// audio settings
		SettingsModule.RegisterSettings("Project", "Engine", "Audio",
			LOCTEXT("EngineAudioSettingsName", "Audio"),
			LOCTEXT("ProjectAudioSettingsDescription", "Audio settings."),
			GetMutableDefault<UAudioSettings>()
		);

		// collision settings
		SettingsModule.RegisterSettings("Project", "Engine", "Collision",
			LOCTEXT("ProjectCollisionSettingsName", "Collision"),
			LOCTEXT("ProjectCollisionSettingsDescription", "Set up and modify collision settings."),
			GetMutableDefault<UCollisionProfile>()
		);

		// command console settings
		SettingsModule.RegisterSettings("Project", "Engine", "Console",
			LOCTEXT("ProjectConsoleSettingsName", "Console"),
			LOCTEXT("ProjectConsoleSettingsDescription", "Configure the in-game input console."),
			GetMutableDefault<UConsoleSettings>()
		);

		// input settings
		SettingsModule.RegisterSettings("Project", "Engine", "Input",
			LOCTEXT("EngineInputSettingsName", "Input"),
			LOCTEXT("ProjectInputSettingsDescription", "Input settings, including default input action and axis bindings."),
			GetMutableDefault<UInputSettings>()
		);

		// navigation system's class can be game specific so we need to find appropriate CDO
		UNavigationSystem* NavSysCDO = (*GEngine->NavigationSystemClass != nullptr)
			? GetMutableDefault<UNavigationSystem>(GEngine->NavigationSystemClass)
			: GetMutableDefault<UNavigationSystem>();
		SettingsModule.RegisterSettings("Project", "Engine", "NavigationSystem",
			LOCTEXT("NavigationSystemSettingsName", "Navigation System"),
			LOCTEXT("NavigationSystemSettingsDescription", "Settings for the navigation system."),
			NavSysCDO
		);

		// navigation mesh
		SettingsModule.RegisterSettings("Project", "Engine", "NavigationMesh",
			LOCTEXT("NavigationMeshSettingsName", "Navigation Mesh"),
			LOCTEXT("NavigationMeshSettingsDescription", "Settings for the navigation mesh."),
			GetMutableDefault<ARecastNavMesh>()
		);

		// AI system
		SettingsModule.RegisterSettings("Project", "Engine", "AISystem",
			LOCTEXT("AISystemSettingsName", "AI System"),
			LOCTEXT("AISystemSettingsDescription", "Settings for the AI System."),
			GetMutableDefault<UAISystem>()
			);

		// Crowd manager
		SettingsModule.RegisterSettings("Project", "Engine", "CrowdManager",
			LOCTEXT("CrowdManagerSettingsName", "Crowd Manager"),
			LOCTEXT("CrowdManagerSettingsDescription", "Settings for the AI Crowd Manager."),
			GetMutableDefault<UCrowdManager>()
			);

		/*
		// network settings
		SettingsModule.RegisterSettings("Project", "Engine", "NetworkManager",
			LOCTEXT("GameNetworkManagerSettingsName", "Network Manager"),
			LOCTEXT("GameNetworkManagerSettingsDescription", "Game Network Manager settings."),
			GetMutableDefault<UGameNetworkManagerSettings>()
		);*/

		// network settings
		SettingsModule.RegisterSettings("Project", "Engine", "Network",
			LOCTEXT("NetworkSettingsName", "Network"),
			LOCTEXT("NetworkSettingsDescription", "Network settings."),
			GetMutableDefault<UNetworkSettings>()
		);

		// Physics settings
		SettingsModule.RegisterSettings("Project", "Engine", "Physics",
			LOCTEXT("EnginePhysicsSettingsName", "Physics"),
			LOCTEXT("ProjectPhysicsSettingsDescription", "Default physics settings."),
			GetMutableDefault<UPhysicsSettings>()
		);

		// Rendering settings
		SettingsModule.RegisterSettings("Project", "Engine", "Rendering",
			LOCTEXT("EngineRenderingSettingsName", "Rendering"),
			LOCTEXT("ProjectRenderingSettingsDescription", "Rendering settings."),
			GetMutableDefault<URendererSettings>()
		);

		// UI settings
		SettingsModule.RegisterSettings("Project", "Engine", "UI",
			LOCTEXT("EngineUISettingsName", "User Interface"),
			LOCTEXT("ProjectUISettingsDescription", "User Interface settings that control Slate and UMG."),
			GetMutableDefault<UUserInterfaceSettings>()
		);

		// Cooker settings
		SettingsModule.RegisterSettings("Project", "Engine", "Cooker",
			LOCTEXT("CookerSettingsName", "Cooker"),
			LOCTEXT("CookerSettingsDescription", "Various cooker settings."),
			GetMutableDefault<UCookerSettings>()
			);
	}

	/**
	 * Registers Project settings.
	 *
	 * @param SettingsModule A reference to the settings module.
	 */
	void RegisterProjectSettings( ISettingsModule& SettingsModule )
	{
		// general project settings
		SettingsModule.RegisterSettings("Project", "Project", "General",
			LOCTEXT("GeneralGameSettingsName", "Description"),
			LOCTEXT("GeneralGameSettingsDescription", "Descriptions and other information about your project."),
			GetMutableDefault<UGeneralProjectSettings>()
		);

		// map related settings
		SettingsModule.RegisterSettings("Project", "Project", "Maps",
			LOCTEXT("GameMapsSettingsName", "Maps & Modes"),
			LOCTEXT("GameMapsSettingsDescription", "Default maps, game modes and other map related settings."),
			GetMutableDefault<UGameMapsSettings>()
		);

		// packaging settings
		SettingsModule.RegisterSettings("Project", "Project", "Packaging",
			LOCTEXT("ProjectPackagingSettingsName", "Packaging"),
			LOCTEXT("ProjectPackagingSettingsDescription", "Fine tune how your project is packaged for release."),
			GetMutableDefault<UProjectPackagingSettings>()
		);

		// platforms settings
		TWeakPtr<SWidget> ProjectTargetPlatformEditorPanel = FModuleManager::LoadModuleChecked<IProjectTargetPlatformEditorModule>("ProjectTargetPlatformEditor").CreateProjectTargetPlatformEditorPanel();
		SettingsModule.RegisterSettings("Project", "Project", "SupportedPlatforms",
			LOCTEXT("ProjectSupportedPlatformsSettingsName", "Supported Platforms"),
			LOCTEXT("ProjectSupportedPlatformsSettingsDescription", "Specify which platforms your project supports."),
			ProjectTargetPlatformEditorPanel.Pin().ToSharedRef()
		);

		// movie settings
		SettingsModule.RegisterSettings("Project", "Project", "Movies",
			LOCTEXT("MovieSettingsName", "Movies"),
			LOCTEXT("MovieSettingsDescription", "Movie player settings"),
			GetMutableDefault<UMoviePlayerSettings>()
		);
/*
		// game session
		SettingsModule.RegisterSettings("Project", "Project", "GameSession",
			LOCTEXT("GameSessionettingsName", "Game Session"),
			LOCTEXT("GameSessionSettingsDescription", "Game Session settings."),
			GetMutableDefault<UGameSessionSettings>()
		);

		// head-up display
		SettingsModule.RegisterSettings("Project", "Project", "HUD",
			LOCTEXT("HudSettingsName", "HUD"),
			LOCTEXT("HudSettingsDescription", "Head-up display (HUD) settings."),
			GetMutableDefault<UHudSettings>()
		);*/
	}

	/** Unregisters all settings. */
	void UnregisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterViewer("Project");

			// engine settings
			SettingsModule->UnregisterSettings("Project", "Engine", "General");
			SettingsModule->UnregisterSettings("Project", "Engine", "CrowdManager");
			SettingsModule->UnregisterSettings("Project", "Engine", "NavigationSystem");
			SettingsModule->UnregisterSettings("Project", "Engine", "NavigationMesh");
			SettingsModule->UnregisterSettings("Project", "Engine", "Input");
			SettingsModule->UnregisterSettings("Project", "Engine", "Collision");
			SettingsModule->UnregisterSettings("Project", "Engine", "Physics");
			SettingsModule->UnregisterSettings("Project", "Engine", "Rendering");
//			SettingsModule->UnregisterSettings("Project", "Engine", "NetworkManager");

			// project settings
			SettingsModule->UnregisterSettings("Project", "Project", "General");
			SettingsModule->UnregisterSettings("Project", "Project", "Maps");
			SettingsModule->UnregisterSettings("Project", "Project", "Packaging");
			SettingsModule->UnregisterSettings("Project", "Project", "SupportedPlatforms");
			SettingsModule->UnregisterSettings("Project", "Project", "Movies");
//			SettingsModule->UnregisterSettings("Project", "Project", "GameSession");
//			SettingsModule->UnregisterSettings("Project", "Project", "HUD");
		}
	}

private:

	/** Handles creating the project settings tab. */
	TSharedRef<SDockTab> HandleSpawnSettingsTab( const FSpawnTabArgs& SpawnTabArgs )
	{
		TSharedRef<SWidget> SettingsEditor = SNullWidget::NullWidget;
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			ISettingsContainerPtr SettingsContainer = SettingsModule->GetContainer("Project");

			if (SettingsContainer.IsValid())
			{
				ISettingsEditorModule& SettingsEditorModule = FModuleManager::GetModuleChecked<ISettingsEditorModule>("SettingsEditor");
				ISettingsEditorModelRef SettingsEditorModel = SettingsEditorModule.CreateModel(SettingsContainer.ToSharedRef());

				SettingsEditor = SettingsEditorModule.CreateEditor(SettingsEditorModel);
				SettingsEditorModelPtr = SettingsEditorModel;
			}
		}

		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SettingsEditor
			];
	}

private:

	/** Holds a pointer to the settings editor's view model. */
	TWeakPtr<ISettingsEditorModel> SettingsEditorModelPtr;
};


IMPLEMENT_MODULE(FProjectSettingsViewerModule, ProjectSettingsViewer);


#undef LOCTEXT_NAMESPACE
