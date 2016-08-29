// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQueryEditorPrivatePCH.h"
#include "GraphEditorActions.h"
#include "ScopedTransaction.h"
#include "EdGraphUtilities.h"

#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"

#include "Toolkits/IToolkitHost.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"
#include "SDockTab.h"
#include "GenericCommands.h"
 
#define LOCTEXT_NAMESPACE "EnvironmentQueryEditor"

const FName FEnvironmentQueryEditor::EQSUpdateGraphTabId( TEXT( "EnvironmentQueryEditor_UpdateGraph" ) );
const FName FEnvironmentQueryEditor::EQSPropertiesTabId( TEXT( "EnvironmentQueryEditor_Properties" ) );

void FEnvironmentQueryEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_EnvironmentQueryEditor", "Environment Query Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner( EQSUpdateGraphTabId, FOnSpawnTab::CreateSP(this, &FEnvironmentQueryEditor::SpawnTab_UpdateGraph) )
		.SetDisplayName( NSLOCTEXT("EnvironmentQueryEditor", "Graph", "Graph") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "GraphEditor.EventGraph_16x"));

	InTabManager->RegisterTabSpawner( EQSPropertiesTabId, FOnSpawnTab::CreateSP(this, &FEnvironmentQueryEditor::SpawnTab_Properties) )
		.SetDisplayName( NSLOCTEXT("EnvironmentQueryEditor", "PropertiesTab", "Details" ) )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FEnvironmentQueryEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner( EQSPropertiesTabId );
	InTabManager->UnregisterTabSpawner( EQSUpdateGraphTabId );
}

void FEnvironmentQueryEditor::InitEnvironmentQueryEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UEnvQuery* InScript )
{
	SelectedNodesCount = 0;
	Query = InScript;
	check(Query != NULL);

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_EnvironmentQuery_Layout" )
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
			FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.7f)
				->AddTab( EQSUpdateGraphTabId, ETabState::OpenedTab )
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.3f)
				->AddTab( EQSPropertiesTabId, ETabState::OpenedTab )
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, FEnvironmentQueryEditorModule::EnvironmentQueryEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Query );
	
	FEnvironmentQueryEditorModule& EnvironmentQueryEditorModule = FModuleManager::LoadModuleChecked<FEnvironmentQueryEditorModule>( "EnvironmentQueryEditor" );
	AddMenuExtender(EnvironmentQueryEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	RegenerateMenusAndToolbars();

	// Update BT asset data based on saved graph to have correct data in editor
	TSharedPtr<SGraphEditor> UpdateGraphEditor = UpdateGraphEdPtr.Pin();
	if (UpdateGraphEditor.IsValid() && UpdateGraphEditor->GetCurrentGraph() != NULL)
	{
		//let's find root node
		UEnvironmentQueryGraph* EQSGraph = Cast<UEnvironmentQueryGraph>(UpdateGraphEditor->GetCurrentGraph());
		EQSGraph->UpdateAsset();
	}
}

FName FEnvironmentQueryEditor::GetToolkitFName() const
{
	return FName("Environment Query");
}

FText FEnvironmentQueryEditor::GetBaseToolkitName() const
{
	return NSLOCTEXT("EnvironmentQueryEditor", "AppLabel", "EnvironmentQuery");
}

FString FEnvironmentQueryEditor::GetWorldCentricTabPrefix() const
{
	return NSLOCTEXT("EnvironmentQueryEditor", "WorldCentricTabPrefix", "EnvironmentQuery ").ToString();
}


FLinearColor FEnvironmentQueryEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}


/** Create new tab for the supplied graph - don't call this directly, call SExplorer->FindTabForGraph.*/
TSharedRef<SGraphEditor> FEnvironmentQueryEditor::CreateGraphEditorWidget(UEdGraph* InGraph)
{
	check(InGraph != NULL);
	
	// Create the appearance info
	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = NSLOCTEXT("EnvironmentQueryEditor", "AppearanceCornerText", "ENVIRONMENT QUERY");

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FEnvironmentQueryEditor::OnSelectedNodesChanged);
	
	CreateCommandList();

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
				.Text(NSLOCTEXT("EnvironmentQueryEditor", "TheQueryGraphLabel", "Query Graph"))
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


TSharedRef<SDockTab> FEnvironmentQueryEditor::SpawnTab_UpdateGraph( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId().TabType == EQSUpdateGraphTabId );
	UEnvironmentQueryGraph* MyGraph = Cast<UEnvironmentQueryGraph>(Query->EdGraph);
	if (Query->EdGraph == NULL)
	{
		MyGraph = NewObject<UEnvironmentQueryGraph>(Query, NAME_None, RF_Transactional);
		Query->EdGraph = MyGraph;

		// let's read data from BT script and generate nodes
		const UEdGraphSchema* Schema = Query->EdGraph->GetSchema();
		Schema->CreateDefaultNodesForGraph(*Query->EdGraph);

		MyGraph->OnCreated();
	}
	else
	{
		MyGraph->OnLoaded();
	}

	MyGraph->Initialize();

	TSharedRef<SGraphEditor> UpdateGraphEditor = CreateGraphEditorWidget(Query->EdGraph);
	UpdateGraphEdPtr = UpdateGraphEditor; // Keep pointer to editor

	return SNew(SDockTab)
		.Label( NSLOCTEXT("EnvironmentQueryEditor", "UpdateGraph", "Update Graph") )
		.TabColorScale( GetTabColorScale() )
		[
			UpdateGraphEditor
		];
}

TSharedRef<SDockTab> FEnvironmentQueryEditor::SpawnTab_Properties(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId() == EQSPropertiesTabId );

	CreateInternalWidgets();

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("SoundClassEditor.Tabs.Properties") )
		.Label( NSLOCTEXT("EnvironmentQueryEditor", "SoundClassPropertiesTitle", "Details" ) )
		[
			DetailsView.ToSharedRef()
		];

	return SpawnedTab;
}

void FEnvironmentQueryEditor::OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection)
{
	TArray<UObject*> Selection;

	SelectedNodesCount = NewSelection.Num();
	if (NewSelection.Num())
	{
		for(TSet<class UObject*>::TConstIterator SetIt(NewSelection);SetIt;++SetIt)
		{
			UEnvironmentQueryGraphNode* GraphNode = Cast<UEnvironmentQueryGraphNode>(*SetIt);
			if (GraphNode)
			{
				if (GraphNode->IsA(UEnvironmentQueryGraphNode_Root::StaticClass()))
				{
					Selection.Add(GraphNode);
				}
				else if (GraphNode->IsA(UEnvironmentQueryGraphNode_Option::StaticClass()))
				{
					UEnvQueryOption* QueryOption = Cast<UEnvQueryOption>(GraphNode->NodeInstance);
					if (QueryOption)
					{
						Selection.Add(QueryOption->Generator);
					}
				}
				else
				{
					Selection.Add(GraphNode->NodeInstance);
				}
			}
		}
	}

	if (Selection.Num() == 1)
	{
		DetailsView->SetObjects(Selection);
	}
	else
	{
		DetailsView->SetObject(NULL);
	}
}

void FEnvironmentQueryEditor::CreateInternalWidgets()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	const FDetailsViewArgs DetailsViewArgs( false, false, true, FDetailsViewArgs::ObjectsUseNameArea, false );
	DetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );
	DetailsView->SetObject( NULL );
	DetailsView->OnFinishedChangingProperties().AddSP(this, &FEnvironmentQueryEditor::OnFinishedChangingProperties);
}

void FEnvironmentQueryEditor::SaveAsset_Execute()
{
	// modify BT asset
	TSharedPtr<SGraphEditor> UpdateGraphEditor = UpdateGraphEdPtr.Pin();
	if (UpdateGraphEditor.IsValid() && UpdateGraphEditor->GetCurrentGraph() != NULL)
	{
		//let's find root node
		UEnvironmentQueryGraph* EdGraph = Cast<UEnvironmentQueryGraph>(UpdateGraphEditor->GetCurrentGraph());
		EdGraph->UpdateAsset();
	}
	// save it
	IEnvironmentQueryEditor::SaveAsset_Execute();
}

void FEnvironmentQueryEditor::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property)
	{
		FGraphPanelSelectionSet CurrentSelection = GetSelectedNodes();
		if (CurrentSelection.Num() == 1)
		{
			for (FGraphPanelSelectionSet::TConstIterator It(CurrentSelection); It; ++It)
			{
				UEnvironmentQueryGraphNode_Test* TestNode = Cast<UEnvironmentQueryGraphNode_Test>(*It);
				UEnvironmentQueryGraphNode_Option* ParentNode = TestNode ? Cast<UEnvironmentQueryGraphNode_Option>(TestNode->ParentNode) : nullptr;

				if (ParentNode)
				{
					ParentNode->CalculateWeights();
					break;
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE