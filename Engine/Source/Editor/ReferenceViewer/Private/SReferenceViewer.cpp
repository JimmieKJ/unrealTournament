// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewerPrivatePCH.h"
#include "GraphEditor.h"
#include "AssetRegistryModule.h"
#include "CollectionManagerModule.h"
#include "AssetEditorManager.h"
#include "AssetThumbnail.h"
#include "ReferenceViewerActions.h"
#include "EditorWidgets.h"
#include "GlobalEditorCommonCommands.h"
#include "ISizeMapModule.h"

#include "Editor/UnrealEd/Public/ObjectTools.h"
#include "Engine/Selection.h"

#define LOCTEXT_NAMESPACE "ReferenceViewer"

SReferenceViewer::~SReferenceViewer()
{
	if (!GExitPurge)
	{
		if ( ensure(GraphObj) )
		{
			GraphObj->RemoveFromRoot();
		}		
	}
}

void SReferenceViewer::Construct( const FArguments& InArgs )
{
	// Create an action list and register commands
	RegisterActions();

	// Set up the history manager
	HistoryManager.SetOnApplyHistoryData(FOnApplyHistoryData::CreateSP(this, &SReferenceViewer::OnApplyHistoryData));
	HistoryManager.SetOnUpdateHistoryData(FOnUpdateHistoryData::CreateSP(this, &SReferenceViewer::OnUpdateHistoryData));

	// Create the graph
	GraphObj = NewObject<UEdGraph_ReferenceViewer>();
	GraphObj->Schema = UReferenceViewerSchema::StaticClass();
	GraphObj->AddToRoot();

	SGraphEditor::FGraphEditorEvents GraphEvents;
	GraphEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &SReferenceViewer::OnNodeDoubleClicked);
	GraphEvents.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateSP(this, &SReferenceViewer::OnCreateGraphActionMenu);

	// Create the graph editor
	GraphEditorPtr = SNew(SGraphEditor)
		.AdditionalCommands(ReferenceViewerActions)
		.GraphToEdit(GraphObj)
		.GraphEvents(GraphEvents)
		.OnNavigateHistoryBack(FSimpleDelegate::CreateSP(this, &SReferenceViewer::GraphNavigateHistoryBack))
		.OnNavigateHistoryForward(FSimpleDelegate::CreateSP(this, &SReferenceViewer::GraphNavigateHistoryForward));

	FEditorWidgetsModule& EditorWidgetsModule = FModuleManager::LoadModuleChecked<FEditorWidgetsModule>("EditorWidgets");
	TSharedRef<SWidget> AssetDiscoveryIndicator = EditorWidgetsModule.CreateAssetDiscoveryIndicator(EAssetDiscoveryIndicatorScaleMode::Scale_None, FMargin(16, 8), false);

	static const FName DefaultForegroundName("DefaultForeground");

	ChildSlot
	[
		SNew(SVerticalBox)

		// Path and history
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding( 0, 0, 0, 4 )
		[
			SNew(SHorizontalBox)

			// History Back Button
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(1,0)
			[
				SNew(SButton)
				.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
				.ForegroundColor( FEditorStyle::GetSlateColor(DefaultForegroundName) )
				.ToolTipText( this, &SReferenceViewer::GetHistoryBackTooltip )
				.ContentPadding( 0 )
				.OnClicked(this, &SReferenceViewer::BackClicked)
				.IsEnabled(this, &SReferenceViewer::IsBackEnabled)
				[
					SNew(SImage) .Image(FEditorStyle::GetBrush("ContentBrowser.HistoryBack"))
				]
			]

			// History Forward Button
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1,0,3,0)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
				.ForegroundColor( FEditorStyle::GetSlateColor(DefaultForegroundName) )
				.ToolTipText( this, &SReferenceViewer::GetHistoryForwardTooltip )
				.ContentPadding( 0 )
				.OnClicked(this, &SReferenceViewer::ForwardClicked)
				.IsEnabled(this, &SReferenceViewer::IsForwardEnabled)
				[
					SNew(SImage) .Image(FEditorStyle::GetBrush("ContentBrowser.HistoryForward"))
				]
			]

			// Path
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.FillWidth(1.f)
			[
				SNew(SBorder)
				.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
				[
					SNew(SEditableTextBox)
					.Text(this, &SReferenceViewer::GetAddressBarText)
					.OnTextCommitted(this, &SReferenceViewer::OnAddressBarTextCommitted)
					.OnTextChanged(this, &SReferenceViewer::OnAddressBarTextChanged)
					.SelectAllTextWhenFocused(true)
					.SelectAllTextOnCommit(true)
					.Style(FEditorStyle::Get(), "ReferenceViewer.PathText")
				]
			]
		]

		// Graph
		+SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SOverlay)

			+SOverlay::Slot()
			[
				GraphEditorPtr.ToSharedRef()
			]

			+SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Padding(8)
			[
				SNew(SBorder)
				.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
				[
					SNew(SVerticalBox)

					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(2.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SearchDepthLabelText", "Search Depth Limit"))
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(2.f)
						[
							SNew(SCheckBox)
							.OnCheckStateChanged( this, &SReferenceViewer::OnSearchDepthEnabledChanged )
							.IsChecked( this, &SReferenceViewer::IsSearchDepthEnabledChecked )
						]
					
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(2.f)
						[
							SNew(SBox)
							.WidthOverride(100)
							[
								SNew(SSpinBox<int32>)
								.Value(this, &SReferenceViewer::GetSearchDepthCount)
								.OnValueChanged(this, &SReferenceViewer::OnSearchDepthCommitted)
								.MinValue(1)
								.MaxValue(12)
							]
						]
					]

					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(2.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SearchBreadthLabelText", "Search Breadth Limit"))
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(2.f)
						[
							SNew(SCheckBox)
							.OnCheckStateChanged( this, &SReferenceViewer::OnSearchBreadthEnabledChanged )
							.IsChecked( this, &SReferenceViewer::IsSearchBreadthEnabledChecked )
						]
					
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(2.f)
						[
							SNew(SBox)
							.WidthOverride(100)
							[
								SNew(SSpinBox<int32>)
								.Value(this, &SReferenceViewer::GetSearchBreadthCount)
								.OnValueChanged(this, &SReferenceViewer::OnSearchBreadthCommitted)
								.MinValue(1)
								.MaxValue(50)
							]
						]
					]

					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(2.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ShowHideSoftReferences", "Show Soft References"))
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(2.f)
						[
							SNew(SCheckBox)
							.OnCheckStateChanged( this, &SReferenceViewer::OnShowSoftReferencesChanged )
							.IsChecked( this, &SReferenceViewer::IsShowSoftReferencesChecked )
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(2.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ShowHideSoftDependencies", "Show Soft Dependencies"))
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(2.f)
						[
							SNew(SCheckBox)
							.OnCheckStateChanged(this, &SReferenceViewer::OnShowSoftDependenciesChanged)
							.IsChecked(this, &SReferenceViewer::IsShowSoftDependenciesChecked)
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(2.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ShowHideHardReferences", "Show Hard References"))
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(2.f)
						[
							SNew(SCheckBox)
							.OnCheckStateChanged(this, &SReferenceViewer::OnShowHardReferencesChanged)
							.IsChecked(this, &SReferenceViewer::IsShowHardReferencesChecked)
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(2.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ShowHideHardDependencies", "Show Hard Dependencies"))
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(2.f)
						[
							SNew(SCheckBox)
							.OnCheckStateChanged(this, &SReferenceViewer::OnShowHardDependenciesChanged)
							.IsChecked(this, &SReferenceViewer::IsShowHardDependenciesChecked)
						]
					]
				]
			]

			+SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(FMargin(24, 0, 24, 0))
			[
				AssetDiscoveryIndicator
			]
		]
	];
}

void SReferenceViewer::SetGraphRootPackageNames(const TArray<FName>& NewGraphRootPackageNames)
{
	GraphObj->SetGraphRoot(NewGraphRootPackageNames);
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	if ( AssetRegistryModule.Get().IsLoadingAssets() )
	{
		// We are still discovering assets, listen for the completion delegate before building the graph
		if (!AssetRegistryModule.Get().OnFilesLoaded().IsBoundToObject(this))
		{		
			AssetRegistryModule.Get().OnFilesLoaded().AddSP( this, &SReferenceViewer::OnInitialAssetRegistrySearchComplete );
		}
	}
	else
	{
		// All assets are already discovered, build the graph now.
		GraphObj->RebuildGraph();
	}

	// Set the initial history data
	HistoryManager.AddHistoryData();
}

void SReferenceViewer::OnNodeDoubleClicked(UEdGraphNode* Node)
{
	TSet<UObject*> Nodes;
	Nodes.Add(Node);
	ReCenterGraphOnNodes( Nodes );
}

FActionMenuContent SReferenceViewer::OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed)
{
	// no context menu when not over a node
	return FActionMenuContent();
}

bool SReferenceViewer::IsBackEnabled() const
{
	return HistoryManager.CanGoBack();
}

bool SReferenceViewer::IsForwardEnabled() const
{
	return HistoryManager.CanGoForward();
}

FReply SReferenceViewer::BackClicked()
{
	HistoryManager.GoBack();

	return FReply::Handled();
}

FReply SReferenceViewer::ForwardClicked()
{
	HistoryManager.GoForward();

	return FReply::Handled();
}

void SReferenceViewer::GraphNavigateHistoryBack()
{
	BackClicked();
}

void SReferenceViewer::GraphNavigateHistoryForward()
{
	ForwardClicked();
}

FText SReferenceViewer::GetHistoryBackTooltip() const
{
	if ( HistoryManager.CanGoBack() )
	{
		return FText::Format( LOCTEXT("HistoryBackTooltip", "Back to {0}"), HistoryManager.GetBackDesc() );
	}
	return FText::GetEmpty();
}

FText SReferenceViewer::GetHistoryForwardTooltip() const
{
	if ( HistoryManager.CanGoForward() )
	{
		return FText::Format( LOCTEXT("HistoryForwardTooltip", "Forward to {0}"), HistoryManager.GetForwardDesc() );
	}
	return FText::GetEmpty();
}

FText SReferenceViewer::GetAddressBarText() const
{
	if ( GraphObj )
	{
		if (TemporaryPathBeingEdited.IsEmpty())
		{
			const TArray<FName>& CurrentGraphRootPackageNames = GraphObj->GetCurrentGraphRootPackageNames();
			if (CurrentGraphRootPackageNames.Num() == 1)
			{
				return FText::FromName(CurrentGraphRootPackageNames[0]);
			}
			else if (CurrentGraphRootPackageNames.Num() > 1)
			{
				return FText::Format(LOCTEXT("AddressBarMultiplePackagesText", "{0} and {1} others"), FText::FromName(CurrentGraphRootPackageNames[0]), FText::AsNumber(CurrentGraphRootPackageNames.Num()));
			}
		}
		else
		{
			return TemporaryPathBeingEdited;
		}
	}

	return FText();
}

void SReferenceViewer::OnAddressBarTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		const FName NewPath(*NewText.ToString());

		TArray<FName> NewPaths;
		NewPaths.Add(NewPath);

		SetGraphRootPackageNames(NewPaths);
	}

	TemporaryPathBeingEdited = FText();
}

void SReferenceViewer::OnAddressBarTextChanged(const FText& NewText)
{
	TemporaryPathBeingEdited = NewText;
}

void SReferenceViewer::OnApplyHistoryData(const FReferenceViewerHistoryData& History)
{
	if ( GraphObj )
	{
		GraphObj->SetGraphRoot(History.PackageNames);
		UEdGraphNode_Reference* NewRootNode = GraphObj->RebuildGraph();
		
		if ( NewRootNode && ensure(GraphEditorPtr.IsValid()) )
		{
			GraphEditorPtr->SetNodeSelection(NewRootNode, true);
		}
	}
}

void SReferenceViewer::OnUpdateHistoryData(FReferenceViewerHistoryData& HistoryData) const
{
	if ( GraphObj )
	{
		const TArray<FName>& CurrentGraphRootPackageNames = GraphObj->GetCurrentGraphRootPackageNames();
		if ( CurrentGraphRootPackageNames.Num() == 1 )
		{
			HistoryData.HistoryDesc = FText::FromName(CurrentGraphRootPackageNames[0]);
		}
		else if ( CurrentGraphRootPackageNames.Num() > 1 )
		{
			HistoryData.HistoryDesc = FText::Format(LOCTEXT("AddressBarMultiplePackagesText", "{0} and {1} others"), FText::FromName(CurrentGraphRootPackageNames[0]), FText::AsNumber(CurrentGraphRootPackageNames.Num()));
		}
		else
		{
			HistoryData.HistoryDesc = FText::GetEmpty();
		}

		HistoryData.PackageNames = CurrentGraphRootPackageNames;
	}
	else
	{
		HistoryData.HistoryDesc = FText::GetEmpty();
		HistoryData.PackageNames.Empty();
	}
}

void SReferenceViewer::OnSearchDepthEnabledChanged( ECheckBoxState NewState )
{
	if ( GraphObj )
	{
		GraphObj->SetSearchDepthLimitEnabled(NewState == ECheckBoxState::Checked);
		GraphObj->RebuildGraph();
	}
}

ECheckBoxState SReferenceViewer::IsSearchDepthEnabledChecked() const
{
	if ( GraphObj )
	{
		return GraphObj->IsSearchDepthLimited() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}

int32 SReferenceViewer::GetSearchDepthCount() const
{
	if ( GraphObj )
	{
		return GraphObj->GetSearchDepthLimit();
	}
	else
	{
		return 0;
	}
}

void SReferenceViewer::OnSearchDepthCommitted(int32 NewValue)
{
	if ( GraphObj )
	{
		GraphObj->SetSearchDepthLimit(NewValue);
		GraphObj->RebuildGraph();
	}
}

void SReferenceViewer::OnSearchBreadthEnabledChanged( ECheckBoxState NewState )
{
	if ( GraphObj )
	{
		GraphObj->SetSearchBreadthLimitEnabled(NewState == ECheckBoxState::Checked);
		GraphObj->RebuildGraph();
	}
}

ECheckBoxState SReferenceViewer::IsSearchBreadthEnabledChecked() const
{
	if ( GraphObj )
	{
		return GraphObj->IsSearchBreadthLimited() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}

void SReferenceViewer::OnShowSoftReferencesChanged(ECheckBoxState NewState)
{
	if (GraphObj)
	{
		GraphObj->SetShowSoftReferencesEnabled(NewState == ECheckBoxState::Checked);
		GraphObj->RebuildGraph();
	}
}

ECheckBoxState SReferenceViewer::IsShowSoftReferencesChecked() const
{
	if (GraphObj)
	{
		return GraphObj->IsShowSoftReferences()? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}

void SReferenceViewer::OnShowSoftDependenciesChanged(ECheckBoxState NewState)
{
	if (GraphObj)
	{
		GraphObj->SetShowSoftDependenciesEnabled(NewState == ECheckBoxState::Checked);
		GraphObj->RebuildGraph();
	}
}

ECheckBoxState SReferenceViewer::IsShowSoftDependenciesChecked() const
{
	if (GraphObj)
	{
		return GraphObj->IsShowSoftDependencies() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}

void SReferenceViewer::OnShowHardReferencesChanged(ECheckBoxState NewState)
{
	if (GraphObj)
	{
		GraphObj->SetShowHardReferencesEnabled(NewState == ECheckBoxState::Checked);
		GraphObj->RebuildGraph();
	}
}


ECheckBoxState SReferenceViewer::IsShowHardReferencesChecked() const
{
	if (GraphObj)
	{
		return GraphObj->IsShowHardReferences() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}

void SReferenceViewer::OnShowHardDependenciesChanged(ECheckBoxState NewState)
{
	if (GraphObj)
	{
		GraphObj->SetShowHardDependenciesEnabled(NewState == ECheckBoxState::Checked);
		GraphObj->RebuildGraph();
	}
}

ECheckBoxState SReferenceViewer::IsShowHardDependenciesChecked() const
{
	if (GraphObj)
	{
		return GraphObj->IsShowHardDependencies() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}

int32 SReferenceViewer::GetSearchBreadthCount() const
{
	if ( GraphObj )
	{
		return GraphObj->GetSearchBreadthLimit();
	}
	else
	{
		return 0;
	}
}

void SReferenceViewer::OnSearchBreadthCommitted(int32 NewValue)
{
	if ( GraphObj )
	{
		GraphObj->SetSearchBreadthLimit(NewValue);
		GraphObj->RebuildGraph();
	}
}

void SReferenceViewer::RegisterActions()
{
	ReferenceViewerActions = MakeShareable(new FUICommandList);
	FReferenceViewerActions::Register();

	ReferenceViewerActions->MapAction(
		FGlobalEditorCommonCommands::Get().FindInContentBrowser,
		FExecuteAction::CreateSP(this, &SReferenceViewer::ShowSelectionInContentBrowser));

	ReferenceViewerActions->MapAction(
		FReferenceViewerActions::Get().OpenSelectedInAssetEditor,
		FExecuteAction::CreateSP(this, &SReferenceViewer::OpenSelectedInAssetEditor),
		FCanExecuteAction::CreateSP(this, &SReferenceViewer::HasExactlyOneNodeSelected));

	ReferenceViewerActions->MapAction(
		FReferenceViewerActions::Get().ReCenterGraph,
		FExecuteAction::CreateSP(this, &SReferenceViewer::ReCenterGraph),
		FCanExecuteAction());

	ReferenceViewerActions->MapAction(
		FReferenceViewerActions::Get().ListReferencedObjects,
		FExecuteAction::CreateSP(this, &SReferenceViewer::ListReferencedObjects),
		FCanExecuteAction::CreateSP(this, &SReferenceViewer::HasExactlyOneNodeSelected));

	ReferenceViewerActions->MapAction(
		FReferenceViewerActions::Get().ListObjectsThatReference,
		FExecuteAction::CreateSP(this, &SReferenceViewer::ListObjectsThatReference),
		FCanExecuteAction::CreateSP(this, &SReferenceViewer::HasExactlyOneNodeSelected));

	ReferenceViewerActions->MapAction(
		FReferenceViewerActions::Get().MakeLocalCollectionWithReferencers,
		FExecuteAction::CreateSP(this, &SReferenceViewer::MakeCollectionWithReferenersOrDependencies, ECollectionShareType::CST_Local, true),
		FCanExecuteAction::CreateSP(this, &SReferenceViewer::HasExactlyOneNodeSelected));
	
	ReferenceViewerActions->MapAction(
		FReferenceViewerActions::Get().MakePrivateCollectionWithReferencers,
		FExecuteAction::CreateSP(this, &SReferenceViewer::MakeCollectionWithReferenersOrDependencies, ECollectionShareType::CST_Private, true),
		FCanExecuteAction::CreateSP(this, &SReferenceViewer::HasExactlyOneNodeSelected));
	
	ReferenceViewerActions->MapAction(
		FReferenceViewerActions::Get().MakeSharedCollectionWithReferencers,
		FExecuteAction::CreateSP(this, &SReferenceViewer::MakeCollectionWithReferenersOrDependencies, ECollectionShareType::CST_Shared, true),
		FCanExecuteAction::CreateSP(this, &SReferenceViewer::HasExactlyOneNodeSelected));

	ReferenceViewerActions->MapAction(
		FReferenceViewerActions::Get().MakeLocalCollectionWithDependencies,
		FExecuteAction::CreateSP(this, &SReferenceViewer::MakeCollectionWithReferenersOrDependencies, ECollectionShareType::CST_Local, false),
		FCanExecuteAction::CreateSP(this, &SReferenceViewer::HasExactlyOneNodeSelected));

	ReferenceViewerActions->MapAction(
		FReferenceViewerActions::Get().MakePrivateCollectionWithDependencies,
		FExecuteAction::CreateSP(this, &SReferenceViewer::MakeCollectionWithReferenersOrDependencies, ECollectionShareType::CST_Private, false),
		FCanExecuteAction::CreateSP(this, &SReferenceViewer::HasExactlyOneNodeSelected));

	ReferenceViewerActions->MapAction(
		FReferenceViewerActions::Get().MakeSharedCollectionWithDependencies,
		FExecuteAction::CreateSP(this, &SReferenceViewer::MakeCollectionWithReferenersOrDependencies, ECollectionShareType::CST_Shared, false),
		FCanExecuteAction::CreateSP(this, &SReferenceViewer::HasExactlyOneNodeSelected));
	
	ReferenceViewerActions->MapAction(
		FReferenceViewerActions::Get().ShowSizeMap,
		FExecuteAction::CreateSP(this, &SReferenceViewer::ShowSizeMap),
		FCanExecuteAction::CreateSP(this, &SReferenceViewer::HasAtLeastOneNodeSelected));

	ReferenceViewerActions->MapAction(
		FReferenceViewerActions::Get().ShowReferenceTree,
		FExecuteAction::CreateSP(this, &SReferenceViewer::ShowReferenceTree),
		FCanExecuteAction::CreateSP(this, &SReferenceViewer::HasExactlyOneNodeSelected));
}

void SReferenceViewer::ShowSelectionInContentBrowser()
{
	TArray<FAssetData> AssetList;

	// Build up a list of selected assets from the graph selection set
	TSet<UObject*> SelectedNodes = GraphEditorPtr->GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator It(SelectedNodes); It; ++It)
	{
		if (UEdGraphNode_Reference* ReferenceNode = Cast<UEdGraphNode_Reference>(*It))
		{
			AssetList.Add(ReferenceNode->GetAssetData());
		}
	}

	if (AssetList.Num() > 0)
	{
		GEditor->SyncBrowserToObjects(AssetList);
	}
}

void SReferenceViewer::OpenSelectedInAssetEditor()
{
	UObject* SelectedObject = GetObjectFromSingleSelectedNode();

	if( SelectedObject )
	{
		FAssetEditorManager::Get().OpenEditorForAsset(SelectedObject);
	}
}

void SReferenceViewer::ReCenterGraph()
{
	ReCenterGraphOnNodes( GraphEditorPtr->GetSelectedNodes() );
}

void SReferenceViewer::ListReferencedObjects()
{
	UObject* SelectedObject = GetObjectFromSingleSelectedNode();

	if( SelectedObject )
	{
		// Show a list of objects that the selected object references
		ObjectTools::ShowReferencedObjs(SelectedObject);
	}
}

void SReferenceViewer::ListObjectsThatReference()
{	
	UObject* SelectedObject = GetObjectFromSingleSelectedNode();

	if ( SelectedObject )
	{
		TArray<UObject*> ObjectsToShow;
		ObjectsToShow.Add(SelectedObject);

		// Show a list of objects that reference the selected object
		ObjectTools::ShowReferencers( ObjectsToShow );
	}
}

void SReferenceViewer::MakeCollectionWithReferenersOrDependencies(ECollectionShareType::Type ShareType, bool bReferencers)
{
	TSet<FName> AllSelectedPackageNames;
	GetPackageNamesFromSelectedNodes(AllSelectedPackageNames);

	if (AllSelectedPackageNames.Num() > 0)
	{
		if (ensure(ShareType != ECollectionShareType::CST_All))
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

			FText CollectionNameAsText;
			FString FirstAssetName = FPackageName::GetLongPackageAssetName(AllSelectedPackageNames.Array()[0].ToString());
			if (bReferencers)
			{
				if (AllSelectedPackageNames.Num() > 1)
				{
					CollectionNameAsText = FText::Format(LOCTEXT("ReferencersForMultipleAssetNames", "{0}AndOthers_Referencers"), FText::FromString(FirstAssetName));
				}
				else
				{
					CollectionNameAsText = FText::Format(LOCTEXT("ReferencersForSingleAsset", "{0}_Referencers"), FText::FromString(FirstAssetName));
				}
			}
			else
			{
				if (AllSelectedPackageNames.Num() > 1)
				{
					CollectionNameAsText = FText::Format(LOCTEXT("DependenciesForMultipleAssetNames", "{0}AndOthers_Dependencies"), FText::FromString(FirstAssetName));
				}
				else
				{
					CollectionNameAsText = FText::Format(LOCTEXT("DependenciesForSingleAsset", "{0}_Dependencies"), FText::FromString(FirstAssetName));
				}
			}

			FName CollectionName;
			CollectionManagerModule.Get().CreateUniqueCollectionName(*CollectionNameAsText.ToString(), ShareType, CollectionName);

			FText ResultsMessage;
			
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			TArray<FName> PackageNamesToAddToCollection;
			if (bReferencers)
			{
				for (FName SelectedPackage : AllSelectedPackageNames)
				{
					AssetRegistryModule.Get().GetReferencers(SelectedPackage, PackageNamesToAddToCollection);
				}
			}
			else
			{
				for (FName SelectedPackage : AllSelectedPackageNames)
				{
					AssetRegistryModule.Get().GetDependencies(SelectedPackage, PackageNamesToAddToCollection);
				}
			}

			TSet<FName> ObjectPathsToAddToCollection;
			for (FName PackageToAdd : PackageNamesToAddToCollection)
			{
				if (!AllSelectedPackageNames.Contains(PackageToAdd))
				{
					const FString PackageString = PackageToAdd.ToString();
					const FName ObjectPath = *FString::Printf(TEXT("%s.%s"), *PackageString, *FPackageName::GetLongPackageAssetName(PackageString));
					ObjectPathsToAddToCollection.Add(ObjectPath);
				}
			}

			if (ObjectPathsToAddToCollection.Num() > 0)
			{
				if (CollectionManagerModule.Get().CreateCollection(CollectionName, ShareType, ECollectionStorageMode::Static))
				{
					if (CollectionManagerModule.Get().AddToCollection(CollectionName, ShareType, ObjectPathsToAddToCollection.Array()))
					{
						ResultsMessage = FText::Format(LOCTEXT("CreateCollectionSucceeded", "Created collection {0}"), FText::FromName(CollectionName));
					}
					else
					{
						ResultsMessage = FText::Format(LOCTEXT("AddToCollectionFailed", "Failed to add to the collection. {0}"), CollectionManagerModule.Get().GetLastError());
					}
				}
				else
				{
					ResultsMessage = FText::Format(LOCTEXT("CreateCollectionFailed", "Failed to create the collection. {0}"), CollectionManagerModule.Get().GetLastError());
				}
			}
			else
			{
				ResultsMessage = LOCTEXT("NothingToAddToCollection", "Nothing to add to collection.");
			}

			FMessageDialog::Open(EAppMsgType::Ok, ResultsMessage);
		}
	}
}

void SReferenceViewer::ShowSizeMap()
{
	TArray<FName> SelectedAssetPackageNames;

	TSet<UObject*> SelectedNodes = GraphEditorPtr->GetSelectedNodes();
	if ( ensure(SelectedNodes.Num()) == 1 )
	{
		UEdGraphNode_Reference* ReferenceNode = Cast<UEdGraphNode_Reference>(SelectedNodes.Array()[0]);
		if ( ReferenceNode )
		{
			SelectedAssetPackageNames.AddUnique( ReferenceNode->GetPackageName() );
		}
	}

	if( SelectedAssetPackageNames.Num() > 0 )
	{
		ISizeMapModule::Get().InvokeSizeMapTab( SelectedAssetPackageNames );
	}
}

void SReferenceViewer::ShowReferenceTree()
{
	UObject* SelectedObject = GetObjectFromSingleSelectedNode();

	if ( SelectedObject )
	{
		bool bObjectWasSelected = false;
		for (FSelectionIterator It(*GEditor->GetSelectedObjects()) ; It; ++It)
		{
			if ( (*It) == SelectedObject )
			{
				GEditor->GetSelectedObjects()->Deselect( SelectedObject );
				bObjectWasSelected = true;
			}
		}

		ObjectTools::ShowReferenceGraph( SelectedObject );

		if ( bObjectWasSelected )
		{
			GEditor->GetSelectedObjects()->Select( SelectedObject );
		}
	}
}

void SReferenceViewer::ReCenterGraphOnNodes(const TSet<UObject*>& Nodes)
{
	TArray<FName> NewGraphRootNames;
	FIntPoint TotalNodePos(EForceInit::ForceInitToZero);
	for ( auto NodeIt = Nodes.CreateConstIterator(); NodeIt; ++NodeIt )
	{
		UEdGraphNode_Reference* ReferenceNode = Cast<UEdGraphNode_Reference>(*NodeIt);
		if ( ReferenceNode )
		{
			NewGraphRootNames.Add(ReferenceNode->GetPackageName());
			TotalNodePos.X += ReferenceNode->NodePosX;
			TotalNodePos.Y += ReferenceNode->NodePosY;
		}
	}

	if ( NewGraphRootNames.Num() > 0 )
	{
		const FIntPoint AverageNodePos = TotalNodePos / NewGraphRootNames.Num();
		GraphObj->SetGraphRoot(NewGraphRootNames, AverageNodePos);
		UEdGraphNode_Reference* NewRootNode = GraphObj->RebuildGraph();

		if ( NewRootNode && ensure(GraphEditorPtr.IsValid()) )
		{
			GraphEditorPtr->ClearSelectionSet();
			GraphEditorPtr->SetNodeSelection(NewRootNode, true);
		}

		// Set the initial history data
		HistoryManager.AddHistoryData();
	}
}

UObject* SReferenceViewer::GetObjectFromSingleSelectedNode() const
{
	UObject* ReturnObject = nullptr;

	TSet<UObject*> SelectedNodes = GraphEditorPtr->GetSelectedNodes();
	if ( ensure(SelectedNodes.Num()) == 1 )
	{
		UEdGraphNode_Reference* ReferenceNode = Cast<UEdGraphNode_Reference>(SelectedNodes.Array()[0]);
		if ( ReferenceNode )
		{
			const FAssetData& AssetData = ReferenceNode->GetAssetData();
			if (AssetData.IsAssetLoaded())
			{
				ReturnObject = AssetData.GetAsset();
			}
			else
			{
				FScopedSlowTask SlowTask(0, LOCTEXT("LoadingSelectedObject", "Loading selection..."));
				SlowTask.MakeDialog();
				ReturnObject = AssetData.GetAsset();
			}
		}
	}

	return ReturnObject;
}

void SReferenceViewer::GetPackageNamesFromSelectedNodes(TSet<FName>& OutNames) const
{
	TSet<UObject*> SelectedNodes = GraphEditorPtr->GetSelectedNodes();
	for (UObject* Node : SelectedNodes)
	{
		UEdGraphNode_Reference* ReferenceNode = Cast<UEdGraphNode_Reference>(Node);
		if (ReferenceNode)
		{
			TArray<FName> NodePackageNames;
			ReferenceNode->GetAllPackageNames(NodePackageNames);
			OutNames.Append(NodePackageNames);
		}
	}
}

bool SReferenceViewer::HasExactlyOneNodeSelected() const
{
	if ( GraphEditorPtr.IsValid() )
	{
		return GraphEditorPtr->GetSelectedNodes().Num() == 1;
	}
	
	return false;
}

bool SReferenceViewer::HasAtLeastOneNodeSelected() const
{
	if ( GraphEditorPtr.IsValid() )
	{
		return GraphEditorPtr->GetSelectedNodes().Num() > 0;
	}
	
	return false;
}

void SReferenceViewer::OnInitialAssetRegistrySearchComplete()
{
	if ( GraphObj )
	{
		GraphObj->RebuildGraph();
	}
}

#undef LOCTEXT_NAMESPACE