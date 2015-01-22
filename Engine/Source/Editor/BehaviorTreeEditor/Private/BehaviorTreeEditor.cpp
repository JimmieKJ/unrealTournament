// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeEditorPrivatePCH.h"
#include "GraphEditorActions.h"
#include "ScopedTransaction.h"
#include "BehaviorTreeColors.h"
#include "EdGraphUtilities.h"

#include "BehaviorTree/Composites/BTComposite_SimpleParallel.h"
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"
#include "BehaviorTree/Tasks/BTTask_BlueprintBase.h"
#include "BehaviorTree/Decorators/BTDecorator_BlueprintBase.h"
#include "BehaviorTree/Services/BTService_BlueprintBase.h"
#include "BehaviorTree/BTDecorator.h"

#include "Toolkits/IToolkitHost.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"

#include "BehaviorTreeEditorModes.h"
#include "BehaviorTreeEditorToolbar.h"
#include "BehaviorTreeEditorTabFactories.h"
#include "BehaviorTreeEditorCommands.h"
#include "BehaviorTreeEditorTabs.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "DetailCustomizations/BlackboardDataDetails.h"
#include "SBehaviorTreeBlackboardView.h"
#include "SBehaviorTreeBlackboardEditor.h"
#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "SDockTab.h"
#include "GenericCommands.h"

#define LOCTEXT_NAMESPACE "BehaviorTreeEditor"

const FName FBehaviorTreeEditor::BehaviorTreeMode(TEXT("BehaviorTree"));
const FName FBehaviorTreeEditor::BlackboardMode(TEXT("Blackboard"));

//////////////////////////////////////////////////////////////////////////
FBehaviorTreeEditor::FBehaviorTreeEditor() 
	: IBehaviorTreeEditor()
{
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor != NULL)
	{
		Editor->RegisterForUndo(this);
	}

	// listen for package change events to update injected nodes
	UPackage::PackageSavedEvent.AddRaw(this, &FBehaviorTreeEditor::OnPackageSaved);
	FClassBrowseHelper::OnPackageListUpdated.AddRaw(this, &FBehaviorTreeEditor::OnClassListUpdated);

	bShowDecoratorRangeLower = false;
	bShowDecoratorRangeSelf = false;
	bSelectedNodeIsInjected = false;
	SelectedNodesCount = 0;

	BehaviorTree = nullptr;
	BlackboardData = nullptr;
}

FBehaviorTreeEditor::~FBehaviorTreeEditor()
{
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor)
	{
		Editor->UnregisterForUndo( this );
	}

	UPackage::PackageSavedEvent.RemoveRaw(this, &FBehaviorTreeEditor::OnPackageSaved);
	FClassBrowseHelper::OnPackageListUpdated.RemoveRaw(this, &FBehaviorTreeEditor::OnClassListUpdated);

	Debugger.Reset();
}

void FBehaviorTreeEditor::PostUndo(bool bSuccess)
{	
	if (bSuccess)
	{
		// Clear selection, to avoid holding refs to nodes that go away
		TSharedPtr<SGraphEditor> CurrentGraphEditor = FocusedGraphEdPtr.Pin();
		if (CurrentGraphEditor.IsValid())
		{
			CurrentGraphEditor->ClearSelectionSet();
			CurrentGraphEditor->NotifyGraphChanged();
		}
		if(BlackboardView.IsValid())
		{
			BlackboardView->SetObject(GetBlackboardData());
		}
		if(BlackboardEditor.IsValid())
		{
			BlackboardEditor->SetObject(GetBlackboardData());
		}
		FSlateApplication::Get().DismissAllMenus();
	}
}

void FBehaviorTreeEditor::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
		// Clear selection, to avoid holding refs to nodes that go away
		TSharedPtr<SGraphEditor> CurrentGraphEditor = FocusedGraphEdPtr.Pin();
		if (CurrentGraphEditor.IsValid())
		{
			CurrentGraphEditor->ClearSelectionSet();
			CurrentGraphEditor->NotifyGraphChanged();
		}
		if(BlackboardView.IsValid())
		{
			BlackboardView->SetObject(GetBlackboardData());
		}
		if(BlackboardEditor.IsValid())
		{
			BlackboardEditor->SetObject(GetBlackboardData());
		}
		FSlateApplication::Get().DismissAllMenus();
	}
}

void FBehaviorTreeEditor::NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged )
{
	if(PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		if(PropertyChangedEvent.Property != nullptr && PropertyChangedEvent.Property->GetFName() == TEXT("BlackboardAsset"))
		{
			BlackboardData = BehaviorTree->BlackboardAsset;
		}

		if(BlackboardView.IsValid())
		{
			BlackboardView->SetObject(GetBlackboardData());
		}
		if(BlackboardEditor.IsValid())
		{
			BlackboardEditor->SetObject(GetBlackboardData());
		}
	}
}

void FBehaviorTreeEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	DocumentManager->SetTabManager(TabManager);

	FWorkflowCentricApplication::RegisterTabSpawners(TabManager);
}

void FBehaviorTreeEditor::RegisterToolbarTab(const TSharedRef<class FTabManager>& TabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(TabManager);
}

void FBehaviorTreeEditor::InitBehaviorTreeEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* InObject )
{
	UBehaviorTree* BehaviorTreeToEdit = Cast<UBehaviorTree>(InObject);
	UBlackboardData* BlackboardDataToEdit = Cast<UBlackboardData>(InObject);

	if(BehaviorTreeToEdit != nullptr)
	{
		BehaviorTree = BehaviorTreeToEdit;
		if(BehaviorTree->BlackboardAsset != nullptr)
		{
			BlackboardData = BehaviorTree->BlackboardAsset;
		}
	}
	else if(BlackboardDataToEdit != nullptr)
	{
		BlackboardData = BlackboardDataToEdit;
	}

	TSharedPtr<FBehaviorTreeEditor> ThisPtr(SharedThis(this));
	if(!DocumentManager.IsValid())
	{
		DocumentManager = MakeShareable(new FDocumentTracker);
		DocumentManager->Initialize(ThisPtr);

		// Register the document factories
		{
			TSharedRef<FDocumentTabFactory> GraphEditorFactory = MakeShareable(new FBTGraphEditorSummoner(ThisPtr,
				FBTGraphEditorSummoner::FOnCreateGraphEditorWidget::CreateSP(this, &FBehaviorTreeEditor::CreateGraphEditorWidget)
				));

			// Also store off a reference to the grapheditor factory so we can find all the tabs spawned by it later.
			GraphEditorTabFactoryPtr = GraphEditorFactory;
			DocumentManager->RegisterDocumentFactory(GraphEditorFactory);
		}
	}

	TArray<UObject*> ObjectsToEdit;
	if(BehaviorTree != nullptr)
	{
		ObjectsToEdit.Add(BehaviorTree);
	}
	if(BlackboardData != nullptr)
	{
		ObjectsToEdit.Add(BlackboardData);
	}

	if (!ToolbarBuilder.IsValid())
	{
		ToolbarBuilder = MakeShareable(new FBehaviorTreeEditorToolbar(SharedThis(this)));
	}

	// if we are already editing objects, dont try to recreate the editor from scratch
	const TArray<UObject*>* EditedObjects = GetObjectsCurrentlyBeingEdited();
	if(EditedObjects == nullptr || EditedObjects->Num() == 0)
	{
		FGraphEditorCommands::Register();
		FBTCommonCommands::Register();
		FBTDebuggerCommands::Register();
		FBTBlackboardCommands::Register();

		const TSharedRef<FTabManager::FLayout> DummyLayout = FTabManager::NewLayout("NullLayout")->AddArea(FTabManager::NewPrimaryArea());
		const bool bCreateDefaultStandaloneMenu = true;
		const bool bCreateDefaultToolbar = true;
		InitAssetEditor( Mode, InitToolkitHost, FBehaviorTreeEditorModule::BehaviorTreeEditorAppIdentifier, DummyLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit );

		BindCommonCommands();
		ExtendMenu();
		CreateInternalWidgets();

		Debugger = MakeShareable(new FBehaviorTreeDebugger);
		Debugger->Setup(BehaviorTree, SharedThis(this));
		Debugger->OnDebuggedBlackboardChanged().AddSP(this, &FBehaviorTreeEditor::HandleDebuggedBlackboardChanged);
		BindDebuggerToolbarCommands();

		FBehaviorTreeEditorModule& BehaviorTreeEditorModule = FModuleManager::LoadModuleChecked<FBehaviorTreeEditorModule>( "BehaviorTreeEditor" );
		AddMenuExtender(BehaviorTreeEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

		AddApplicationMode(BehaviorTreeMode, MakeShareable(new FBehaviorTreeEditorApplicationMode(SharedThis(this))));
		AddApplicationMode(BlackboardMode, MakeShareable(new FBlackboardEditorApplicationMode(SharedThis(this))));

		BlackboardView = SNew(SBehaviorTreeBlackboardView, GetToolkitCommands(), GetBlackboardData())
			.OnGetDebugKeyValue(this, &FBehaviorTreeEditor::HandleGetDebugKeyValue)
			.OnIsDebuggerReady(this, &FBehaviorTreeEditor::IsDebuggerReady)
			.OnIsDebuggerPaused(this, &FBehaviorTreeEditor::IsDebuggerPaused)
			.OnGetDebugTimeStamp(this, &FBehaviorTreeEditor::HandleGetDebugTimeStamp)
			.OnGetDisplayCurrentState(this, &FBehaviorTreeEditor::HandleGetDisplayCurrentState);

		BlackboardEditor = SNew(SBehaviorTreeBlackboardEditor, GetToolkitCommands(), GetBlackboardData())
				.OnEntrySelected(this, &FBehaviorTreeEditor::HandleBlackboardEntrySelected)
				.OnGetDebugKeyValue(this, &FBehaviorTreeEditor::HandleGetDebugKeyValue)
				.OnIsDebuggerReady(this, &FBehaviorTreeEditor::IsDebuggerReady)
				.OnIsDebuggerPaused(this, &FBehaviorTreeEditor::IsDebuggerPaused)
				.OnGetDebugTimeStamp(this, &FBehaviorTreeEditor::HandleGetDebugTimeStamp)
				.OnGetDisplayCurrentState(this, &FBehaviorTreeEditor::HandleGetDisplayCurrentState)
				.OnBlackboardKeyChanged(this, &FBehaviorTreeEditor::HandleBlackboardKeyChanged)
				.OnIsBlackboardModeActive(this, &FBehaviorTreeEditor::HandleIsBlackboardModeActive);
	}
	else
	{
		check(Debugger.IsValid());
		Debugger->Setup(BehaviorTree, SharedThis(this));
	}

	if(BehaviorTreeToEdit != nullptr)
	{
		SetCurrentMode(BehaviorTreeMode);
	}
	else if(BlackboardDataToEdit != nullptr)
	{
		SetCurrentMode(BlackboardMode);
	}

	RegenerateMenusAndToolbars();
}

void FBehaviorTreeEditor::RestoreBehaviorTree()
{
	// Update BT asset data based on saved graph to have correct data in editor
	UBehaviorTreeGraph* MyGraph = Cast<UBehaviorTreeGraph>(BehaviorTree->BTGraph);
	const bool bNewGraph = MyGraph == NULL;
	if (MyGraph == NULL)
	{
		BehaviorTree->BTGraph = FBlueprintEditorUtils::CreateNewGraph(BehaviorTree, TEXT("Behavior Tree"), UBehaviorTreeGraph::StaticClass(), UEdGraphSchema_BehaviorTree::StaticClass());
		MyGraph = Cast<UBehaviorTreeGraph>(BehaviorTree->BTGraph);
		check(MyGraph);

		// Initialize the behavior tree graph
		const UEdGraphSchema* Schema = MyGraph->GetSchema();
		Schema->CreateDefaultNodesForGraph(*MyGraph);
	}
	else
	{
		MyGraph->UpdatePinConnectionTypes();
		MyGraph->UpdateDeprecatedNodes();
	}

	MyGraph->UpdateBlackboardChange();
	MyGraph->UpdateInjectedNodes();
	MyGraph->UpdateUnknownNodeClasses();

	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(MyGraph);
	TSharedPtr<SDockTab> DocumentTab = DocumentManager->OpenDocument(Payload, bNewGraph ? FDocumentTracker::OpenNewDocument : FDocumentTracker::RestorePreviousDocument);

	if(BehaviorTree->LastEditedDocuments.Num() > 0)
	{
		TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(DocumentTab->GetContent());
		GraphEditor->SetViewLocation(BehaviorTree->LastEditedDocuments[0].SavedViewOffset, BehaviorTree->LastEditedDocuments[0].SavedZoomAmount);
	}

	const bool bIncreaseVersionNum = false;
	if(bNewGraph)
	{
		MyGraph->UpdateAsset(UBehaviorTreeGraph::ClearDebuggerFlags, bIncreaseVersionNum);
	}
	else
	{
		MyGraph->UpdateAsset(UBehaviorTreeGraph::SkipDebuggerFlags, bIncreaseVersionNum);
		RefreshDebugger();
	}

	FAbortDrawHelper EmptyMode;
	bShowDecoratorRangeLower = false;
	bShowDecoratorRangeSelf = false;
	bSelectedNodeIsInjected = false;
	MyGraph->UpdateAbortHighlight(EmptyMode, EmptyMode);
}

void FBehaviorTreeEditor::SaveEditedObjectState()
{
	// Clear currently edited documents
	BehaviorTree->LastEditedDocuments.Empty();

	// Ask all open documents to save their state, which will update LastEditedDocuments
	DocumentManager->SaveAllState();
}

void FBehaviorTreeEditor::HandleBlackboardEntrySelected(const FBlackboardEntry* BlackboardEntry, bool bIsInherited)
{
	// refresh details view
	const bool bForceRefresh = true;
	BlackboardDetailsView->SetObject(GetBlackboardData(), bForceRefresh);
}

int32 FBehaviorTreeEditor::HandleGetSelectedBlackboardItemIndex(bool& bIsInherited)
{
	if(BlackboardEditor.IsValid())
	{
		return BlackboardEditor->GetSelectedEntryIndex(bIsInherited);
	}

	return INDEX_NONE;
}

FText FBehaviorTreeEditor::HandleGetDebugKeyValue(const FName& InKeyName, bool bUseCurrentState) const
{
	if(IsDebuggerReady())
	{
		return Debugger->FindValueForKey(InKeyName, bUseCurrentState);
	}

	return FText();
}

float FBehaviorTreeEditor::HandleGetDebugTimeStamp(bool bUseCurrentState) const
{
	if(IsDebuggerReady())
	{
		return Debugger->GetTimeStamp(bUseCurrentState);
	}

	return 0.0f;
}

void FBehaviorTreeEditor::HandleDebuggedBlackboardChanged(UBlackboardData* InBlackboardData)
{
	if(BlackboardView.IsValid())
	{
		BlackboardView->SetObject(InBlackboardData);
	}
	if(BlackboardEditor.IsValid())
	{
		BlackboardEditor->SetObject(InBlackboardData);
	}
}

bool FBehaviorTreeEditor::HandleGetDisplayCurrentState() const
{
	if(IsDebuggerReady())
	{
		return Debugger->IsShowingCurrentState();
	}

	return false;
}

void FBehaviorTreeEditor::HandleBlackboardKeyChanged(UBlackboardData* InBlackboardData, FBlackboardEntry* const InKey)
{
	if(BlackboardView.IsValid())
	{
		// re-set object in blackboard view to keep it up to date
		BlackboardView->SetObject(InBlackboardData);
	}
}

bool FBehaviorTreeEditor::HandleIsBlackboardModeActive() const
{
	return GetCurrentMode() == BlackboardMode;
}

void FBehaviorTreeEditor::GetBlackboardSelectionInfo(int32& OutSelectionIndex, bool& bOutIsInherited) const
{
	OutSelectionIndex = CurrentBlackboardEntryIndex;
	bOutIsInherited = bIsCurrentBlackboardEntryInherited;
}

bool FBehaviorTreeEditor::IsDebuggerReady() const
{
	return Debugger.IsValid() && Debugger->IsDebuggerReady();
}

bool FBehaviorTreeEditor::IsDebuggerPaused() const
{
	return IsDebuggerReady() && GUnrealEd->PlayWorld && GUnrealEd->PlayWorld->bDebugPauseExecution;
}

bool FBehaviorTreeEditor::CanEditWithDebuggerActive() const
{
	if(Debugger.IsValid())
	{
		return !Debugger->IsDebuggerReady();
	}

	return true;
}

EVisibility FBehaviorTreeEditor::GetDebuggerDetailsVisibility() const
{
	return Debugger.IsValid() && Debugger->IsDebuggerRunning() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FBehaviorTreeEditor::GetRangeLowerVisibility() const
{
	return FBehaviorTreeDebugger::IsPIENotSimulating() && bShowDecoratorRangeLower ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FBehaviorTreeEditor::GetRangeSelfVisibility() const
{
	return FBehaviorTreeDebugger::IsPIENotSimulating() && bShowDecoratorRangeSelf ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FBehaviorTreeEditor::GetInjectedNodeVisibility() const
{
	return FBehaviorTreeDebugger::IsPIENotSimulating() && bSelectedNodeIsInjected ? EVisibility::Visible : EVisibility::Collapsed;
}

FGraphAppearanceInfo FBehaviorTreeEditor::GetGraphAppearance() const
{
	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText", "BEHAVIOR TREE").ToString();

	const int32 StepIdx = Debugger.IsValid() ? Debugger->GetShownStateIndex() : 0;
	if (Debugger.IsValid() && !Debugger->IsDebuggerRunning())
	{
		AppearanceInfo.PIENotifyText = TEXT("INACTIVE");
	}
	else if (StepIdx)
	{
		AppearanceInfo.PIENotifyText = FString::Printf(TEXT("%d STEP%s BACK"), StepIdx, StepIdx > 1 ? TEXT("S") : TEXT(""));
	}
	else if (FBehaviorTreeDebugger::IsPlaySessionPaused())
	{
		AppearanceInfo.PIENotifyText = TEXT("PAUSED");
	}
	
	return AppearanceInfo;
}

FName FBehaviorTreeEditor::GetToolkitFName() const
{
	return FName("Behavior Tree");
}

FText FBehaviorTreeEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "BehaviorTree");
}

FString FBehaviorTreeEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "BehaviorTree ").ToString();
}


FLinearColor FBehaviorTreeEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}


/** Create new tab for the supplied graph - don't call this directly, call SExplorer->FindTabForGraph.*/
TSharedRef<SGraphEditor> FBehaviorTreeEditor::CreateGraphEditorWidget(UEdGraph* InGraph)
{
	check(InGraph != NULL);
	
	if (!GraphEditorCommands.IsValid())
	{
		GraphEditorCommands = MakeShareable( new FUICommandList );

		// Editing commands
		GraphEditorCommands->MapAction( FGenericCommands::Get().SelectAll,
			FExecuteAction::CreateSP( this, &FBehaviorTreeEditor::SelectAllNodes ),
			FCanExecuteAction::CreateSP( this, &FBehaviorTreeEditor::CanSelectAllNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Delete,
			FExecuteAction::CreateSP( this, &FBehaviorTreeEditor::DeleteSelectedNodes ),
			FCanExecuteAction::CreateSP( this, &FBehaviorTreeEditor::CanDeleteNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Copy,
			FExecuteAction::CreateSP( this, &FBehaviorTreeEditor::CopySelectedNodes ),
			FCanExecuteAction::CreateSP( this, &FBehaviorTreeEditor::CanCopyNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Cut,
			FExecuteAction::CreateSP( this, &FBehaviorTreeEditor::CutSelectedNodes ),
			FCanExecuteAction::CreateSP( this, &FBehaviorTreeEditor::CanCutNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Paste,
			FExecuteAction::CreateSP( this, &FBehaviorTreeEditor::PasteNodes ),
			FCanExecuteAction::CreateSP( this, &FBehaviorTreeEditor::CanPasteNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Duplicate,
			FExecuteAction::CreateSP( this, &FBehaviorTreeEditor::DuplicateNodes ),
			FCanExecuteAction::CreateSP( this, &FBehaviorTreeEditor::CanDuplicateNodes )
			);

		GraphEditorCommands->MapAction( FGraphEditorCommands::Get().RemoveExecutionPin,
			FExecuteAction::CreateSP( this, &FBehaviorTreeEditor::OnRemoveInputPin ),
			FCanExecuteAction::CreateSP( this, &FBehaviorTreeEditor::CanRemoveInputPin )
			);

		GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AddExecutionPin,
			FExecuteAction::CreateSP( this, &FBehaviorTreeEditor::OnAddInputPin ),
			FCanExecuteAction::CreateSP( this, &FBehaviorTreeEditor::CanAddInputPin )
			);

		// Debug actions
		GraphEditorCommands->MapAction( FGraphEditorCommands::Get().AddBreakpoint,
			FExecuteAction::CreateSP( this, &FBehaviorTreeEditor::OnAddBreakpoint ),
			FCanExecuteAction::CreateSP( this, &FBehaviorTreeEditor::CanAddBreakpoint ),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateSP( this, &FBehaviorTreeEditor::CanAddBreakpoint )
			);

		GraphEditorCommands->MapAction( FGraphEditorCommands::Get().RemoveBreakpoint,
			FExecuteAction::CreateSP( this, &FBehaviorTreeEditor::OnRemoveBreakpoint ),
			FCanExecuteAction::CreateSP( this, &FBehaviorTreeEditor::CanRemoveBreakpoint ),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateSP( this, &FBehaviorTreeEditor::CanRemoveBreakpoint )
			);

		GraphEditorCommands->MapAction( FGraphEditorCommands::Get().EnableBreakpoint,
			FExecuteAction::CreateSP( this, &FBehaviorTreeEditor::OnEnableBreakpoint ),
			FCanExecuteAction::CreateSP( this, &FBehaviorTreeEditor::CanEnableBreakpoint ),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateSP( this, &FBehaviorTreeEditor::CanEnableBreakpoint )
			);

		GraphEditorCommands->MapAction( FGraphEditorCommands::Get().DisableBreakpoint,
			FExecuteAction::CreateSP( this, &FBehaviorTreeEditor::OnDisableBreakpoint ),
			FCanExecuteAction::CreateSP( this, &FBehaviorTreeEditor::CanDisableBreakpoint ),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateSP( this, &FBehaviorTreeEditor::CanDisableBreakpoint )
			);

		GraphEditorCommands->MapAction( FGraphEditorCommands::Get().ToggleBreakpoint,
			FExecuteAction::CreateSP( this, &FBehaviorTreeEditor::OnToggleBreakpoint ),
			FCanExecuteAction::CreateSP( this, &FBehaviorTreeEditor::CanToggleBreakpoint ),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateSP( this, &FBehaviorTreeEditor::CanToggleBreakpoint )
			);
	}

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FBehaviorTreeEditor::OnSelectedNodesChanged);
	InEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FBehaviorTreeEditor::OnNodeDoubleClicked);

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
				.Text(LOCTEXT("BehaviorTreeGraphLabel", "Behavior Tree"))
				.TextStyle( FEditorStyle::Get(), TEXT("GraphBreadcrumbButtonText") )
			]
		];

	// Make full graph editor
	const bool bGraphIsEditable = InGraph->bEditable;
	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(this, &FBehaviorTreeEditor::InEditingMode, bGraphIsEditable)
		.Appearance(this, &FBehaviorTreeEditor::GetGraphAppearance)
		.TitleBar(TitleBarWidget)
		.GraphToEdit(InGraph)
		.GraphEvents(InEvents);
}

bool FBehaviorTreeEditor::InEditingMode(bool bGraphIsEditable) const
{
	return bGraphIsEditable && FBehaviorTreeDebugger::IsPIENotSimulating();
}

TSharedRef<SWidget> FBehaviorTreeEditor::SpawnSearch()
{
	FindResults = SNew(SFindInBT, SharedThis(this));
	return FindResults.ToSharedRef();
}

TSharedRef<SWidget> FBehaviorTreeEditor::SpawnProperties()
{
	return
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		.HAlign(HAlign_Fill)
		[
			DetailsView.ToSharedRef()
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.Padding(0.0f, 5.0f)
			[
				SNew(SBorder)
				.BorderBackgroundColor(BehaviorTreeColors::NodeBody::InjectedSubNode)
				.BorderImage(FEditorStyle::GetBrush("Graph.StateNode.Body"))
				.Visibility(this, &FBehaviorTreeEditor::GetInjectedNodeVisibility)
				.Padding(FMargin(5.0f))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("InjectedNode", "Node is injected by subtree and can't be edited"))
				]
			]
			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.Padding(0.0f, 5.0f)
			[
				SNew(SBorder)
				.BorderBackgroundColor(BehaviorTreeColors::NodeBorder::HighlightAbortRange0)
				.BorderImage( FEditorStyle::GetBrush( "Graph.StateNode.Body" ) )
				.Visibility(this, &FBehaviorTreeEditor::GetRangeLowerVisibility)
				.Padding(FMargin(5.0f))
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("AbortModeHighlight", "Nodes aborted by mode: {0}"), LOCTEXT("AbortPriorityLower", "Lower Priority")))
				]
			]
			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.Padding(0.0f, 5.0f)
			[
				SNew(SBorder)
				.BorderBackgroundColor(BehaviorTreeColors::NodeBorder::HighlightAbortRange1)
				.BorderImage( FEditorStyle::GetBrush( "Graph.StateNode.Body" ) )
				.Visibility(this, &FBehaviorTreeEditor::GetRangeSelfVisibility)
				.Padding(FMargin(5.0f))
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("AbortModeHighlight", "Nodes aborted by mode: {0}"), LOCTEXT("AbortPrioritySelf", "Self")))
				]
			]
		];
}

TSharedRef<SWidget> FBehaviorTreeEditor::SpawnBlackboardView()
{
	return BlackboardView.ToSharedRef();
}

TSharedRef<SWidget> FBehaviorTreeEditor::SpawnBlackboardEditor()
{
	return BlackboardEditor.ToSharedRef();
}

TSharedRef<SWidget> FBehaviorTreeEditor::SpawnBlackboardDetails()
{
	const bool bIsUpdatable = false;
	const bool bAllowFavorites = true;
	const bool bIsLockable = false;
	const bool bAllowSearch = true;
	const bool bObjectsUseNameArea = false;
	const bool bHideSelectionTip = true;
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	FDetailsViewArgs DetailsViewArgs( bIsUpdatable, bIsLockable, bAllowSearch, bObjectsUseNameArea, bHideSelectionTip );
	DetailsViewArgs.NotifyHook = this;
	BlackboardDetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );

	FOnGetSelectedBlackboardItemIndex OnGetSelectedBlackboardItemIndex = FOnGetSelectedBlackboardItemIndex::CreateSP(this, &FBehaviorTreeEditor::HandleGetSelectedBlackboardItemIndex);
	FOnGetDetailCustomizationInstance LayoutVariableDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FBlackboardDataDetails::MakeInstance, OnGetSelectedBlackboardItemIndex);
	BlackboardDetailsView->RegisterInstancedCustomPropertyLayout(UBlackboardData::StaticClass(), LayoutVariableDetails);

	BlackboardDetailsView->SetObject(GetBlackboardData());
	BlackboardDetailsView->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FBehaviorTreeEditor::CanEditWithDebuggerActive)));

	return BlackboardDetailsView.ToSharedRef();
}

void FBehaviorTreeEditor::OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection)
{
	TArray<UObject*> Selection;
	
	UBehaviorTreeGraphNode_CompositeDecorator* FoundGraphNode_CompDecorator = NULL;
	UBTDecorator* FoundDecorator = NULL;
	bool bInjectedNode = false;

	SelectedNodesCount = NewSelection.Num();
	if(NewSelection.Num())
	{
		for(TSet<class UObject*>::TConstIterator SetIt(NewSelection);SetIt;++SetIt)
		{
			UBehaviorTreeGraphNode_Composite* GraphNode_Composite = Cast<UBehaviorTreeGraphNode_Composite>(*SetIt);
			if (GraphNode_Composite)
			{
				Selection.Add(GraphNode_Composite->NodeInstance);
				continue;
			}

			UBehaviorTreeGraphNode_Task* GraphNode_Task = Cast<UBehaviorTreeGraphNode_Task>(*SetIt);
			if (GraphNode_Task)
			{
				Selection.Add(GraphNode_Task->NodeInstance);
				continue;
			}

			UBehaviorTreeGraphNode_Decorator* GraphNode_Decorator1 = Cast<UBehaviorTreeGraphNode_Decorator>(*SetIt);
			if (GraphNode_Decorator1)
			{
				Selection.Add(GraphNode_Decorator1->NodeInstance);
				FoundDecorator = Cast<UBTDecorator>(GraphNode_Decorator1->NodeInstance);
				bInjectedNode = bInjectedNode || GraphNode_Decorator1->bInjectedNode;
				continue;
			}

			UBehaviorTreeDecoratorGraphNode_Decorator* GraphNode_Decorator2 = Cast<UBehaviorTreeDecoratorGraphNode_Decorator>(*SetIt);
			if (GraphNode_Decorator2)
			{
				Selection.Add(GraphNode_Decorator2->NodeInstance);
				bInjectedNode = bInjectedNode || !GraphNode_Decorator2->GetGraph()->bEditable;
				continue;
			}
			
			UBehaviorTreeGraphNode_Service* GraphNode_Service = Cast<UBehaviorTreeGraphNode_Service>(*SetIt);
			if (GraphNode_Service)
			{
				Selection.Add(GraphNode_Service->NodeInstance);
				continue;
			}

			UBehaviorTreeGraphNode_CompositeDecorator* GraphNode_CompDecorator = Cast<UBehaviorTreeGraphNode_CompositeDecorator>(*SetIt);
			if (GraphNode_CompDecorator)
			{
				FoundGraphNode_CompDecorator = GraphNode_CompDecorator;
				bInjectedNode = bInjectedNode || GraphNode_CompDecorator->bInjectedNode;
			}

			Selection.Add(*SetIt);
		}
	}

	UBehaviorTreeGraph* MyGraph = Cast<UBehaviorTreeGraph>(BehaviorTree->BTGraph);
	FAbortDrawHelper Mode0, Mode1;
	bShowDecoratorRangeLower = false;
	bShowDecoratorRangeSelf = false;
	bForceDisablePropertyEdit = bInjectedNode;
	bSelectedNodeIsInjected = bInjectedNode;

	if (Selection.Num() == 1)
	{
		if (DetailsView.IsValid())
		{
			DetailsView->SetObjects(Selection);
		}

		if (FoundDecorator)
		{
			GetAbortModePreview(FoundDecorator, Mode0, Mode1);
		}
		else if (FoundGraphNode_CompDecorator)
		{
			GetAbortModePreview(FoundGraphNode_CompDecorator, Mode0, Mode1);
		}
	}
	else if (DetailsView.IsValid())
	{
		if (Selection.Num() == 0)
		{
			// if nothing is selected, display the root
			UBehaviorTreeGraphNode* RootNode = nullptr;
			for (const auto& Node : MyGraph->Nodes)
			{
				RootNode = Cast<UBehaviorTreeGraphNode_Root>(Node);
				if (RootNode != nullptr)
				{
					break;
				}
			}

			DetailsView->SetObject(RootNode);
		}
		else
		{
			DetailsView->SetObject(nullptr);
		}
	}

	MyGraph->UpdateAbortHighlight(Mode0, Mode1);
}

static uint16 GetMaxAllowedRange(const class UBTDecorator* DecoratorOb)
{
	uint16 MaxRange = MAX_uint16;
	
	UBTCompositeNode* TestParent = DecoratorOb->GetParentNode();
	while (TestParent)
	{
		if (TestParent->IsA(UBTComposite_SimpleParallel::StaticClass()))
		{
			MaxRange = TestParent->GetLastExecutionIndex();
			break;
		}

		TestParent = TestParent->GetParentNode();
	}

	return MaxRange;
}

static void FillAbortPreview_LowerPriority(const class UBTDecorator* DecoratorOb, const UBTCompositeNode* DecoratorParent, FAbortDrawHelper& Mode)
{
	const uint16 MaxRange = GetMaxAllowedRange(DecoratorOb);

	Mode.AbortStart = DecoratorParent->GetChildExecutionIndex(DecoratorOb->GetChildIndex() + 1);
	Mode.AbortEnd = MaxRange;
	Mode.SearchStart = DecoratorParent->GetExecutionIndex();
	Mode.SearchEnd = MaxRange;
}

static void FillAbortPreview_Self(const class UBTDecorator* DecoratorOb, const UBTCompositeNode* DecoratorParent, FAbortDrawHelper& Mode)
{
	const uint16 MaxRange = GetMaxAllowedRange(DecoratorOb);

	Mode.AbortStart = DecoratorOb->GetExecutionIndex();
	Mode.AbortEnd = DecoratorParent->GetChildExecutionIndex(DecoratorOb->GetChildIndex() + 1) - 1;
	Mode.SearchStart = DecoratorParent->GetChildExecutionIndex(DecoratorOb->GetChildIndex() + 1);
	Mode.SearchEnd = MaxRange;
}

void FBehaviorTreeEditor::GetAbortModePreview(const class UBehaviorTreeGraphNode_CompositeDecorator* Node, struct FAbortDrawHelper& Mode0, struct FAbortDrawHelper& Mode1)
{
	Mode0.SearchStart = MAX_uint16;
	Mode0.AbortStart = MAX_uint16;
	Mode1.SearchStart = MAX_uint16;
	Mode1.AbortStart = MAX_uint16;

	TArray<UBTDecorator*> Decorators;
	TArray<FBTDecoratorLogic> Operations;
	Node->CollectDecoratorData(Decorators, Operations);

	int32 LowerPriIdx = INDEX_NONE;
	int32 SelfIdx = INDEX_NONE;
	
	for (int32 i = 0; i < Decorators.Num(); i++)
	{
		EBTFlowAbortMode::Type FlowAbort = Decorators[i]->GetParentNode() ? Decorators[i]->GetFlowAbortMode() : EBTFlowAbortMode::None;

		if (FlowAbort == EBTFlowAbortMode::LowerPriority || FlowAbort == EBTFlowAbortMode::Both)
		{
			LowerPriIdx = i;
		}
		
		if (FlowAbort == EBTFlowAbortMode::Self || FlowAbort == EBTFlowAbortMode::Both)
		{
			SelfIdx = i;
		}
	}

	if (Decorators.IsValidIndex(LowerPriIdx))
	{
		FillAbortPreview_LowerPriority(Decorators[LowerPriIdx], Decorators[LowerPriIdx]->GetParentNode(), Mode0);
		bShowDecoratorRangeLower = true;
	}

	if (Decorators.IsValidIndex(SelfIdx))
	{
		FillAbortPreview_Self(Decorators[SelfIdx], Decorators[SelfIdx]->GetParentNode(), Mode1);
		bShowDecoratorRangeSelf = true;
	}
}

void FBehaviorTreeEditor::GetAbortModePreview(const class UBTDecorator* DecoratorOb, FAbortDrawHelper& Mode0, FAbortDrawHelper& Mode1)
{
	const UBTCompositeNode* DecoratorParent = DecoratorOb ? DecoratorOb->GetParentNode() : NULL;
	EBTFlowAbortMode::Type FlowAbort = DecoratorParent ? DecoratorOb->GetFlowAbortMode() : EBTFlowAbortMode::None;

	Mode0.SearchStart = MAX_uint16;
	Mode0.AbortStart = MAX_uint16;
	Mode1.SearchStart = MAX_uint16;
	Mode1.AbortStart = MAX_uint16;

	switch (FlowAbort)
	{
		case EBTFlowAbortMode::LowerPriority:
			FillAbortPreview_LowerPriority(DecoratorOb, DecoratorParent, Mode0);
			bShowDecoratorRangeLower = true;
			break;

		case EBTFlowAbortMode::Self:
			FillAbortPreview_Self(DecoratorOb, DecoratorParent, Mode1);
			bShowDecoratorRangeSelf = true;
			break;

		case EBTFlowAbortMode::Both:
			FillAbortPreview_LowerPriority(DecoratorOb, DecoratorParent, Mode0);
			FillAbortPreview_Self(DecoratorOb, DecoratorParent, Mode1);
			bShowDecoratorRangeLower = true;
			bShowDecoratorRangeSelf = true;
			break;

		default:
			break;
	}
}

void FBehaviorTreeEditor::CreateInternalWidgets()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	FDetailsViewArgs DetailsViewArgs( false, false, true, true, false );
	DetailsViewArgs.NotifyHook = this;
	DetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );
	DetailsView->SetObject( NULL );
	DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &FBehaviorTreeEditor::IsPropertyVisible));
	DetailsView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateSP(this, &FBehaviorTreeEditor::IsPropertyEditable));
	DetailsView->OnFinishedChangingProperties().AddSP(this, &FBehaviorTreeEditor::OnFinishedChangingProperties);
}

void FBehaviorTreeEditor::ExtendMenu()
{
	struct Local
	{
		static void FillEditMenu(FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.BeginSection("EditSearch", LOCTEXT("EditMenu_SearchHeading", "Search"));
			{
				MenuBuilder.AddMenuEntry(FBTCommonCommands::Get().SearchBT);
			}
			MenuBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender);

	// Extend the Edit menu
	MenuExtender->AddMenuExtension(
		"EditHistory",
		EExtensionHook::After,
		GetToolkitCommands(),
		FMenuExtensionDelegate::CreateStatic(&Local::FillEditMenu));

	AddMenuExtender(MenuExtender);
}

void FBehaviorTreeEditor::BindCommonCommands()
{
	ToolkitCommands->MapAction(FBTCommonCommands::Get().SearchBT,
			FExecuteAction::CreateSP(this, &FBehaviorTreeEditor::SearchTree),
			FCanExecuteAction::CreateSP(this, &FBehaviorTreeEditor::CanSearchTree)
			);

	ToolkitCommands->MapAction(FBTCommonCommands::Get().NewBlackboard,
			FExecuteAction::CreateSP(this, &FBehaviorTreeEditor::CreateNewBlackboard),
			FCanExecuteAction::CreateSP(this, &FBehaviorTreeEditor::CanCreateNewBlackboard)
			);
}

void FBehaviorTreeEditor::SearchTree()
{
	TabManager->InvokeTab(FBehaviorTreeEditorTabs::SearchID);
	FindResults->FocusForUse();
}

bool FBehaviorTreeEditor::CanSearchTree() const
{
	return true;
}

TSharedRef<class SWidget> FBehaviorTreeEditor::OnGetDebuggerActorsMenu()
{
	FMenuBuilder MenuBuilder(true, NULL);

	if (Debugger.IsValid())
	{
		TArray<UBehaviorTreeComponent*> MatchingInstances;
		Debugger->GetMatchingInstances(MatchingInstances);

		// Fill the combo menu with presets of common screen resolutions
		for (int32 i = 0; i < MatchingInstances.Num(); i++)
		{
			if (MatchingInstances[i])
			{
				const FText ActorDesc = FText::FromString(Debugger->DescribeInstance(*MatchingInstances[i]));
				TWeakObjectPtr<UBehaviorTreeComponent> InstancePtr = MatchingInstances[i];

				FUIAction ItemAction(FExecuteAction::CreateSP(this, &FBehaviorTreeEditor::OnDebuggerActorSelected, InstancePtr));
				MenuBuilder.AddMenuEntry(ActorDesc, TAttribute<FText>(), FSlateIcon(), ItemAction);
			}
		}

		// Failsafe when no actor match
		if (MatchingInstances.Num() == 0)
		{
			const FText ActorDesc = LOCTEXT("NoMatchForDebug","Can't find matching actors");
			TWeakObjectPtr<UBehaviorTreeComponent> InstancePtr;

			FUIAction ItemAction(FExecuteAction::CreateSP(this, &FBehaviorTreeEditor::OnDebuggerActorSelected, InstancePtr));
			MenuBuilder.AddMenuEntry(ActorDesc, TAttribute<FText>(), FSlateIcon(), ItemAction);
		}
	}

	return MenuBuilder.MakeWidget();
}

void FBehaviorTreeEditor::OnDebuggerActorSelected(TWeakObjectPtr<UBehaviorTreeComponent> InstanceToDebug)
{
	if (Debugger.IsValid())
	{
		Debugger->OnInstanceSelectedInDropdown(InstanceToDebug.Get());
	}
}

FString FBehaviorTreeEditor::GetDebuggerActorDesc() const
{
	return Debugger.IsValid() ? Debugger->GetDebuggedInstanceDesc() : FString();
}

void FBehaviorTreeEditor::BindDebuggerToolbarCommands()
{
	const FBTDebuggerCommands& Commands = FBTDebuggerCommands::Get();
	TSharedRef<FBehaviorTreeDebugger> DebuggerOb = Debugger.ToSharedRef();

	ToolkitCommands->MapAction(
		Commands.BackOver,
		FExecuteAction::CreateSP(DebuggerOb, &FBehaviorTreeDebugger::StepBackOver),
		FCanExecuteAction::CreateSP(DebuggerOb, &FBehaviorTreeDebugger::CanStepBackOver));

	ToolkitCommands->MapAction(
		Commands.BackInto,
		FExecuteAction::CreateSP(DebuggerOb, &FBehaviorTreeDebugger::StepBackInto),
		FCanExecuteAction::CreateSP(DebuggerOb, &FBehaviorTreeDebugger::CanStepBackInto));

	ToolkitCommands->MapAction(
		Commands.ForwardInto,
		FExecuteAction::CreateSP(DebuggerOb, &FBehaviorTreeDebugger::StepForwardInto),
		FCanExecuteAction::CreateSP(DebuggerOb, &FBehaviorTreeDebugger::CanStepForwardInto));

	ToolkitCommands->MapAction(
		Commands.ForwardOver,
		FExecuteAction::CreateSP(DebuggerOb, &FBehaviorTreeDebugger::StepForwardOver),
		FCanExecuteAction::CreateSP(DebuggerOb, &FBehaviorTreeDebugger::CanStepForwardOver));

	ToolkitCommands->MapAction(
		Commands.StepOut,
		FExecuteAction::CreateSP(DebuggerOb, &FBehaviorTreeDebugger::StepOut),
		FCanExecuteAction::CreateSP(DebuggerOb, &FBehaviorTreeDebugger::CanStepOut));

	ToolkitCommands->MapAction(
		Commands.PausePlaySession,
		FExecuteAction::CreateStatic(&FBehaviorTreeDebugger::PausePlaySession),
		FCanExecuteAction::CreateStatic(&FBehaviorTreeDebugger::IsPlaySessionRunning),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateStatic(&FBehaviorTreeDebugger::IsPlaySessionRunning));

	ToolkitCommands->MapAction(
		Commands.ResumePlaySession,
		FExecuteAction::CreateStatic(&FBehaviorTreeDebugger::ResumePlaySession),
		FCanExecuteAction::CreateStatic(&FBehaviorTreeDebugger::IsPlaySessionPaused),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateStatic(&FBehaviorTreeDebugger::IsPlaySessionPaused));

	ToolkitCommands->MapAction(
		Commands.StopPlaySession,
		FExecuteAction::CreateStatic(&FBehaviorTreeDebugger::StopPlaySession));
}

bool FBehaviorTreeEditor::IsPropertyVisible( const FPropertyAndParent& PropertyAndParent) const
{
	return !PropertyAndParent.Property.HasAnyPropertyFlags(CPF_DisableEditOnInstance);
}

bool FBehaviorTreeEditor::IsPropertyEditable() const
{
	if (FBehaviorTreeDebugger::IsPIESimulating() || bForceDisablePropertyEdit)
	{
		return false;
	}

	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	return FocusedGraphEd.IsValid() && FocusedGraphEd->GetCurrentGraph() && FocusedGraphEd->GetCurrentGraph()->bEditable;
}

void FBehaviorTreeEditor::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (FocusedGraphOwner.IsValid())
	{
		FocusedGraphOwner->OnInnerGraphChanged();
	}

	// update abort range highlight when changing decorator's flow abort mode
	if (PropertyChangedEvent.Property)
	{
		if(PropertyChangedEvent.Property->GetFName() == TEXT("FlowAbortMode"))
		{
			bShowDecoratorRangeLower = false;
			bShowDecoratorRangeSelf = false;

			FGraphPanelSelectionSet CurrentSelection = GetSelectedNodes();
			if (CurrentSelection.Num() == 1)
			{
				for (FGraphPanelSelectionSet::TConstIterator It(CurrentSelection); It; ++It)
				{
					UBehaviorTreeGraphNode_Decorator* DecoratorNode = Cast<UBehaviorTreeGraphNode_Decorator>(*It);
					if (DecoratorNode)
					{
						FAbortDrawHelper Mode0, Mode1;
						GetAbortModePreview(Cast<UBTDecorator>(DecoratorNode->NodeInstance), Mode0, Mode1);

						UBehaviorTreeGraph* MyGraph = Cast<UBehaviorTreeGraph>(BehaviorTree->BTGraph);
						MyGraph->UpdateAbortHighlight(Mode0, Mode1);
					}
				}			
			}
		}
		else if(PropertyChangedEvent.Property->GetFName() == TEXT("BlackboardAsset"))
		{
			if(BlackboardView.IsValid())
			{
				BlackboardView->SetObject(GetBlackboardData());
			}
		}
	}

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == TEXT("BehaviorAsset"))
	{
		UBehaviorTreeGraph* MyGraph = Cast<UBehaviorTreeGraph>(BehaviorTree->BTGraph);
		MyGraph->UpdateInjectedNodes();
		MyGraph->UpdateAsset(UBehaviorTreeGraph::ClearDebuggerFlags);
	}
}

void FBehaviorTreeEditor::OnPackageSaved(const FString& PackageFileName, UObject* Outer)
{
	UBehaviorTreeGraph* MyGraph = BehaviorTree ? Cast<UBehaviorTreeGraph>(BehaviorTree->BTGraph) : NULL;
	if (MyGraph)
	{
		const bool bUpdated = MyGraph->UpdateInjectedNodes();
		if (bUpdated)
		{
			MyGraph->UpdateAsset(UBehaviorTreeGraph::ClearDebuggerFlags);
		}
	}
}

void FBehaviorTreeEditor::OnClassListUpdated()
{
	UBehaviorTreeGraph* MyGraph = BehaviorTree ? Cast<UBehaviorTreeGraph>(BehaviorTree->BTGraph) : NULL;
	if (MyGraph)
	{
		const bool bUpdated = MyGraph->UpdateUnknownNodeClasses();
		if (bUpdated)
		{
			FGraphPanelSelectionSet CurrentSelection = GetSelectedNodes();
			OnSelectedNodesChanged(CurrentSelection);

			MyGraph->UpdateAsset(UBehaviorTreeGraph::ClearDebuggerFlags);
		}
	}
}

FGraphPanelSelectionSet FBehaviorTreeEditor::GetSelectedNodes() const
{
	FGraphPanelSelectionSet CurrentSelection;
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		CurrentSelection = FocusedGraphEd->GetSelectedNodes();
	}
	return CurrentSelection;
}

void FBehaviorTreeEditor::SelectAllNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = FocusedGraphEdPtr.Pin();
	if (CurrentGraphEditor.IsValid())
	{
		CurrentGraphEditor->SelectAllNodes();
	}
}

bool FBehaviorTreeEditor::CanSelectAllNodes() const
{
	return true;
}

void FBehaviorTreeEditor::DeleteSelectedNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = FocusedGraphEdPtr.Pin();
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

bool FBehaviorTreeEditor::CanDeleteNodes() const
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

void FBehaviorTreeEditor::DeleteSelectedDuplicatableNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = FocusedGraphEdPtr.Pin();
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

void FBehaviorTreeEditor::CutSelectedNodes()
{
	CopySelectedNodes();
	DeleteSelectedDuplicatableNodes();
}

bool FBehaviorTreeEditor::CanCutNodes() const
{
	return CanCopyNodes() && CanDeleteNodes();
}

void FBehaviorTreeEditor::CopySelectedNodes()
{
	// Export the selected nodes and place the text on the clipboard
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	TArray<UBehaviorTreeGraphNode*> SubNodes;

	FString ExportedText;

	int32 CopySubNodeIndex = 0;
	for (FGraphPanelSelectionSet::TIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UBehaviorTreeGraphNode* Node = Cast<UBehaviorTreeGraphNode>(*SelectedIter);
		
		// skip all manually selected subnodes
		if (Node == NULL || Node->IsSubNode() || Node->bInjectedNode)
		{
			SelectedIter.RemoveCurrent();
			continue;
		}

		Node->PrepareForCopying();
		Node->CopySubNodeIndex = CopySubNodeIndex;

		// append all subnodes for selection
		for (int32 i = 0; i < Node->Decorators.Num(); i++)
		{
			if (!Node->Decorators[i]->bInjectedNode)
			{
				Node->Decorators[i]->CopySubNodeIndex = CopySubNodeIndex;
				SubNodes.Add(Node->Decorators[i]);
			}
		}

		for (int32 i = 0; i < Node->Services.Num(); i++)
		{
			if (!Node->Services[i]->bInjectedNode)
			{
				Node->Services[i]->CopySubNodeIndex = CopySubNodeIndex;
				SubNodes.Add(Node->Services[i]);
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
		UBehaviorTreeGraphNode* Node = Cast<UBehaviorTreeGraphNode>(*SelectedIter);
		Node->PostCopyNode();
	}
}

bool FBehaviorTreeEditor::CanCopyNodes() const
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

void FBehaviorTreeEditor::PasteNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = FocusedGraphEdPtr.Pin();
	if (CurrentGraphEditor.IsValid())
	{
		PasteNodesHere(CurrentGraphEditor->GetPasteLocation());
	}
}

void FBehaviorTreeEditor::PasteNodesHere(const FVector2D& Location)
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = FocusedGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}

	// Undo/Redo support
	const FScopedTransaction Transaction( FGenericCommands::Get().Paste->GetDescription() );
	UBehaviorTreeGraph* BTGraph = Cast<UBehaviorTreeGraph>(CurrentGraphEditor->GetCurrentGraph());
	BTGraph->Modify();
	BTGraph->LockUpdates();

	// Clear the selection set (newly pasted stuff will be selected)
	CurrentGraphEditor->ClearSelectionSet();

	// Grab the text to paste from the clipboard.
	FString TextToImport;
	FPlatformMisc::ClipboardPaste(TextToImport);

	// Import the nodes
	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(BTGraph, TextToImport, /*out*/ PastedNodes);

	//Average position of nodes so we can move them while still maintaining relative distances to each other
	FVector2D AvgNodePosition(0.0f,0.0f);

	float NumTopLevelNodes = 0.0f;
	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UBehaviorTreeGraphNode* BTNode = Cast<UBehaviorTreeGraphNode>(*It);
		if (BTNode && !BTNode->IsSubNode())
		{
			AvgNodePosition.X += BTNode->NodePosX;
			AvgNodePosition.Y += BTNode->NodePosY;
			NumTopLevelNodes += 1.0f;
		}
	}

	if (NumTopLevelNodes > 0.0f)
	{
		float InvNumNodes = 1.0f/NumTopLevelNodes;
		AvgNodePosition.X *= InvNumNodes;
		AvgNodePosition.Y *= InvNumNodes;
	}

	TMap<int32, UBehaviorTreeGraphNode*> ParentMap;
	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UBehaviorTreeGraphNode* BTNode = Cast<UBehaviorTreeGraphNode>(*It);
		if (BTNode && !BTNode->IsSubNode())
		{
			// Select the newly pasted stuff
			CurrentGraphEditor->SetNodeSelection(BTNode, true);

			BTNode->NodePosX = (BTNode->NodePosX - AvgNodePosition.X) + Location.X ;
			BTNode->NodePosY = (BTNode->NodePosY - AvgNodePosition.Y) + Location.Y ;

			BTNode->SnapToGrid(16);

			// Give new node a different Guid from the old one
			BTNode->CreateNewGuid();

			BTNode->Decorators.Reset();
			BTNode->Services.Reset();

			ParentMap.Add(BTNode->CopySubNodeIndex, BTNode);
		}
	}

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UBehaviorTreeGraphNode* BTNode = Cast<UBehaviorTreeGraphNode>(*It);
		if (BTNode && BTNode->IsSubNode())
		{
			BTNode->NodePosX = 0;
			BTNode->NodePosY = 0;

			// Give new node a different Guid from the old one
			BTNode->CreateNewGuid();

			// remove subnode from graph, it will be referenced from parent node
			BTNode->DestroyNode();

			BTNode->ParentNode = ParentMap.FindRef(BTNode->CopySubNodeIndex);
			if (BTNode->ParentNode)
			{
				const bool bIsService = BTNode->IsA(UBehaviorTreeGraphNode_Service::StaticClass());
				if (bIsService)
				{
					BTNode->ParentNode->Services.Add(BTNode);
				}
				else
				{
					BTNode->ParentNode->Decorators.Add(BTNode);
				}
			}
		}
	}

	BTGraph->UnlockUpdates();

	// Update UI
	CurrentGraphEditor->NotifyGraphChanged();

	BehaviorTree->PostEditChange();
	BehaviorTree->MarkPackageDirty();
}

bool FBehaviorTreeEditor::CanPasteNodes() const
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = FocusedGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformMisc::ClipboardPaste(ClipboardContent);

	return FEdGraphUtilities::CanImportNodesFromText(CurrentGraphEditor->GetCurrentGraph(), ClipboardContent);
}

void FBehaviorTreeEditor::DuplicateNodes()
{
	CopySelectedNodes();
	PasteNodes();
}

bool FBehaviorTreeEditor::CanDuplicateNodes() const
{
	return CanCopyNodes();
}

void FBehaviorTreeEditor::OnNodeDoubleClicked(class UEdGraphNode* Node)
{
	UBehaviorTreeGraphNode* MyNode = Cast<UBehaviorTreeGraphNode>(Node);
	if (MyNode && MyNode->bInjectedNode)
	{
		UBTTask_RunBehavior* SubtreeTask = MyNode->ParentNode ? Cast<UBTTask_RunBehavior>(MyNode->ParentNode->NodeInstance) : NULL;
		if (SubtreeTask && SubtreeTask->GetSubtreeAsset())
		{
			FAssetEditorManager::Get().OpenEditorForAsset(SubtreeTask->GetSubtreeAsset());

			IBehaviorTreeEditor* ChildNodeEditor = static_cast<IBehaviorTreeEditor*>(FAssetEditorManager::Get().FindEditorForAsset(SubtreeTask->GetSubtreeAsset(), true));
			if (ChildNodeEditor)
			{
				ChildNodeEditor->InitializeDebuggerState(Debugger.Get());
				
				int32 FirstInjectedIdx = INDEX_NONE;
				for (int32 Idx = 0; Idx < MyNode->ParentNode->Decorators.Num(); Idx++)
				{
					if (MyNode->ParentNode->Decorators[Idx]->bInjectedNode)
					{
						FirstInjectedIdx = Idx;
						break;
					}
				}

				if (FirstInjectedIdx != INDEX_NONE)
				{
					const int32 NodeIdx = MyNode->ParentNode->Decorators.IndexOfByKey(MyNode) - FirstInjectedIdx;
					UEdGraphNode* OtherNode = ChildNodeEditor->FindInjectedNode(NodeIdx);
					if (OtherNode)
					{
						ChildNodeEditor->DoubleClickNode(OtherNode);
					}
				}
			}
		}
	}
	else if (UBehaviorTreeGraphNode_CompositeDecorator* Decorator = Cast<UBehaviorTreeGraphNode_CompositeDecorator>(Node))
	{
		if (Decorator->GetBoundGraph())
		{
			TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(const_cast<UEdGraph*>(Decorator->GetBoundGraph()));
			
			TArray< TSharedPtr<SDockTab> > MatchingTabs;
			DocumentManager->FindMatchingTabs(Payload, MatchingTabs);
			if (MatchingTabs.Num())
			{
				DocumentManager->CloseTab(Payload);
				DocumentManager->OpenDocument(Payload, FDocumentTracker::RestorePreviousDocument);
			}
			else
			{
				DocumentManager->OpenDocument(Payload, FDocumentTracker::OpenNewDocument);
			}
		}
	}
	else if (UBehaviorTreeGraphNode_SubtreeTask* Task = Cast<UBehaviorTreeGraphNode_SubtreeTask>(Node))
	{
		if (UBTTask_RunBehavior* RunTask = Cast<UBTTask_RunBehavior>(Task->NodeInstance))
		{
			if (RunTask->GetSubtreeAsset())
			{
				FAssetEditorManager::Get().OpenEditorForAsset(RunTask->GetSubtreeAsset());

				IBehaviorTreeEditor* ChildNodeEditor = static_cast<IBehaviorTreeEditor*>(FAssetEditorManager::Get().FindEditorForAsset(RunTask->GetSubtreeAsset(), true));
				if (ChildNodeEditor)
				{
					ChildNodeEditor->InitializeDebuggerState(Debugger.Get());
				}
			}
		}
	}

	if (MyNode && MyNode->NodeInstance &&
		MyNode->NodeInstance->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		UClass* NodeClass = MyNode->NodeInstance->GetClass();
		UPackage* Pkg = NodeClass->GetOuterUPackage();
		FString ClassName = NodeClass->GetName().LeftChop(2);
		UBlueprint* BlueprintOb = FindObject<UBlueprint>(Pkg, *ClassName);
		if(BlueprintOb)
		{
			FAssetEditorManager::Get().OpenEditorForAsset(BlueprintOb);
		}
	}
}

void FBehaviorTreeEditor::OnAddInputPin()
{
	FGraphPanelSelectionSet CurrentSelection;
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		CurrentSelection = FocusedGraphEd->GetSelectedNodes();
	}

	// Iterate over all nodes, and add the pin
	for (FGraphPanelSelectionSet::TConstIterator It(CurrentSelection); It; ++It)
	{
		UBehaviorTreeDecoratorGraphNode_Logic* LogicNode = Cast<UBehaviorTreeDecoratorGraphNode_Logic>(*It);
		if (LogicNode)
		{
			const FScopedTransaction Transaction( LOCTEXT("AddInputPin", "Add Input Pin") );

			LogicNode->Modify();
			LogicNode->AddInputPin();

			const UEdGraphSchema* Schema = LogicNode->GetSchema();
			Schema->ReconstructNode(*LogicNode);
		}
	}

	// Refresh the current graph, so the pins can be updated
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->NotifyGraphChanged();
	}
}

bool FBehaviorTreeEditor::CanAddInputPin() const
{
	FGraphPanelSelectionSet CurrentSelection = GetSelectedNodes();
	bool bReturnValue = false;

	// Iterate over all nodes, and make sure all execution sequence nodes will always have at least 2 outs
	for (FGraphPanelSelectionSet::TConstIterator It(CurrentSelection); It; ++It)
	{
		UBehaviorTreeDecoratorGraphNode_Logic* LogicNode = Cast<UBehaviorTreeDecoratorGraphNode_Logic>(*It);
		if (LogicNode)
		{
			bReturnValue = LogicNode->CanAddPins();
			break;
		}
	}

	return bReturnValue;
};

void FBehaviorTreeEditor::OnRemoveInputPin()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		const FScopedTransaction Transaction( LOCTEXT("RemoveInputPin", "Remove Input Pin") );

		UEdGraphPin* SelectedPin = FocusedGraphEd->GetGraphPinForMenu();
		UEdGraphNode* OwningNode = SelectedPin->GetOwningNode();

		OwningNode->Modify();
		SelectedPin->Modify();

		if (UBehaviorTreeDecoratorGraphNode_Logic* LogicNode = Cast<UBehaviorTreeDecoratorGraphNode_Logic>(OwningNode))
		{
			LogicNode->RemoveInputPin(SelectedPin);
		}

		// Update the graph so that the node will be refreshed
		FocusedGraphEd->NotifyGraphChanged();
	}
}

bool FBehaviorTreeEditor::CanRemoveInputPin() const
{
	FGraphPanelSelectionSet CurrentSelection = GetSelectedNodes();
	bool bReturnValue = false;

	// Iterate over all nodes, and make sure all execution sequence nodes will always have at least 2 outs
	for (FGraphPanelSelectionSet::TConstIterator It(CurrentSelection); It; ++It)
	{
		UBehaviorTreeDecoratorGraphNode_Logic* LogicNode = Cast<UBehaviorTreeDecoratorGraphNode_Logic>(*It);
		if (LogicNode)
		{
			bReturnValue = LogicNode->CanRemovePins();
			break;
		}
	}

	return bReturnValue;
};

void FBehaviorTreeEditor::OnGraphEditorFocused(const TSharedRef<SGraphEditor>& InGraphEditor)
{
	FocusedGraphEdPtr = InGraphEditor;
	FocusedGraphOwner = Cast<UBehaviorTreeGraphNode_CompositeDecorator>(InGraphEditor->GetCurrentGraph()->GetOuter());

	FGraphPanelSelectionSet CurrentSelection;
	CurrentSelection = InGraphEditor->GetSelectedNodes();
	OnSelectedNodesChanged(CurrentSelection);
}

void FBehaviorTreeEditor::SaveAsset_Execute()
{
	if (BehaviorTree)
	{
		UBehaviorTreeGraph* BTGraph = Cast<UBehaviorTreeGraph>(BehaviorTree->BTGraph);
		if (BTGraph)
		{
			BTGraph->UpdateAsset(UBehaviorTreeGraph::SkipDebuggerFlags);
		}
	}
	// save it
	IBehaviorTreeEditor::SaveAsset_Execute();
}

void FBehaviorTreeEditor::OnEnableBreakpoint()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UBehaviorTreeGraphNode* SelectedNode = Cast<UBehaviorTreeGraphNode>(*NodeIt);
		if (SelectedNode && SelectedNode->bHasBreakpoint && !SelectedNode->bIsBreakpointEnabled)
		{
			SelectedNode->bIsBreakpointEnabled = true;
			Debugger->OnBreakpointAdded(SelectedNode);
		}
	}
}

bool FBehaviorTreeEditor::CanEnableBreakpoint() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UBehaviorTreeGraphNode* SelectedNode = Cast<UBehaviorTreeGraphNode>(*NodeIt);
		if (SelectedNode && SelectedNode->bHasBreakpoint && !SelectedNode->bIsBreakpointEnabled)
		{
			return true;
		}
	}

	return false;
}

void FBehaviorTreeEditor::OnDisableBreakpoint()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UBehaviorTreeGraphNode* SelectedNode = Cast<UBehaviorTreeGraphNode>(*NodeIt);
		if (SelectedNode && SelectedNode->bHasBreakpoint && SelectedNode->bIsBreakpointEnabled)
		{
			SelectedNode->bIsBreakpointEnabled = false;
			Debugger->OnBreakpointRemoved(SelectedNode);
		}
	}
}

bool FBehaviorTreeEditor::CanDisableBreakpoint() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UBehaviorTreeGraphNode* SelectedNode = Cast<UBehaviorTreeGraphNode>(*NodeIt);
		if (SelectedNode && SelectedNode->bHasBreakpoint && SelectedNode->bIsBreakpointEnabled)
		{
			return true;
		}
	}

	return false;
}

void FBehaviorTreeEditor::OnAddBreakpoint()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UBehaviorTreeGraphNode* SelectedNode = Cast<UBehaviorTreeGraphNode>(*NodeIt);
		if (SelectedNode && SelectedNode->CanPlaceBreakpoints() && !SelectedNode->bHasBreakpoint)
		{
			SelectedNode->bHasBreakpoint = true;
			SelectedNode->bIsBreakpointEnabled = true;
			Debugger->OnBreakpointAdded(SelectedNode);
		}
	}
}

bool FBehaviorTreeEditor::CanAddBreakpoint() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UBehaviorTreeGraphNode* SelectedNode = Cast<UBehaviorTreeGraphNode>(*NodeIt);
		if (SelectedNode && SelectedNode->CanPlaceBreakpoints() && !SelectedNode->bHasBreakpoint)
		{
			return true;
		}
	}

	return false;
}

void FBehaviorTreeEditor::OnRemoveBreakpoint()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UBehaviorTreeGraphNode* SelectedNode = Cast<UBehaviorTreeGraphNode>(*NodeIt);
		if (SelectedNode && SelectedNode->CanPlaceBreakpoints() && SelectedNode->bHasBreakpoint)
		{
			SelectedNode->bHasBreakpoint = false;
			SelectedNode->bIsBreakpointEnabled = false;
			Debugger->OnBreakpointRemoved(SelectedNode);
		}
	}
}

bool FBehaviorTreeEditor::CanRemoveBreakpoint() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UBehaviorTreeGraphNode* SelectedNode = Cast<UBehaviorTreeGraphNode>(*NodeIt);
		if (SelectedNode && SelectedNode->CanPlaceBreakpoints() && SelectedNode->bHasBreakpoint)
		{
			return true;
		}
	}

	return false;
}

void FBehaviorTreeEditor::OnToggleBreakpoint()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UBehaviorTreeGraphNode* SelectedNode = Cast<UBehaviorTreeGraphNode>(*NodeIt);
		if (SelectedNode && SelectedNode->CanPlaceBreakpoints())
		{
			if (SelectedNode->bHasBreakpoint)
			{
				SelectedNode->bHasBreakpoint = false;
				SelectedNode->bIsBreakpointEnabled = false;
				Debugger->OnBreakpointRemoved(SelectedNode);
			}
			else
			{
				SelectedNode->bHasBreakpoint = true;
				SelectedNode->bIsBreakpointEnabled = true;
				Debugger->OnBreakpointAdded(SelectedNode);
			}
		}
	}
}

bool FBehaviorTreeEditor::CanToggleBreakpoint() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UBehaviorTreeGraphNode* SelectedNode = Cast<UBehaviorTreeGraphNode>(*NodeIt);
		if (SelectedNode && SelectedNode->CanPlaceBreakpoints())
		{
			return true;
		}
	}

	return false;
}

void FBehaviorTreeEditor::JumpToNode(const UEdGraphNode* Node)
{
	TSharedPtr<SDockTab> ActiveTab = DocumentManager->GetActiveTab();
	if (ActiveTab.IsValid())
	{
		TSharedPtr<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(ActiveTab->GetContent());
		if (GraphEditor.IsValid())
		{
			GraphEditor->JumpToNode(Node, false);
		}
	}
}

TWeakPtr<SGraphEditor> FBehaviorTreeEditor::GetFocusedGraphPtr() const
{
	return FocusedGraphEdPtr;
}

bool FBehaviorTreeEditor::CanAccessBlackboardMode() const
{
	return GetBlackboardData() != nullptr;
}

bool FBehaviorTreeEditor::CanAccessBehaviorTreeMode() const
{
	return BehaviorTree != nullptr;
}

FText FBehaviorTreeEditor::GetLocalizedMode(FName InMode)
{
	static TMap< FName, FText > LocModes;

	if (LocModes.Num() == 0)
	{
		LocModes.Add( BehaviorTreeMode, LOCTEXT("BehaviorTreeMode", "Behavior Tree") );
		LocModes.Add( BlackboardMode, LOCTEXT("BlackboardMode", "Blackboard") );
	}

	check( InMode != NAME_None );
	const FText* OutDesc = LocModes.Find( InMode );
	check( OutDesc );
	return *OutDesc;
}

UEdGraphNode* FBehaviorTreeEditor::FindInjectedNode(int32 Index) const
{
	UBehaviorTreeGraph* BTGraph = Cast<UBehaviorTreeGraph>(BehaviorTree->BTGraph);
	return BTGraph ? BTGraph->FindInjectedNode(Index) : NULL;
}

void FBehaviorTreeEditor::DoubleClickNode(class UEdGraphNode* Node)
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = FocusedGraphEdPtr.Pin();
	if (CurrentGraphEditor.IsValid())
	{
		CurrentGraphEditor->ClearSelectionSet();
		CurrentGraphEditor->SetNodeSelection(Node, true);
	}

	JumpToNode(Node);
	OnNodeDoubleClicked(Node);
}

void FBehaviorTreeEditor::FocusWindow(UObject* ObjectToFocusOn)
{
	if(ObjectToFocusOn == BehaviorTree)
	{
		SetCurrentMode(BehaviorTreeMode);
	}
	else if(ObjectToFocusOn == GetBlackboardData())
	{
		SetCurrentMode(BlackboardMode);
	}

	FWorkflowCentricApplication::FocusWindow(ObjectToFocusOn);
}

void FBehaviorTreeEditor::InitializeDebuggerState(class FBehaviorTreeDebugger* ParentDebugger) const
{
	if (Debugger.IsValid())
	{
		Debugger->InitializeFromParent(ParentDebugger);
	}
}

void FBehaviorTreeEditor::DebuggerSwitchAsset(UBehaviorTree* NewAsset)
{
	if (NewAsset)
	{
		FAssetEditorManager::Get().OpenEditorForAsset(NewAsset);

		IBehaviorTreeEditor* ChildNodeEditor = static_cast<IBehaviorTreeEditor*>(FAssetEditorManager::Get().FindEditorForAsset(NewAsset, true));
		if (ChildNodeEditor)
		{
			ChildNodeEditor->InitializeDebuggerState(Debugger.Get());
		}
	}
}

void FBehaviorTreeEditor::DebuggerUpdateGraph()
{
	UBehaviorTreeGraph* BTGraph = BehaviorTree ? Cast<UBehaviorTreeGraph>(BehaviorTree->BTGraph) : NULL;
	if (BTGraph)
	{
		BTGraph->RebuildExecutionOrder();
	}
}

FText FBehaviorTreeEditor::GetToolkitName() const
{
	const UObject* EditingObject = GetCurrentMode() == BehaviorTreeMode ? (UObject*)BehaviorTree : (UObject*)GetBlackboardData();
	if(EditingObject != nullptr)
	{
		return FAssetEditorToolkit::GetDescriptionForObject(EditingObject);
	}

	return FText();
}

UBehaviorTree* FBehaviorTreeEditor::GetBehaviorTree() const 
{
	return BehaviorTree; 
}

UBlackboardData* FBehaviorTreeEditor::GetBlackboardData() const 
{
	return BehaviorTree == nullptr ? BlackboardData : BehaviorTree->BlackboardAsset; 
}

void FBehaviorTreeEditor::RefreshDebugger()
{
	Debugger->Refresh();
}

bool FBehaviorTreeEditor::CanCreateNewTask() const
{
	return !IsDebuggerReady();
}

bool FBehaviorTreeEditor::CanCreateNewDecorator() const
{
	return !IsDebuggerReady();
}

bool FBehaviorTreeEditor::CanCreateNewService() const
{
	return !IsDebuggerReady();
}

template <typename Type>
class FNewNodeClassFilter : public IClassViewerFilter
{
public:
	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) override
	{
		if(InClass != nullptr)
		{
			return InClass->IsChildOf(Type::StaticClass());
		}
		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return InUnloadedClassData->IsChildOf(Type::StaticClass());
	}
};


TSharedRef<SWidget> FBehaviorTreeEditor::HandleCreateNewTaskMenu() const
{
	FClassViewerInitializationOptions Options;
	Options.bShowUnloadedBlueprints = true;
	Options.ClassFilter = MakeShareable( new FNewNodeClassFilter<UBTTask_BlueprintBase> );

	FOnClassPicked OnPicked( FOnClassPicked::CreateSP( this, &FBehaviorTreeEditor::HandleNewNodeClassPicked ) );

	return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked);
}

TSharedRef<SWidget> FBehaviorTreeEditor::HandleCreateNewDecoratorMenu() const
{
	FClassViewerInitializationOptions Options;
	Options.bShowUnloadedBlueprints = true;
	Options.ClassFilter = MakeShareable( new FNewNodeClassFilter<UBTDecorator_BlueprintBase> );

	FOnClassPicked OnPicked( FOnClassPicked::CreateSP( this, &FBehaviorTreeEditor::HandleNewNodeClassPicked ) );

	return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked);
}

TSharedRef<SWidget> FBehaviorTreeEditor::HandleCreateNewServiceMenu() const
{
	FClassViewerInitializationOptions Options;
	Options.bShowUnloadedBlueprints = true;
	Options.ClassFilter = MakeShareable( new FNewNodeClassFilter<UBTService_BlueprintBase> );

	FOnClassPicked OnPicked( FOnClassPicked::CreateSP( this, &FBehaviorTreeEditor::HandleNewNodeClassPicked ) );

	return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked);
}

void FBehaviorTreeEditor::HandleNewNodeClassPicked(UClass* InClass) const
{
	if(BehaviorTree != nullptr)
	{
		FString ClassName = InClass->GetName();
		ClassName.RemoveFromEnd(TEXT("_C"));

		FString PathName = BehaviorTree->GetOutermost()->GetPathName();
		PathName = FPaths::GetPath(PathName);
		PathName /= ClassName;

		FString Name;
		FString PackageName;
		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateUniqueAssetName(PathName, TEXT("_New"), PackageName, Name);

		UPackage* Package = CreatePackage(NULL, *PackageName);
		if (ensure(Package))
		{
			// Create and init a new Blueprint
			if (UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(InClass, Package, FName(*Name), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass()))
			{
				FAssetEditorManager::Get().OpenEditorForAsset(NewBP);

				// Notify the asset registry
				FAssetRegistryModule::AssetCreated(NewBP);

				// Mark the package dirty...
				Package->MarkPackageDirty();
			}
		}
	}
}

void FBehaviorTreeEditor::CreateNewTask() const
{
	HandleNewNodeClassPicked(UBTTask_BlueprintBase::StaticClass());
}

bool FBehaviorTreeEditor::IsNewTaskButtonVisible() const
{
	return !FClassBrowseHelper::IsMoreThanOneTaskClassAvailable();
}

bool FBehaviorTreeEditor::IsNewTaskComboVisible() const
{
	return FClassBrowseHelper::IsMoreThanOneTaskClassAvailable();
}

void FBehaviorTreeEditor::CreateNewDecorator() const
{
	HandleNewNodeClassPicked(UBTDecorator_BlueprintBase::StaticClass());
}

bool FBehaviorTreeEditor::IsNewDecoratorButtonVisible() const
{
	return !FClassBrowseHelper::IsMoreThanOneDecoratorClassAvailable();
}

bool FBehaviorTreeEditor::IsNewDecoratorComboVisible() const
{
	return FClassBrowseHelper::IsMoreThanOneDecoratorClassAvailable();
}

void FBehaviorTreeEditor::CreateNewService() const
{
	HandleNewNodeClassPicked(UBTService_BlueprintBase::StaticClass());
}

bool FBehaviorTreeEditor::IsNewServiceButtonVisible() const
{
	return !FClassBrowseHelper::IsMoreThanOneServiceClassAvailable();
}

bool FBehaviorTreeEditor::IsNewServiceComboVisible() const
{
	return FClassBrowseHelper::IsMoreThanOneServiceClassAvailable();
}

void FBehaviorTreeEditor::CreateNewBlackboard()
{
	FString PathName = BehaviorTree->GetOutermost()->GetPathName();
	PathName = FPaths::GetPath(PathName);
	const FString PathNameWithFilename = PathName / LOCTEXT("NewBlackboardName", "NewBlackboardData").ToString();

	FString Name;
	FString PackageName;
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(PathNameWithFilename, TEXT(""), PackageName, Name);

	UDataAssetFactory* DataAssetFactory = ConstructObject<UDataAssetFactory>(UDataAssetFactory::StaticClass());
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().CreateNewAsset(Name, PathName, UBlackboardData::StaticClass(), DataAssetFactory);
}

bool FBehaviorTreeEditor::CanCreateNewBlackboard() const
{
	return !IsDebuggerReady();
}

#undef LOCTEXT_NAMESPACE
