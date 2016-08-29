// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/Kismet/Public/BlueprintEditorTabs.h"
#include "Editor/Kismet/Public/SBlueprintEditorToolbar.h"
#include "Editor/Kismet/Public/BlueprintEditorModes.h"
#include "Editor/Kismet/Public/BlueprintEditorSharedTabFactories.h"
#include "SKismetInspector.h"

#include "WidgetBlueprintEditor.h"
#include "WidgetDesignerApplicationMode.h"
#include "WidgetBlueprintEditorToolbar.h"

#include "IDetailsView.h"

#include "BlueprintEditorTabs.h"
#include "PaletteTabSummoner.h"
#include "HierarchyTabSummoner.h"
#include "DesignerTabSummoner.h"
#include "SequencerTabSummoner.h"
#include "DetailsTabSummoner.h"
#include "AnimationTabSummoner.h"
#include "BlueprintModes/WidgetBlueprintApplicationModes.h"

#define LOCTEXT_NAMESPACE "WidgetDesignerMode"

/////////////////////////////////////////////////////
// FWidgetDesignerApplicationMode

FWidgetDesignerApplicationMode::FWidgetDesignerApplicationMode(TSharedPtr<FWidgetBlueprintEditor> InWidgetEditor)
	: FWidgetBlueprintApplicationMode(InWidgetEditor, FWidgetBlueprintApplicationModes::DesignerMode)
{
	// Override the default created category here since "Designer Editor" sounds awkward
	WorkspaceMenuCategory = FWorkspaceItem::NewGroup(LOCTEXT("WorkspaceMenu_WidgetDesigner", "Widget Designer"));

	TabLayout = FTabManager::NewLayout( "WidgetBlueprintEditor_Designer_Layout_v4_1" )
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient( 0.2f )
			->SetHideTabWell(true)
			->AddTab( InWidgetEditor->GetToolbarTabId(), ETabState::OpenedTab )
		)
		->Split
		(
			FTabManager::NewSplitter()
			->SetOrientation(Orient_Horizontal)
			->SetSizeCoefficient( 0.70f )
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient( 0.15f )
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient( 0.5f )
					->AddTab( FPaletteTabSummoner::TabID, ETabState::OpenedTab )
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient( 0.5f )
					->AddTab( FHierarchyTabSummoner::TabID, ETabState::OpenedTab )
				)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient( 0.85f )
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetHideTabWell(true)
					->AddTab( FDesignerTabSummoner::TabID, ETabState::OpenedTab )
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient( 0.35f )
					->AddTab( FDetailsTabSummoner::TabID, ETabState::OpenedTab )
				)
			)
		)
		->Split
		(
			FTabManager::NewSplitter()
			->SetOrientation(Orient_Horizontal)
			->SetSizeCoefficient( 0.30f )
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(.15f)
				->AddTab(FAnimationTabSummoner::TabID, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(.85f)
				->SetForegroundTab(FSequencerTabSummoner::TabID)
				->AddTab(FSequencerTabSummoner::TabID, ETabState::OpenedTab)
				->AddTab(FBlueprintEditorTabs::CompilerResultsID, ETabState::OpenedTab)
			)
		)
	);

	// Add Tab Spawners
	//TabFactories.RegisterFactory(MakeShareable(new FSelectionDetailsSummoner(InWidgetEditor)));
	TabFactories.RegisterFactory(MakeShareable(new FDetailsTabSummoner(InWidgetEditor)));
	TabFactories.RegisterFactory(MakeShareable(new FDesignerTabSummoner(InWidgetEditor)));
	TabFactories.RegisterFactory(MakeShareable(new FHierarchyTabSummoner(InWidgetEditor)));
	TabFactories.RegisterFactory(MakeShareable(new FPaletteTabSummoner(InWidgetEditor)));
	TabFactories.RegisterFactory(MakeShareable(new FSequencerTabSummoner(InWidgetEditor)));
	TabFactories.RegisterFactory(MakeShareable(new FAnimationTabSummoner(InWidgetEditor)));
	TabFactories.RegisterFactory(MakeShareable(new FCompilerResultsSummoner(InWidgetEditor)));

	// setup toolbar - clear existing toolbar extender from the BP mode
	//@TODO: Keep this in sync with BlueprintEditorModes.cpp
	ToolbarExtender = MakeShareable(new FExtender);
	InWidgetEditor->GetWidgetToolbarBuilder()->AddWidgetBlueprintEditorModesToolbar(ToolbarExtender);
	InWidgetEditor->GetToolbarBuilder()->AddCompileToolbar(ToolbarExtender);
	//InWidgetEditor->GetToolbarBuilder()->AddComponentsToolbar(ToolbarExtender);
	InWidgetEditor->GetToolbarBuilder()->AddDebuggingToolbar(ToolbarExtender);

	//InWidgetEditor->GetToolbarBuilder()->AddScriptingToolbar(ToolbarExtender);
	//InWidgetEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(ToolbarExtender);
}

void FWidgetDesignerApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FWidgetBlueprintEditor> BP = GetBlueprintEditor();

	BP->RegisterToolbarTab(InTabManager.ToSharedRef());
	BP->PushTabFactories(TabFactories);
}

void FWidgetDesignerApplicationMode::PreDeactivateMode()
{
	//FWidgetBlueprintApplicationMode::PreDeactivateMode();
}

void FWidgetDesignerApplicationMode::PostActivateMode()
{
	//FWidgetBlueprintApplicationMode::PostActivateMode();
	TSharedPtr<FWidgetBlueprintEditor> BP = GetBlueprintEditor();

	BP->OnEnteringDesigner();
}

#undef LOCTEXT_NAMESPACE