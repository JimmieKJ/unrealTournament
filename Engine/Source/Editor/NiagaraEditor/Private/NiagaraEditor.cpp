// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "NiagaraScript.h"

#include "Toolkits/IToolkitHost.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "SDockTab.h"
#include "GenericCommands.h"
#include "EdGraphUtilities.h"

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"
#include "ScopedTransaction.h"
#include "BlueprintEditorUtils.h"
 
#define LOCTEXT_NAMESPACE "NiagaraEditor"

const FName FNiagaraEditor::NodeGraphTabId(TEXT("NiagaraEditor_NodeGraph")); 
const FName FNiagaraEditor::FlattenedNodeGraphTabId(TEXT("NiagaraEditor_FlattenedNodeGraph")); 
const FName FNiagaraEditor::PropertiesTabId(TEXT("NiagaraEditor_MaterialProperties"));

void FNiagaraEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_NiagaraEditor", "Niagara"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
	
	TSharedRef<FWorkspaceItem> WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	InTabManager->RegisterTabSpawner( NodeGraphTabId, FOnSpawnTab::CreateSP(this, &FNiagaraEditor::SpawnTab_NodeGraph) )
		.SetDisplayName( LOCTEXT("NodeGraph", "Node Graph") )
		.SetGroup(WorkspaceMenuCategoryRef); 

	InTabManager->RegisterTabSpawner( FlattenedNodeGraphTabId, FOnSpawnTab::CreateSP(this, &FNiagaraEditor::SpawnTab_FlattenedNodeGraph))
		.SetDisplayName(LOCTEXT("FlattenedNodeGraph", "Flattened Node Graph"))
		.SetGroup(WorkspaceMenuCategoryRef); 

	InTabManager->RegisterTabSpawner(PropertiesTabId, FOnSpawnTab::CreateSP(this, &FNiagaraEditor::SpawnTab_NodeProperties))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FNiagaraEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(NodeGraphTabId);
	InTabManager->UnregisterTabSpawner(PropertiesTabId);
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
TSharedRef<SGraphEditor> FNiagaraEditor::CreateGraphEditorWidget(UEdGraph* InGraph, bool bEditable)
{
	check(InGraph != NULL);
	
	if ( !GraphEditorCommands.IsValid() )
	{
		GraphEditorCommands = MakeShareable( new FUICommandList );
	
		// Editing commands
		GraphEditorCommands->MapAction(FGenericCommands::Get().SelectAll,
			FExecuteAction::CreateRaw(this, &FNiagaraEditor::SelectAllNodes),
			FCanExecuteAction::CreateRaw(this, &FNiagaraEditor::CanSelectAllNodes)
			);

		GraphEditorCommands->MapAction(FGenericCommands::Get().Delete,
			FExecuteAction::CreateRaw(this, &FNiagaraEditor::DeleteSelectedNodes),
			FCanExecuteAction::CreateRaw(this, &FNiagaraEditor::CanDeleteNodes)
			);

		GraphEditorCommands->MapAction(FGenericCommands::Get().Copy,
			FExecuteAction::CreateRaw(this, &FNiagaraEditor::CopySelectedNodes),
			FCanExecuteAction::CreateRaw(this, &FNiagaraEditor::CanCopyNodes)
			);

		GraphEditorCommands->MapAction(FGenericCommands::Get().Cut,
			FExecuteAction::CreateRaw(this, &FNiagaraEditor::CutSelectedNodes),
			FCanExecuteAction::CreateRaw(this, &FNiagaraEditor::CanCutNodes)
			);

		GraphEditorCommands->MapAction(FGenericCommands::Get().Paste,
			FExecuteAction::CreateRaw(this, &FNiagaraEditor::PasteNodes),
			FCanExecuteAction::CreateRaw(this, &FNiagaraEditor::CanPasteNodes)
			);

		GraphEditorCommands->MapAction(FGenericCommands::Get().Duplicate,
			FExecuteAction::CreateRaw(this, &FNiagaraEditor::DuplicateNodes),
			FCanExecuteAction::CreateRaw(this, &FNiagaraEditor::CanDuplicateNodes)
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
		.GraphEvents(InEvents)
		.IsEditable(bEditable);
}


TSharedRef<SDockTab> FNiagaraEditor::SpawnTab_NodeGraph( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId().TabType == NodeGraphTabId );

	TSharedRef<SGraphEditor> NodeGraphEditor = CreateGraphEditorWidget(Source->NodeGraph, true);

	NodeGraphEditorPtr = NodeGraphEditor; // Keep pointer to editor

	return SNew(SDockTab)
		.Label( LOCTEXT("NodeGraph", "Node Graph") )
		.TabColorScale( GetTabColorScale() )
		[
			NodeGraphEditor
		];
}

TSharedRef<SDockTab> FNiagaraEditor::SpawnTab_FlattenedNodeGraph(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == FlattenedNodeGraphTabId);

	TSharedRef<SGraphEditor> NodeGraphEditor = CreateGraphEditorWidget(Source->FlattenedNodeGraph, false);

	FlattenedNodeGraphEditorPtr = NodeGraphEditor; // Keep pointer to editor

	return SNew(SDockTab)
		.Label(LOCTEXT("Flattened NodeGraph", "Flattened Node Graph"))
		.TabColorScale(GetTabColorScale())
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

	TSharedPtr<SGraphEditor> CurrentGraphEditor = NodeGraphEditorPtr.Pin();
	if (CurrentGraphEditor.IsValid())
		CurrentGraphEditor->NotifyGraphChanged();

	TSharedPtr<SGraphEditor> FlattenedGraphEditor = FlattenedNodeGraphEditorPtr.Pin();
	if (FlattenedGraphEditor.IsValid())
		FlattenedGraphEditor->NotifyGraphChanged();

	return FReply::Handled();
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

void FNiagaraEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		// Clear selection, to avoid holding refs to nodes that go away
		TSharedPtr<SGraphEditor> CurrentGraphEditor = NodeGraphEditorPtr.Pin();
		if (CurrentGraphEditor.IsValid())
		{
			CurrentGraphEditor->ClearSelectionSet();
			CurrentGraphEditor->NotifyGraphChanged();
		}
		FSlateApplication::Get().DismissAllMenus();
	}
}

void FNiagaraEditor::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
		// Clear selection, to avoid holding refs to nodes that go away
		TSharedPtr<SGraphEditor> CurrentGraphEditor = NodeGraphEditorPtr.Pin();
		if (CurrentGraphEditor.IsValid())
		{
			CurrentGraphEditor->ClearSelectionSet();
			CurrentGraphEditor->NotifyGraphChanged();
		}
		FSlateApplication::Get().DismissAllMenus();
	}
}

void FNiagaraEditor::SelectAllNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = NodeGraphEditorPtr.Pin();
	if (CurrentGraphEditor.IsValid())
	{
		CurrentGraphEditor->SelectAllNodes();
	}
}

bool FNiagaraEditor::CanSelectAllNodes() const
{
	return true;
}

void FNiagaraEditor::DeleteSelectedNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = NodeGraphEditorPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(FGenericCommands::Get().Delete->GetDescription());
	CurrentGraphEditor->GetCurrentGraph()->Modify();

	const FGraphPanelSelectionSet SelectedNodes = CurrentGraphEditor->GetSelectedNodes();
	CurrentGraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
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

bool FNiagaraEditor::CanDeleteNodes() const
{
	// If any of the nodes can be deleted then we should allow deleting
	TSharedPtr<SGraphEditor> CurrentGraphEditor = NodeGraphEditorPtr.Pin();
	if (CurrentGraphEditor.IsValid())
	{
		const FGraphPanelSelectionSet SelectedNodes = CurrentGraphEditor->GetSelectedNodes();
		for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
		{
			UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
			if (Node && Node->CanUserDeleteNode())
			{
				return true;
			}
		}
	}
	return false;
}

void FNiagaraEditor::DeleteSelectedDuplicatableNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = NodeGraphEditorPtr.Pin();
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

void FNiagaraEditor::CutSelectedNodes()
{
	CopySelectedNodes();
	DeleteSelectedDuplicatableNodes();
}

bool FNiagaraEditor::CanCutNodes() const
{
	return CanCopyNodes() && CanDeleteNodes();
}

void FNiagaraEditor::CopySelectedNodes()
{
	// Export the selected nodes and place the text on the clipboard
	TSharedPtr<SGraphEditor> CurrentGraphEditor = NodeGraphEditorPtr.Pin();
	FGraphPanelSelectionSet SelectedNodes = CurrentGraphEditor->GetSelectedNodes();

	FString ExportedText;

	int32 CopySubNodeIndex = 0;
	for (FGraphPanelSelectionSet::TIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node == nullptr)
		{
			SelectedIter.RemoveCurrent();
			continue;
		}

		Node->PrepareForCopying();
	}
	
	FEdGraphUtilities::ExportNodesToText(SelectedNodes, ExportedText);
	FPlatformMisc::ClipboardCopy(*ExportedText);
}

bool FNiagaraEditor::CanCopyNodes() const
{
	// If any of the nodes can be duplicated then we should allow copying
	TSharedPtr<SGraphEditor> CurrentGraphEditor = NodeGraphEditorPtr.Pin();
	const FGraphPanelSelectionSet SelectedNodes = CurrentGraphEditor->GetSelectedNodes();
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

void FNiagaraEditor::PasteNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = NodeGraphEditorPtr.Pin();
	if (CurrentGraphEditor.IsValid())
	{
		PasteNodesHere(CurrentGraphEditor->GetPasteLocation());
	}
}

void FNiagaraEditor::PasteNodesHere(const FVector2D& Location)
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = NodeGraphEditorPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}

	// Undo/Redo support
	const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());
	UEdGraph* EdGraph = Cast<UEdGraph>(CurrentGraphEditor->GetCurrentGraph());
	EdGraph->Modify();

	const FGraphPanelSelectionSet SelectedNodes = CurrentGraphEditor->GetSelectedNodes();

	// Clear the selection set (newly pasted stuff will be selected)
	CurrentGraphEditor->ClearSelectionSet();

	// Grab the text to paste from the clipboard.
	FString TextToImport;
	FPlatformMisc::ClipboardPaste(TextToImport);

	// Import the nodes
	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(EdGraph, TextToImport, /*out*/ PastedNodes);

	//Average position of nodes so we can move them while still maintaining relative distances to each other
	FVector2D AvgNodePosition(0.0f, 0.0f);

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* Node = (*It);
		if (Node)
		{
			AvgNodePosition.X += Node->NodePosX;
			AvgNodePosition.Y += Node->NodePosY;
		}
	}

	if (PastedNodes.Num() > 0)
	{
		float InvNumNodes = 1.0f / float(PastedNodes.Num());
		AvgNodePosition.X *= InvNumNodes;
		AvgNodePosition.Y *= InvNumNodes;
	}
	
	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* PasteNode = (*It);
		if (PasteNode)
		{
			// Select the newly pasted stuff
			CurrentGraphEditor->SetNodeSelection(PasteNode, true);

			PasteNode->NodePosX = (PasteNode->NodePosX - AvgNodePosition.X) + Location.X;
			PasteNode->NodePosY = (PasteNode->NodePosY - AvgNodePosition.Y) + Location.Y;

			PasteNode->SnapToGrid(16);

			// Give new node a different Guid from the old one
			PasteNode->CreateNewGuid();
		}
	}


	// Update UI
	CurrentGraphEditor->NotifyGraphChanged();

	UObject* GraphOwner = EdGraph->GetOuter();
	if (GraphOwner)
	{
		GraphOwner->PostEditChange();
		GraphOwner->MarkPackageDirty();
	}
}

bool FNiagaraEditor::CanPasteNodes() const
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = NodeGraphEditorPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformMisc::ClipboardPaste(ClipboardContent);

	return FEdGraphUtilities::CanImportNodesFromText(CurrentGraphEditor->GetCurrentGraph(), ClipboardContent);
}

void FNiagaraEditor::DuplicateNodes()
{
	CopySelectedNodes();
	PasteNodes();
}

bool FNiagaraEditor::CanDuplicateNodes() const
{
	return CanCopyNodes();
}

#undef LOCTEXT_NAMESPACE
