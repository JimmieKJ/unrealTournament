// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "BlueprintEditorPrivatePCH.h"
#include "BlueprintEditorModule.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetEditorUtilities.h"
#include "BlueprintEditor.h"
#include "BlueprintUtilities.h"
#include "Toolkits/ToolkitManager.h"
#include "Editor/UnrealEd/Public/Kismet2/DebuggerCommands.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "UserDefinedEnumEditor.h"
#include "Developer/MessageLog/Public/MessageLogModule.h"
#include "UObjectToken.h"
#include "InstancedStaticMeshSCSEditorCustomization.h"
#include "UserDefinedStructureEditor.h"
#include "BlueprintGraphPanelPinFactory.h"
#include "SDockTab.h"

#define LOCTEXT_NAMESPACE "BlueprintEditor"

IMPLEMENT_MODULE( FBlueprintEditorModule, Kismet );

const FName BlueprintEditorAppName = FName(TEXT("BlueprintEditorApp"));
const FName DebuggerAppName = FName(TEXT("DebuggerApp"));

//////////////////////////////////////////////////////////////////////////
// SDebuggerApp

#include "Debugging/SKismetDebuggingView.h"

static bool CanCloseBlueprintDebugger()
{
	return !GIntraFrameDebuggingGameThread;
}

TSharedRef<SDockTab> CreateBluprintDebuggerTab( const FSpawnTabArgs& Args )
{
	return SNew(SDockTab)
		.TabRole( ETabRole::NomadTab )
		.OnCanCloseTab( SDockTab::FCanCloseTab::CreateStatic(&CanCloseBlueprintDebugger) )
		.Label( NSLOCTEXT("BlueprintDebugger", "TabTitle", "Blueprint Debugger") )
		[
			SNew(SKismetDebuggingView)
		];
}

//////////////////////////////////////////////////////////////////////////
// FBlueprintEditorModule

TSharedRef<FExtender> ExtendLevelViewportContextMenuForBlueprints(const TSharedRef<FUICommandList> CommandList, TArray<AActor*> SelectedActors);

FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors LevelViewportContextMenuBlueprintExtender;

static void FocusBlueprintEditorOnObject(const TSharedRef<IMessageToken>& Token)
{
	if( Token->GetType() == EMessageToken::Object )
	{
		const TSharedRef<FUObjectToken> UObjectToken = StaticCastSharedRef<FUObjectToken>(Token);
		if(UObjectToken->GetObject().IsValid())
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(UObjectToken->GetObject().Get());
		}
	}
}

void FBlueprintEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	SharedBlueprintEditorCommands = MakeShareable(new FUICommandList);

	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner( DebuggerAppName, FOnSpawnTab::CreateStatic(&CreateBluprintDebuggerTab) )
		.SetDisplayName( NSLOCTEXT("BlueprintDebugger", "TabTitle", "Blueprint Debugger") )
		.SetTooltipText( NSLOCTEXT("BlueprintDebugger", "TooltipText", "Open the Blueprint Debugger tab.") )
		.SetGroup( MenuStructure.GetDeveloperToolsDebugCategory() )
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "BlueprintDebugger.TabIcon"));

	// Have to check GIsEditor because right now editor modules can be loaded by the game
	// Once LoadModule is guaranteed to return NULL for editor modules in game, this can be removed
	// Without this check, loading the level editor in the game will crash
	if (GIsEditor)
	{
		// Extend the level viewport context menu to handle blueprints
		LevelViewportContextMenuBlueprintExtender = FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateStatic(&ExtendLevelViewportContextMenuForBlueprints);
		FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		auto& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();
		MenuExtenders.Add(LevelViewportContextMenuBlueprintExtender);
		LevelViewportContextMenuBlueprintExtenderDelegateHandle = MenuExtenders.Last().GetHandle();
	}

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions InitOptions;
	InitOptions.bShowFilters = true;
	InitOptions.bShowPages = true;
	MessageLogModule.RegisterLogListing("BlueprintLog", LOCTEXT("BlueprintLog", "Blueprint Log"), InitOptions);

	// Listen for clicks in log so we can focus on the object, might have to restart K2 if the K2 tab has been closed
	MessageLogModule.GetLogListing("BlueprintLog")->OnMessageTokenClicked().AddStatic( &FocusBlueprintEditorOnObject );
	
	// Also listen for clicks in the PIE log, runtime errors with Blueprints may post clickable links there
	MessageLogModule.GetLogListing("PIE")->OnMessageTokenClicked().AddStatic( &FocusBlueprintEditorOnObject );

	// Add a page for pre-loading of the editor
	MessageLogModule.GetLogListing("BlueprintLog")->NewPage(LOCTEXT("PreloadLogPageLabel", "Editor Load"));

	// Register internal SCS editor customizations
	RegisterSCSEditorCustomization("InstancedStaticMeshComponent", FSCSEditorCustomizationBuilder::CreateStatic(&FInstancedStaticMeshSCSEditorCustomization::MakeInstance));

	TSharedPtr<FBlueprintGraphPanelPinFactory> BlueprintGraphPanelPinFactory = MakeShareable(new FBlueprintGraphPanelPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(BlueprintGraphPanelPinFactory);

	PrepareAutoGeneratedDefaultEvents();
}

void FBlueprintEditorModule::ShutdownModule()
{
	// Cleanup all information for auto generated default event nodes by this module
	FKismetEditorUtilities::UnregisterAutoBlueprintNodeCreation(this);

	SharedBlueprintEditorCommands.Reset();
	MenuExtensibilityManager.Reset();

	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner( DebuggerAppName );
	}
	
	// Remove level viewport context menu extenders
	if ( FModuleManager::Get().IsModuleLoaded( "LevelEditor" ) )
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>( "LevelEditor" );
		LevelEditorModule.GetAllLevelViewportContextMenuExtenders().RemoveAll([&](const FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors& Delegate) {
			return Delegate.GetHandle() == LevelViewportContextMenuBlueprintExtenderDelegateHandle;
		});
	}

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.UnregisterLogListing("BlueprintLog");

	// Unregister internal SCS editor customizations
	UnregisterSCSEditorCustomization("InstancedStaticMeshComponent");
}


TSharedRef<IBlueprintEditor> FBlueprintEditorModule::CreateBlueprintEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UBlueprint* Blueprint, bool bShouldOpenInDefaultsMode)
{
	TSharedRef< FBlueprintEditor > NewBlueprintEditor( new FBlueprintEditor() );
	
	TArray<UBlueprint*> Blueprints;
	Blueprints.Add(Blueprint);
	NewBlueprintEditor->InitBlueprintEditor(Mode, InitToolkitHost, Blueprints, bShouldOpenInDefaultsMode);

	for(auto It(SCSEditorCustomizations.CreateConstIterator()); It; ++It)
	{
		NewBlueprintEditor->RegisterSCSEditorCustomization(It->Key, It->Value.Execute(NewBlueprintEditor));
	}
	
	EBlueprintType const BPType = Blueprint ? (EBlueprintType)Blueprint->BlueprintType : BPTYPE_Normal;
	BlueprintEditorOpened.Broadcast(BPType);

	return NewBlueprintEditor;
}

TSharedRef<IBlueprintEditor> FBlueprintEditorModule::CreateBlueprintEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray< UBlueprint* >& BlueprintsToEdit )
{
	TSharedRef< FBlueprintEditor > NewBlueprintEditor( new FBlueprintEditor() );

	NewBlueprintEditor->InitBlueprintEditor(Mode, InitToolkitHost, BlueprintsToEdit, true);

	for(auto It(SCSEditorCustomizations.CreateConstIterator()); It; ++It)
	{
		NewBlueprintEditor->RegisterSCSEditorCustomization(It->Key, It->Value.Execute(NewBlueprintEditor));
	}

	EBlueprintType const BPType = ( (BlueprintsToEdit.Num() > 0) && (BlueprintsToEdit[0] != NULL) ) 
		? (EBlueprintType) BlueprintsToEdit[0]->BlueprintType
		: BPTYPE_Normal;

	BlueprintEditorOpened.Broadcast(BPType);

	return NewBlueprintEditor;
}

TSharedRef<IUserDefinedEnumEditor> FBlueprintEditorModule::CreateUserDefinedEnumEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UUserDefinedEnum* UDEnum)
{
	TSharedRef<FUserDefinedEnumEditor> UserDefinedEnumEditor(new FUserDefinedEnumEditor());
	UserDefinedEnumEditor->InitEditor(Mode, InitToolkitHost, UDEnum);
	return UserDefinedEnumEditor;
}

TSharedRef<IUserDefinedStructureEditor> FBlueprintEditorModule::CreateUserDefinedStructEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UUserDefinedStruct* UDStruct)
{
	TSharedRef<FUserDefinedStructureEditor> UserDefinedStructureEditor(new FUserDefinedStructureEditor());
	UserDefinedStructureEditor->InitEditor(Mode, InitToolkitHost, UDStruct);
	return UserDefinedStructureEditor;
}

void FBlueprintEditorModule::RegisterSCSEditorCustomization(const FName& InComponentName, FSCSEditorCustomizationBuilder InCustomizationBuilder)
{
	SCSEditorCustomizations.Add(InComponentName, InCustomizationBuilder);
}

void FBlueprintEditorModule::UnregisterSCSEditorCustomization(const FName& InComponentName)
{
	SCSEditorCustomizations.Remove(InComponentName);
}

void FBlueprintEditorModule::PrepareAutoGeneratedDefaultEvents()
{
	// Load up all default events that should be spawned for Blueprints that are children of specific classes
	const FString ConfigSection = TEXT("DefaultEventNodes");
	const FString SettingName = TEXT("Node");
	TArray< FString > NodeSpawns;
	GConfig->GetArray(*ConfigSection, *SettingName, NodeSpawns, GEditorPerProjectIni);

	for(FString CurrentNodeSpawn : NodeSpawns)
	{
		FString TargetClassName;
		if(!FParse::Value(*CurrentNodeSpawn, TEXT("TargetClass="), TargetClassName))
		{
			// Could not find a class name, cannot continue with this line
			continue;
		}

		UClass* FoundTargetClass = FindObject<UClass>(ANY_PACKAGE, *TargetClassName, true);
		if(FoundTargetClass)
		{
			FString TargetEventFunction;
			if(!FParse::Value(*CurrentNodeSpawn, TEXT("TargetEvent="), TargetEventFunction))
			{
				// Could not find a class name, cannot continue with this line
				continue;
			}

			if (FindObject<UFunction>(FoundTargetClass, *TargetEventFunction))
			{
				FKismetEditorUtilities::RegisterAutoGeneratedDefaultEvent(this, FoundTargetClass, FName(*TargetEventFunction));
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
