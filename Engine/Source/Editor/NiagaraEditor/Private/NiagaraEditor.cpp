// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "NiagaraScript.h"

#include "Toolkits/IToolkitHost.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "SDockTab.h"
#include "GenericCommands.h"

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"
 
#define LOCTEXT_NAMESPACE "NiagaraEditor"

const FName FNiagaraEditor::NodeGraphTabId(TEXT("NiagaraEditor_NodeGraph"));
const FName FNiagaraEditor::PropertiesTabId(TEXT("NiagaraEditor_MaterialProperties"));

void FNiagaraEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	WorkspaceMenuCategory = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_NiagaraEditor", "Niagara"));

	FAssetEditorToolkit::RegisterTabSpawners(TabManager);
	
	TSharedRef<FWorkspaceItem> WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	TabManager->RegisterTabSpawner( NodeGraphTabId, FOnSpawnTab::CreateSP(this, &FNiagaraEditor::SpawnTab_NodeGraph) )
		.SetDisplayName( LOCTEXT("NodeGraph", "Node Graph") )
		.SetGroup(WorkspaceMenuCategoryRef);

	TabManager->RegisterTabSpawner(PropertiesTabId, FOnSpawnTab::CreateSP(this, &FNiagaraEditor::SpawnTab_NodeProperties))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FNiagaraEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(TabManager);

	TabManager->UnregisterTabSpawner(NodeGraphTabId);
	TabManager->UnregisterTabSpawner(PropertiesTabId);
}

FNiagaraEditor::~FNiagaraEditor()
{
	NiagaraDetailsView.Reset();
}


void FNiagaraEditor::InitNiagaraEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraScript* InScript )
{	
	Script = InScript;
	check(Script != NULL);
	Source = CastChecked<UNiagaraScriptSource>(Script->Source);
	check(Source->NodeGraph != NULL);

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_Niagara_Layout_v4" )
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->AddTab(GetToolbarTabId(), ETabState::OpenedTab) 
			->SetHideTabWell( true )
		)
		->Split
		(
			FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.9f)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.2f)
				->AddTab(PropertiesTabId, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.8f)
				->AddTab(NodeGraphTabId, ETabState::OpenedTab)
			)
		)
	);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	const FDetailsViewArgs DetailsViewArgs(false, false, true, FDetailsViewArgs::HideNameArea, true, this);
	NiagaraDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, FNiagaraEditorModule::NiagaraEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Script );

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>( "NiagaraEditor" );
	AddMenuExtender(NiagaraEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// @todo toolkit world centric editing
	/*// Setup our tool's layout
	if( IsWorldCentricAssetEditor() )
	{
		const FString TabInitializationPayload(TEXT(""));		// NOTE: Payload not currently used for table properties
		SpawnToolkitTab( NodeGraphTabId, TabInitializationPayload, EToolkitTabSpot::Details );
	}*/
}

FName FNiagaraEditor::GetToolkitFName() const
{
	return FName("Niagara");
}

FText FNiagaraEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Niagara");
}

FString FNiagaraEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Niagara ").ToString();
}


FLinearColor FNiagaraEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}


/** Create new tab for the supplied graph - don't call this directly, call SExplorer->FindTabForGraph.*/
TSharedRef<SGraphEditor> FNiagaraEditor::CreateGraphEditorWidget(UEdGraph* InGraph)
{
	check(InGraph != NULL);
	
	if ( !GraphEditorCommands.IsValid() )
	{
		GraphEditorCommands = MakeShareable( new FUICommandList );
	
		// Editing commands
		GraphEditorCommands->MapAction( FGenericCommands::Get().Delete,
			FExecuteAction::CreateSP( this, &FNiagaraEditor::DeleteSelectedNodes ),
			FCanExecuteAction::CreateSP( this, &FNiagaraEditor::CanDeleteNodes )
			);
	}

	// Create the appearance info
	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText", "NIAGARA");

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FNiagaraEditor::OnSelectedNodesChanged);

	// Make title bar
	TSharedRef<SWidget> TitleBarWidget = 
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush( TEXT("Graph.TitleBackground") ) )
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.FillWidth(1.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NodeGraphLabel", "Node Graph"))
				.TextStyle( FEditorStyle::Get(), TEXT("GraphBreadcrumbButtonText") )
			]
		];

	// Make full graph editor
	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.Appearance(AppearanceInfo)
		.TitleBar(TitleBarWidget)
		.GraphToEdit(InGraph)
		.GraphEvents(InEvents);
}


TSharedRef<SDockTab> FNiagaraEditor::SpawnTab_NodeGraph( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId().TabType == NodeGraphTabId );

	TSharedRef<SGraphEditor> NodeGraphEditor = CreateGraphEditorWidget(Source->NodeGraph);

	NodeGraphEditorPtr = NodeGraphEditor; // Keep pointer to editor

	return SNew(SDockTab)
		.Label( LOCTEXT("NodeGraph", "Node Graph") )
		.TabColorScale( GetTabColorScale() )
		[
			NodeGraphEditor
		];
}

TSharedRef<SDockTab> FNiagaraEditor::SpawnTab_NodeProperties(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("LevelEditor.Tabs.Details"))
		.Label(LOCTEXT("NiagaraDetailsTitle", "Details"))
		[
			NiagaraDetailsView.ToSharedRef()
		];

	TSharedPtr<SGraphEditor> NodeGraphEditor = NodeGraphEditorPtr.Pin();
	if (NodeGraphEditor.IsValid())
	{
		// Since we're initialising, make sure nothing is selected
		NodeGraphEditor->ClearSelectionSet();
	}

	return SpawnedTab;
}

void FNiagaraEditor::ExtendToolbar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, TSharedRef<SWidget> CompileBox)
		{
			ToolbarBuilder.BeginSection("Compile");
			{
				ToolbarBuilder.AddWidget(CompileBox);
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	TSharedRef<SWidget> Compilebox = SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4)
		[
			SNew(SButton)
			.OnClicked(this, &FNiagaraEditor::OnCompileClicked)
			.Content()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("LevelEditor.Recompile"))
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NiagaraToolbar_Compile", "Compile"))
				]
			]
		];

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic( &Local::FillToolbar, Compilebox )
		);

	AddToolbarExtender(ToolbarExtender);

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>( "NiagaraEditor" );
	AddToolbarExtender(NiagaraEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

FReply FNiagaraEditor::OnCompileClicked()
{
	Script->Source->Compile();
	return FReply::Handled();
}

void FNiagaraEditor::DeleteSelectedNodes()
{
	TSharedPtr<SGraphEditor> NodeGraphEditor = NodeGraphEditorPtr.Pin();
	if (!NodeGraphEditor.IsValid())
	{
		return;
	}

	const FGraphPanelSelectionSet SelectedNodes = NodeGraphEditor->GetSelectedNodes();
	NodeGraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt( SelectedNodes ); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
		{
			if (Node->CanUserDeleteNode())
			{
				Node->DestroyNode();
			}
		}
	}
}

bool FNiagaraEditor::CanDeleteNodes() const
{
	TSharedPtr<SGraphEditor> NodeGraphEditor = NodeGraphEditorPtr.Pin();
	return (NodeGraphEditor.IsValid() && NodeGraphEditor->GetSelectedNodes().Num() > 0);
}

void FNiagaraEditor::OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection)
{
	TArray<UObject*> SelectedObjects;

	UObject* EditObject = Script;

	if (NewSelection.Num() == 0)
	{
		SelectedObjects.Add(EditObject);
	}
	else
	{
		for (TSet<class UObject*>::TConstIterator SetIt(NewSelection); SetIt; ++SetIt)
		{
			SelectedObjects.Add(*SetIt);
		}
	}

	GetDetailView()->SetObjects(SelectedObjects, true);
}

#undef LOCTEXT_NAMESPACE
