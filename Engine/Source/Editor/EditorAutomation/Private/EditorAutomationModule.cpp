// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EditorAutomationPrivatePCH.h"
#include "EditorAutomationModule.h"
#include "LevelEditor.h"
#include "ISessionFrontendModule.h"
#include "IPlacementModeModule.h"
#include "FunctionalTest.h"
#include "AssetRegistryModule.h"

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
			"AutomationTools",
			EExtensionHook::First,
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
		//MenuBuilder.BeginSection("Automation", LOCTEXT("Fortnite", "Fortnite"));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AutomationLabel", "Automation Frontend"),
			LOCTEXT("Tooltip", "Launch the Automation Frontend."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "AutomationTools.MenuIcon"),
			FUIAction(FExecuteAction::CreateStatic(&FEditorAutomationModule::OnShowAutomationFrontend)));
		//MenuBuilder.EndSection();
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
		}
	}

private:
	TSharedPtr<FExtender> Extender;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEditorAutomationModule, EditorAutomation);