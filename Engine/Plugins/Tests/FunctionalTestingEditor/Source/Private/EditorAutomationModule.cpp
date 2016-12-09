// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EditorAutomationModule.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "GameFramework/Actor.h"
#include "AssetData.h"
#include "EdGraph/EdGraphSchema.h"
#include "LevelEditor.h"
#include "ISessionFrontendModule.h"
#include "IPlacementModeModule.h"
#include "FunctionalTest.h"
#include "ScreenshotFunctionalTest.h"
#include "AssetRegistryModule.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "EditorAutomation"

void OpenMapAndFocusActor(const TArray<FString>& Args)
{
	if ( Args.Num() < 2 )
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("Automate.OpenMapAndFocusActor failed\n"));
		return;
	}

	FString AssetPath = Args[0];
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> AllAssets;
	AssetRegistryModule.Get().GetAssetsByPackageName(*AssetPath, AllAssets);

	if ( AllAssets.Num() > 0 )
	{
		bool bIsWorlAlreadyOpened = false;
		if ( UWorld* EditorWorld = GEditor->GetEditorWorldContext().World() )
		{
			if ( FAssetData(EditorWorld).PackageName == AllAssets[0].PackageName )
			{
				bIsWorlAlreadyOpened = true;
			}
		}

		if ( !bIsWorlAlreadyOpened )
		{
			UObject* ObjectToEdit = AllAssets[0].GetAsset();
			if ( ObjectToEdit )
			{
				GEditor->EditObject(ObjectToEdit);
			}
		}
	}

	if ( UWorld* EditorWorld = GEditor->GetEditorWorldContext().World() )
	{
		AActor* ActorToFocus = nullptr;
		for ( int32 LevelIndex=0; LevelIndex < EditorWorld->GetNumLevels(); LevelIndex++ )
		{
			ULevel* Level = EditorWorld->GetLevel(LevelIndex);
			ActorToFocus = static_cast<AActor*>( FindObjectWithOuter(Level, AActor::StaticClass(), *Args[1]) );
			if ( ActorToFocus )
			{
				break;
			}
		}

		if ( ActorToFocus )
		{
			GEditor->SelectNone(/*bNoteSelectionChange=*/ false, false, false);
			GEditor->SelectActor(ActorToFocus, true, true);
			GEditor->NoteSelectionChange();

			const bool bActiveViewportOnly = false;
			GEditor->MoveViewportCamerasToActor(*ActorToFocus, bActiveViewportOnly);
		}
	}
}

FAutoConsoleCommand OpenMapAndFocusActorCmd(
	TEXT("Automate.OpenMapAndFocusActor"),
	TEXT("Opens a map and focuses a particular actor by name."),
	FConsoleCommandWithArgsDelegate::CreateStatic(OpenMapAndFocusActor)
);

class FEditorAutomationModule : public IEditorAutomationModule
{
	void StartupModule()
	{
		// make an extension to add the Orion function menu
		Extender = MakeShareable(new FExtender());
		Extender->AddMenuExtension(
			"General",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateRaw(this, &FEditorAutomationModule::OnAutomationToolsMenuCreation));

		// add the menu extension to the editor
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(Extender);

		FModuleManager::Get().OnModulesChanged().AddRaw(this, &FEditorAutomationModule::OnModulesChanged);

		if ( IPlacementModeModule::IsAvailable() )
		{
			OnModulesChanged("PlacementMode", EModuleChangeReason::ModuleLoaded);
		}
	}

	void ShutdownModule()
	{
		if ( FModuleManager::Get().IsModuleLoaded("LevelEditor") )
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
			LevelEditorModule.GetMenuExtensibilityManager()->RemoveExtender(Extender);
		}

		FModuleManager::Get().OnModulesChanged().RemoveAll(this);

		if ( IPlacementModeModule::IsAvailable() )
		{
			IPlacementModeModule::Get().UnregisterPlacementCategory("Testing");
		}
	}

	void OnAutomationToolsMenuCreation(FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.BeginSection("Testing", LOCTEXT("Testing", "Testing"));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AutomationLabel", "Test Automation"),
			LOCTEXT("Tooltip", "Launch the Testing Automation Frontend."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "AutomationTools.MenuIcon"),
			FUIAction(FExecuteAction::CreateStatic(&FEditorAutomationModule::OnShowAutomationFrontend)));
		MenuBuilder.EndSection();

		// TODO Come up with a way to hide this if no tools are registered.
		if (WorkspaceMenu::GetMenuStructure().GetAutomationToolsCategory()->GetChildItems().Num() > 0)
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("AutomationTools", "Automation Tools"),
				LOCTEXT("AutomationToolsToolTip", "Assorted tools to help generate data for some of the automation tests."),
				FNewMenuDelegate::CreateRaw(this, &FEditorAutomationModule::PopulateAutomationTools)
			);
		}
	}

	void PopulateAutomationTools(FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.BeginSection("AutomationTools", LOCTEXT("AutomationTools", "Automation Tools"));
		const bool bAutoAndOrphanedMenus = false;
		FGlobalTabmanager::Get()->PopulateTabSpawnerMenu(MenuBuilder, WorkspaceMenu::GetMenuStructure().GetAutomationToolsCategory(), bAutoAndOrphanedMenus);
		MenuBuilder.EndSection();
	}

	static void OnShowAutomationFrontend()
	{
		ISessionFrontendModule& SessionFrontend = FModuleManager::LoadModuleChecked<ISessionFrontendModule>("SessionFrontend");
		SessionFrontend.InvokeSessionFrontend(FName("AutomationPanel"));
	}

	void OnModulesChanged(FName Module, EModuleChangeReason Reason)
	{
		if ( Module == TEXT("PlacementMode") && Reason == EModuleChangeReason::ModuleLoaded )
		{
			FPlacementCategoryInfo Info(
				LOCTEXT("FunctionalTestingCategoryName", "Testing"),
				"Testing",
				TEXT("PMTesting"),
				25
				);

			IPlacementModeModule::Get().RegisterPlacementCategory(Info);
			IPlacementModeModule::Get().RegisterPlaceableItem(Info.UniqueHandle, MakeShareable(new FPlaceableItem(nullptr, FAssetData(AFunctionalTest::StaticClass()))));
			IPlacementModeModule::Get().RegisterPlaceableItem(Info.UniqueHandle, MakeShareable(new FPlaceableItem(nullptr, FAssetData(AScreenshotFunctionalTest::StaticClass()))));
		}
	}

private:
	TSharedPtr<FExtender> Extender;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEditorAutomationModule, EditorAutomation);
