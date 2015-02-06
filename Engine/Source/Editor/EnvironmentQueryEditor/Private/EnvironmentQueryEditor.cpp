// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

void FEnvironmentQueryEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	WorkspaceMenuCategory = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_EnvironmentQueryEditor", "Environment Query Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(TabManager);

	TabManager->RegisterTabSpawner( EQSUpdateGraphTabId, FOnSpawnTab::CreateSP(this, &FEnvironmentQueryEditor::SpawnTab_UpdateGraph) )
		.SetDisplayName( NSLOCTEXT("EnvironmentQueryEditor", "Graph", "Graph") )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "GraphEditor.EventGraph_16x"));

	TabManager->RegisterTabSpawner( EQSPropertiesTabId, FOnSpawnTab::CreateSP(this, &FEnvironmentQueryEditor::SpawnTab_Properties) )
		.SetDisplayName( NSLOCTEXT("EnvironmentQueryEditor", "PropertiesTab", "Details" ) )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FEnvironmentQueryEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(TabManager);

	TabManager->UnregisterTabSpawner( EQSPropertiesTabId );
	TabManager->UnregisterTabSpawner( EQSUpdateGraphTabId );
}

FEnvironmentQueryEditor::FEnvironmentQueryEditor() : IEnvironmentQueryEditor()
{
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor != NULL)
	{
		Editor->RegisterForUndo(this);
	}
}

FEnvironmentQueryEditor::~FEnvironmentQueryEditor()
{
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor)
	{
		Editor->UnregisterForUndo( this );
	}
}

void FEnvironmentQueryEditor::InitEnvironmentQueryEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UEnvQuery* InScript )
{
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
	
	GraphEditorCommands = MakeShareable( new FUICommandList );
	{
		// Editing commands
		GraphEditorCommands->MapAction( FGenericCommands::Get().SelectAll,
			FExecuteAction::CreateSP( this, &FEnvironmentQueryEditor::SelectAllNodes ),
			FCanExecuteAction::CreateSP( this, &FEnvironmentQueryEditor::CanSelectAllNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Delete,
			FExecuteAction::CreateSP( this, &FEnvironmentQueryEditor::DeleteSelectedNodes ),
			FCanExecuteAction::CreateSP( this, &FEnvironmentQueryEditor::CanDeleteNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Copy,
			FExecuteAction::CreateSP( this, &FEnvironmentQueryEditor::CopySelectedNodes ),
			FCanExecuteAction::CreateSP( this, &FEnvironmentQueryEditor::CanCopyNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Cut,
			FExecuteAction::CreateSP( this, &FEnvironmentQueryEditor::CutSelectedNodes ),
			FCanExecuteAction::CreateSP( this, &FEnvironmentQueryEditor::CanCutNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Paste,
			FExecuteAction::CreateSP( this, &FEnvironmentQueryEditor::PasteNodes ),
			FCanExecuteAction::CreateSP( this, &FEnvironmentQueryEditor::CanPasteNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Duplicate,
			FExecuteAction::CreateSP( this, &FEnvironmentQueryEditor::DuplicateNodes ),
			FCanExecuteAction::CreateSP( this, &FEnvironmentQueryEditor::CanDuplicateNodes )
			);
	}

	// Create the appearance info
	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = NSLOCTEXT("EnvironmentQueryEditor", "AppearanceCornerText", "ENVIRONMENT QUERY");

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FEnvironmentQueryEditor::OnSelectedNodesChanged);

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
	if (Query->EdGraph == NULL)
	{
		UEnvironmentQueryGraph* MyGraph = ConstructObject<UEnvironmentQueryGraph>(UEnvironmentQueryGraph::StaticClass(), Query, NAME_None, RF_Transactional);
		MyGraph->MarkVersion();

		Query->EdGraph = MyGraph;

		// let's read data from BT script and generate nodes
		const UEdGraphSchema* Schema = Query->EdGraph->GetSchema();
		Schema->CreateDefaultNodesForGraph(*Query->EdGraph);
	}

	// Conversion should be removed are a while
	UEnvironmentQueryGraph* QueryGraph = Cast<UEnvironmentQueryGraph>(Query->EdGraph);
	QueryGraph->UpdateVersion();
	QueryGraph->CalculateAllWeights();

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

	if(NewSelection.Num())
	{
		for(TSet<class UObject*>::TConstIterator SetIt(NewSelection);SetIt;++SetIt)
		{
			UEnvironmentQueryGraphNode* GraphNode = CastChecked<UEnvironmentQueryGraphNode>(*SetIt);
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
				if (TestNode && TestNode->ParentNode)
				{
					TestNode->ParentNode->CalculateWeights();
					break;
				}
			}
		}
	}
}

void FEnvironmentQueryEditor::PostUndo(bool bSuccess)
{	
	if (bSuccess)
	{
		// Clear selection, to avoid holding refs to nodes that go away
		TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
		if (CurrentGraphEditor.IsValid())
		{
			CurrentGraphEditor->ClearSelectionSet();
			CurrentGraphEditor->NotifyGraphChanged();
		}
		FSlateApplication::Get().DismissAllMenus();
	}
}

void FEnvironmentQueryEditor::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
		// Clear selection, to avoid holding refs to nodes that go away
		TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
		if (CurrentGraphEditor.IsValid())
		{
			CurrentGraphEditor->ClearSelectionSet();
			CurrentGraphEditor->NotifyGraphChanged();
		}
		FSlateApplication::Get().DismissAllMenus();
	}
}

FGraphPanelSelectionSet FEnvironmentQueryEditor::GetSelectedNodes() const
{
	FGraphPanelSelectionSet CurrentSelection;
	TSharedPtr<SGraphEditor> FocusedGraphEd = UpdateGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		CurrentSelection = FocusedGraphEd->GetSelectedNodes();
	}
	return CurrentSelection;
}

void FEnvironmentQueryEditor::SelectAllNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	if (CurrentGraphEditor.IsValid())
	{
		CurrentGraphEditor->SelectAllNodes();
	}
}

bool FEnvironmentQueryEditor::CanSelectAllNodes() const
{
	return true;
}

void FEnvironmentQueryEditor::DeleteSelectedNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction( FGenericCommands::Get().Delete->GetDescription() );
	CurrentGraphEditor->GetCurrentGraph()->Modify();

	const FGraphPanelSelectionSet SelectedNodes = CurrentGraphEditor->GetSelectedNodes();
	CurrentGraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt( SelectedNodes ); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
		{
			if (Node->CanUserDeleteNode())
			{
				Node->Modify();
				Node->DestroyNode();
			}
		}
	}
}

bool FEnvironmentQueryEditor::CanDeleteNodes() const
{
	// If any of the nodes can be deleted then we should allow deleting
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanUserDeleteNode())
		{
			return true;
		}
	}

	return false;
}

void FEnvironmentQueryEditor::DeleteSelectedDuplicatableNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}

	const FGraphPanelSelectionSet OldSelectedNodes = CurrentGraphEditor->GetSelectedNodes();
	CurrentGraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanDuplicateNode())
		{
			CurrentGraphEditor->SetNodeSelection(Node, true);
		}
	}

	// Delete the duplicatable nodes
	DeleteSelectedNodes();

	CurrentGraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
		{
			CurrentGraphEditor->SetNodeSelection(Node, true);
		}
	}
}

void FEnvironmentQueryEditor::CutSelectedNodes()
{
	CopySelectedNodes();
	DeleteSelectedDuplicatableNodes();
}

bool FEnvironmentQueryEditor::CanCutNodes() const
{
	return CanCopyNodes() && CanDeleteNodes();
}

void FEnvironmentQueryEditor::CopySelectedNodes()
{
	// Export the selected nodes and place the text on the clipboard
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	TArray<UEnvironmentQueryGraphNode*> SubNodes;

	FString ExportedText;

	int32 CopySubNodeIndex = 0;
	for (FGraphPanelSelectionSet::TIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEnvironmentQueryGraphNode_Option* OptionNode = Cast<UEnvironmentQueryGraphNode_Option>(*SelectedIter);
		UEnvironmentQueryGraphNode_Test* TestNode = Cast<UEnvironmentQueryGraphNode_Test>(*SelectedIter);
		UEnvironmentQueryGraphNode* Node = OptionNode ? (UEnvironmentQueryGraphNode*)OptionNode : (UEnvironmentQueryGraphNode*)TestNode;

		if (OptionNode == NULL && TestNode == NULL)
		{
			SelectedIter.RemoveCurrent();
			continue;
		}

		Node->PrepareForCopying();
		Node->CopySubNodeIndex = CopySubNodeIndex;

		// append all subnodes for selection
		if (OptionNode)
		{
			for (int32 i = 0; i < OptionNode->Tests.Num(); i++)
			{
				OptionNode->Tests[i]->CopySubNodeIndex = CopySubNodeIndex;
				SubNodes.Add(OptionNode->Tests[i]);
			}
		}

		CopySubNodeIndex++;
	}

	for (int32 i = 0; i < SubNodes.Num(); i++)
	{
		SelectedNodes.Add(SubNodes[i]);
		SubNodes[i]->PrepareForCopying();
	}

	FEdGraphUtilities::ExportNodesToText(SelectedNodes, ExportedText);
	FPlatformMisc::ClipboardCopy(*ExportedText);

	for (FGraphPanelSelectionSet::TIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEnvironmentQueryGraphNode* Node = Cast<UEnvironmentQueryGraphNode>(*SelectedIter);
		Node->PostCopyNode();
	}
}

bool FEnvironmentQueryEditor::CanCopyNodes() const
{
	// If any of the nodes can be duplicated then we should allow copying
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanDuplicateNode())
		{
			return true;
		}
	}

	return false;
}

void FEnvironmentQueryEditor::PasteNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	if (CurrentGraphEditor.IsValid())
	{
		PasteNodesHere(CurrentGraphEditor->GetPasteLocation());
	}
}

void FEnvironmentQueryEditor::PasteNodesHere(const FVector2D& Location)
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}

	// Undo/Redo support
	const FScopedTransaction Transaction( FGenericCommands::Get().Paste->GetDescription() );
	UEnvironmentQueryGraph* EdGraph = Cast<UEnvironmentQueryGraph>(CurrentGraphEditor->GetCurrentGraph());
	EdGraph->Modify();
	EdGraph->LockUpdates();

	// Find currently selected option to paste all new tests into
	UEnvironmentQueryGraphNode_Option* SelectedOption = NULL;
	bool bHasMultipleOptionsSelected = false;

	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEnvironmentQueryGraphNode_Option* Node = Cast<UEnvironmentQueryGraphNode_Option>(*SelectedIter);
		UEnvironmentQueryGraphNode_Test* TestNode = Cast<UEnvironmentQueryGraphNode_Test>(*SelectedIter);

		if (TestNode)
		{
			Node = TestNode->ParentNode;
		}

		if (Node)
		{
			if (SelectedOption == NULL)
			{
				SelectedOption = Node;
			}
			else
			{
				bHasMultipleOptionsSelected = true;
				break;
			}
		}
	}

	// Clear the selection set (newly pasted stuff will be selected)
	CurrentGraphEditor->ClearSelectionSet();

	// Grab the text to paste from the clipboard.
	FString TextToImport;
	FPlatformMisc::ClipboardPaste(TextToImport);

	// Import the nodes
	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(EdGraph, TextToImport, /*out*/ PastedNodes);

	//Average position of nodes so we can move them while still maintaining relative distances to each other
	FVector2D AvgNodePosition(0.0f,0.0f);

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEnvironmentQueryGraphNode* BTNode = Cast<UEnvironmentQueryGraphNode>(*It);
		if (BTNode && !BTNode->IsSubNode())
		{
			AvgNodePosition.X += BTNode->NodePosX;
			AvgNodePosition.Y += BTNode->NodePosY;
		}
	}

	if (PastedNodes.Num() > 0)
	{
		float InvNumNodes = 1.0f/float(PastedNodes.Num());
		AvgNodePosition.X *= InvNumNodes;
		AvgNodePosition.Y *= InvNumNodes;
	}

	bool bPastedOptionNode = false;

	TMap<int32, UEnvironmentQueryGraphNode_Option*> ParentMap;
	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEnvironmentQueryGraphNode* PasteNode = Cast<UEnvironmentQueryGraphNode>(*It);
		if (PasteNode && !PasteNode->IsSubNode())
		{
			bPastedOptionNode = true;

			// Select the newly pasted stuff
			CurrentGraphEditor->SetNodeSelection(PasteNode, true);

			PasteNode->NodePosX = (PasteNode->NodePosX - AvgNodePosition.X) + Location.X ;
			PasteNode->NodePosY = (PasteNode->NodePosY - AvgNodePosition.Y) + Location.Y ;

			PasteNode->SnapToGrid(16);

			// Give new node a different Guid from the old one
			PasteNode->CreateNewGuid();

			UEnvironmentQueryGraphNode_Option* PasteOption = Cast<UEnvironmentQueryGraphNode_Option>(PasteNode);
			if (PasteOption)
			{
				PasteOption->Tests.Reset();
				ParentMap.Add(PasteOption->CopySubNodeIndex, PasteOption);
			}			
		}
	}

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEnvironmentQueryGraphNode_Test* PasteTest = Cast<UEnvironmentQueryGraphNode_Test>(*It);
		if (PasteTest)
		{
			PasteTest->NodePosX = 0;
			PasteTest->NodePosY = 0;

			// Give new node a different Guid from the old one
			PasteTest->CreateNewGuid();

			// remove subnode from graph, it will be referenced from parent node
			PasteTest->DestroyNode();

			PasteTest->ParentNode = ParentMap.FindRef(PasteTest->CopySubNodeIndex);
			if (PasteTest->ParentNode)
			{
				PasteTest->ParentNode->Tests.Add(PasteTest);
			}
			else if (!bHasMultipleOptionsSelected && !bPastedOptionNode && SelectedOption)
			{
				PasteTest->ParentNode = SelectedOption;
				SelectedOption->Tests.Add(PasteTest);
				SelectedOption->Modify();
			}
		}
	}

	// Update UI
	EdGraph->UnlockUpdates();
	CurrentGraphEditor->NotifyGraphChanged();

	Query->PostEditChange();
	Query->MarkPackageDirty();
}

bool FEnvironmentQueryEditor::CanPasteNodes() const
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformMisc::ClipboardPaste(ClipboardContent);

	return FEdGraphUtilities::CanImportNodesFromText(CurrentGraphEditor->GetCurrentGraph(), ClipboardContent);
}

void FEnvironmentQueryEditor::DuplicateNodes()
{
	CopySelectedNodes();
	PasteNodes();
}

bool FEnvironmentQueryEditor::CanDuplicateNodes() const
{
	return CanCopyNodes();
}

#undef LOCTEXT_NAMESPACE