// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EditorSettingsViewerPrivatePCH.h"
#include "ISettingsCategory.h"
#include "ISettingsContainer.h"
#include "ISettingsEditorModel.h"
#include "ISettingsEditorModule.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "ISettingsViewer.h"
#include "ModuleManager.h"
#include "SDockTab.h"

#include "Tests/AutomationTestSettings.h"
#include "BlueprintEditorSettings.h"

#include "CrashReporterSettings.h"
#include "Analytics/AnalyticsPrivacySettings.h"

#define LOCTEXT_NAMESPACE "FEditorSettingsViewerModule"

static const FName EditorSettingsTabName("EditorSettings");


/**
 * Implements the EditorSettingsViewer module.
 */
class FEditorSettingsViewerModule
	: public IModuleInterface
	, public ISettingsViewer
{
public:

	// ISettingsViewer interface

	virtual void ShowSettings( const FName& CategoryName, const FName& SectionName ) override
	{
		FGlobalTabmanager::Get()->InvokeTab(EditorSettingsTabName);
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
			RegisterGeneralSettings(*SettingsModule);
			RegisterLevelEditorSettings(*SettingsModule);
			RegisterContentEditorsSettings(*SettingsModule);
			RegisterPrivacySettings(*SettingsModule);

			SettingsModule->RegisterViewer("Editor", *this);
		}

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(EditorSettingsTabName, FOnSpawnTab::CreateRaw(this, &FEditorSettingsViewerModule::HandleSpawnSettingsTab))
			.SetDisplayName(LOCTEXT("EditorSettingsTabTitle", "Editor Preferences"))
			.SetMenuType(ETabSpawnerMenuType::Hidden)
			.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "EditorPreferences.TabIcon"));
	}

	virtual void ShutdownModule() override
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(EditorSettingsTabName);
		UnregisterSettings();
	}

	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}

protected:

	/**
	 * Registers general Editor settings.
	 *
	 * @param SettingsModule A reference to the settings module.
	 */
	void RegisterGeneralSettings( ISettingsModule& SettingsModule )
	{
		// automation
		SettingsModule.RegisterSettings("Editor", "Advanced", "AutomationTest",
			LOCTEXT("AutomationSettingsName", "Automation"),
			LOCTEXT("AutomationSettingsDescription", "Set up automation test assets."),
			GetMutableDefault<UAutomationTestSettings>()
		);

		// region & language
		ISettingsSectionPtr RegionAndLanguageSettings = SettingsModule.RegisterSettings("Editor", "General", "Internationalization",
			LOCTEXT("InternationalizationSettingsModelName", "Region & Language"),
			LOCTEXT("InternationalizationSettingsModelDescription", "Configure the editor's behavior to use a language and fit a region's culture."),
			GetMutableDefault<UInternationalizationSettingsModel>()
		);

		// input bindings
		TWeakPtr<SWidget> InputBindingEditorPanel = FModuleManager::LoadModuleChecked<IInputBindingEditorModule>("InputBindingEditor").CreateInputBindingEditorPanel();
		ISettingsSectionPtr InputBindingSettingsSection = SettingsModule.RegisterSettings("Editor", "General", "InputBindings",
			LOCTEXT("InputBindingsSettingsName", "Keyboard Shortcuts"),
			LOCTEXT("InputBindingsSettingsDescription", "Configure keyboard shortcuts to quickly invoke operations."),
			InputBindingEditorPanel.Pin().ToSharedRef()
		);

		if (InputBindingSettingsSection.IsValid())
		{
			InputBindingSettingsSection->OnExport().BindRaw(this, &FEditorSettingsViewerModule::HandleInputBindingsExport);
			InputBindingSettingsSection->OnImport().BindRaw(this, &FEditorSettingsViewerModule::HandleInputBindingsImport);
			InputBindingSettingsSection->OnResetDefaults().BindRaw(this, &FEditorSettingsViewerModule::HandleInputBindingsResetToDefault);
			InputBindingSettingsSection->OnSave().BindRaw(this, &FEditorSettingsViewerModule::HandleInputBindingsSave);
		}

		// loading & saving features
		SettingsModule.RegisterSettings("Editor", "General", "LoadingSaving",
			LOCTEXT("LoadingSavingSettingsName", "Loading & Saving"),
			LOCTEXT("LoadingSavingSettingsDescription", "Change how the Editor loads and saves files."),
			GetMutableDefault<UEditorLoadingSavingSettings>()
		);

		// @todo thomass: proper settings support for source control module
		GetMutableDefault<UEditorLoadingSavingSettings>()->SccHackInitialize();

		// misc unsorted settings
		SettingsModule.RegisterSettings("Editor", "General", "UserSettings",
			LOCTEXT("UserSettingsName", "Miscellaneous"),
			LOCTEXT("UserSettingsDescription", "Customize the behavior, look and feel of the editor."),
			GetMutableDefault<UEditorPerProjectUserSettings>()
		);

		// Crash Reporter settings
		SettingsModule.RegisterSettings("Editor", "Advanced", "CrashReporter",
			LOCTEXT("CrashReporterSettingsName", "Crash Reporter"),
			LOCTEXT("CrashReporterSettingsDescription", "Various Crash Reporter related settings."),
			GetMutableDefault<UCrashReporterSettings>()
			);

		// experimental features
		SettingsModule.RegisterSettings("Editor", "General", "Experimental",
			LOCTEXT("ExperimentalettingsName", "Experimental"),
			LOCTEXT("ExperimentalSettingsDescription", "Enable and configure experimental Editor features."),
			GetMutableDefault<UEditorExperimentalSettings>()
		);
	}

	/**
	 * Registers Level Editor settings.
	 *
	 * @param SettingsModule A reference to the settings module.
	 */
	void RegisterLevelEditorSettings( ISettingsModule& SettingsModule )
	{
		// play-in settings
		SettingsModule.RegisterSettings("Editor", "LevelEditor", "PlayIn",
			LOCTEXT("LevelEditorPlaySettingsName", "Play"),
			LOCTEXT("LevelEditorPlaySettingsDescription", "Set up window sizes and other options for the Play In Editor (PIE) feature."),
			GetMutableDefault<ULevelEditorPlaySettings>()
		);

		// view port settings
		SettingsModule.RegisterSettings("Editor", "LevelEditor", "Viewport",
			LOCTEXT("LevelEditorViewportSettingsName", "Viewports"),
			LOCTEXT("LevelEditorViewportSettingsDescription", "Configure the look and feel of the Level Editor view ports."),
			GetMutableDefault<ULevelEditorViewportSettings>()
		);
	}

	/**
	 * Registers Other Tools settings.
	 *
	 * @param SettingsModule A reference to the settings module.
	 */
	void RegisterContentEditorsSettings( ISettingsModule& SettingsModule )
	{
		// content browser
		SettingsModule.RegisterSettings("Editor", "ContentEditors", "ContentBrowser",
			LOCTEXT("ContentEditorsContentBrowserSettingsName", "Content Browser"),
			LOCTEXT("ContentEditorsContentBrowserSettingsDescription", "Change the behavior of the Content Browser."),
			GetMutableDefault<UContentBrowserSettings>()
		);

		// destructable mesh editor
/*		SettingsModule.RegisterSettings("Editor", "ContentEditors", "DestructableMeshEditor",
			LOCTEXT("ContentEditorsDestructableMeshEditorSettingsName", "Destructable Mesh Editor"),
			LOCTEXT("ContentEditorsDestructableMeshEditorSettingsDescription", "Change the behavior of the Destructable Mesh Editor."),
			GetMutableDefault<UDestructableMeshEditorSettings>()
		);*/

		// graph editors
		SettingsModule.RegisterSettings("Editor", "ContentEditors", "GraphEditor",
			LOCTEXT("ContentEditorsGraphEditorSettingsName", "Graph Editors"),
			LOCTEXT("ContentEditorsGraphEditorSettingsDescription", "Customize Anim, Blueprint and Material Editor."),
			GetMutableDefault<UGraphEditorSettings>()
		);

		// graph editors
		SettingsModule.RegisterSettings("Editor", "ContentEditors", "BlueprintEditor",
			LOCTEXT("ContentEditorsBlueprintEditorSettingsName", "Blueprint Editor"),
			LOCTEXT("ContentEditorsGraphBlueprintSettingsDescription", "Customize Blueprint Editors."),
			GetMutableDefault<UBlueprintEditorSettings>()
		);

		// Persona editors
		SettingsModule.RegisterSettings("Editor", "ContentEditors", "Persona",
			LOCTEXT("ContentEditorsPersonaSettingsName", "Animation Editor"),
			LOCTEXT("ContentEditorsPersonaSettingsDescription", "Customize Persona Editor."),
			GetMutableDefault<UPersonaOptions>()
		);
	}

	void RegisterPrivacySettings(ISettingsModule& SettingsModule)
	{
		// Analytics
		SettingsModule.RegisterSettings("Editor", "Privacy", "Analytics",
			LOCTEXT("PrivacyAnalyticsSettingsName", "Usage Data"),
			LOCTEXT("PrivacyAnalyticsSettingsDescription", "Configure the way your Editor usage information is handled."),
			GetMutableDefault<UAnalyticsPrivacySettings>()
			);
	}

	/** Unregisters all settings. */
	void UnregisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterViewer("Editor");

			// general settings
			SettingsModule->UnregisterSettings("Editor", "General", "InputBindings");
			SettingsModule->UnregisterSettings("Editor", "General", "LoadingSaving");
			SettingsModule->UnregisterSettings("Editor", "General", "GameAgnostic");
			SettingsModule->UnregisterSettings("Editor", "General", "UserSettings");
			SettingsModule->UnregisterSettings("Editor", "General", "AutomationTest");
			SettingsModule->UnregisterSettings("Editor", "General", "Internationalization");
			SettingsModule->UnregisterSettings("Editor", "General", "Experimental");
			SettingsModule->UnregisterSettings("Editor", "General", "CrashReporter");			

			// level editor settings
			SettingsModule->UnregisterSettings("Editor", "LevelEditor", "PlayIn");
			SettingsModule->UnregisterSettings("Editor", "LevelEditor", "Viewport");

			// other tools
			SettingsModule->UnregisterSettings("Editor", "ContentEditors", "ContentBrowser");
//			SettingsModule->UnregisterSettings("Editor", "ContentEditors", "DestructableMeshEditor");
			SettingsModule->UnregisterSettings("Editor", "ContentEditors", "GraphEditor");
			SettingsModule->UnregisterSettings("Editor", "ContentEditors", "Persona");
		}
	}

private:

	/** Handles creating the editor settings tab. */
	TSharedRef<SDockTab> HandleSpawnSettingsTab( const FSpawnTabArgs& SpawnTabArgs )
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		TSharedRef<SWidget> SettingsEditor = SNullWidget::NullWidget;

		if (SettingsModule != nullptr)
		{
			ISettingsContainerPtr SettingsContainer = SettingsModule->GetContainer("Editor");

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

	// Show a warning that the editor will require a restart and return its result
	EAppReturnType::Type ShowRestartWarning(const FText& Title) const
	{
		return OpenMsgDlgInt(EAppMsgType::OkCancel, LOCTEXT("ActionRestartMsg", "Imported settings won't be applied until the editor is restarted. Do you wish to restart now (you will be prompted to save any changes)?" ), Title);
	}

	// Backup a file
	bool BackupFile(const FString& SrcFilename, const FString& DstFilename)
	{
		if (IFileManager::Get().Copy(*DstFilename, *SrcFilename) == COPY_OK)
		{
			return true;
		}

		// log error	
		FMessageLog EditorErrors("EditorErrors");
		if(!FPaths::FileExists(SrcFilename))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("FileName"), FText::FromString(SrcFilename));
			EditorErrors.Warning(FText::Format(LOCTEXT("UnsuccessfulBackup_NoExist_Notification", "Unsuccessful backup! {FileName} does not exist!"), Arguments));
		}
		else if(IFileManager::Get().IsReadOnly(*DstFilename))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("FileName"), FText::FromString(DstFilename));
			EditorErrors.Warning(FText::Format(LOCTEXT("UnsuccessfulBackup_ReadOnly_Notification", "Unsuccessful backup! {FileName} is read-only!"), Arguments));
		}
		else
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("SourceFileName"), FText::FromString(SrcFilename));
			Arguments.Add(TEXT("BackupFileName"), FText::FromString(DstFilename));
			// We don't specifically know why it failed, this is a fallback.
			EditorErrors.Warning(FText::Format(LOCTEXT("UnsuccessfulBackup_Fallback_Notification", "Unsuccessful backup of {SourceFileName} to {BackupFileName}"), Arguments));
		}
		EditorErrors.Notify(LOCTEXT("BackupUnsuccessful_Title", "Backup Unsuccessful!"));	

		return false;
	}

	// Handles exporting input bindings to a file
	bool HandleInputBindingsExport( const FString& Filename )
	{
		FInputBindingManager::Get().SaveInputBindings();
		GConfig->Flush(false, GEditorKeyBindingsIni);
		return BackupFile(GEditorKeyBindingsIni, Filename);
	}

	// Handles importing input bindings from a file
	bool HandleInputBindingsImport( const FString& Filename )
	{
		if( EAppReturnType::Ok == ShowRestartWarning(LOCTEXT("ImportKeyBindings_Title", "Import Key Bindings")))
		{
			FUnrealEdMisc::Get().SetConfigRestoreFilename(Filename, GEditorKeyBindingsIni);
			FUnrealEdMisc::Get().RestartEditor(false);

			return true;
		}

		return false;
	}

	// Handles resetting input bindings back to the defaults
	bool HandleInputBindingsResetToDefault()
	{
		if( EAppReturnType::Ok == ShowRestartWarning(LOCTEXT("ResetKeyBindings_Title", "Reset Key Bindings")))
		{
			FInputBindingManager::Get().RemoveUserDefinedChords();
			GConfig->Flush(false, GEditorKeyBindingsIni);
			FUnrealEdMisc::Get().RestartEditor(false);

			return true;
		}

		return false;
	}

	// Handles saving default input bindings.
	// This only gets called by SSettingsEditor::HandleImportButtonClicked when importing new settings,
	// and its implementation here is just to flush custom input bindings so that editor shutdown doesn't
	// overwrite the imported settings just copied across.
	bool HandleInputBindingsSave()
	{
		FInputBindingManager::Get().RemoveUserDefinedChords();
		GConfig->Flush(false, GEditorKeyBindingsIni);
		return true;
	}

private:

	/** Holds a pointer to the settings editor's view model. */
	TWeakPtr<ISettingsEditorModel> SettingsEditorModelPtr;
};


IMPLEMENT_MODULE(FEditorSettingsViewerModule, EditorSettingsViewer);


#undef LOCTEXT_NAMESPACE
