// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CollectionManagerTypes.h"

/**
 * 
 */
class SReferenceViewer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SReferenceViewer ){}

	SLATE_END_ARGS()

	~SReferenceViewer();

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

	/** Sets a new root package name */
	void SetGraphRootPackageNames(const TArray<FName>& NewGraphRootPackageNames);

private:

	/** Called to create context menu when right-clicking on graph */
	FActionMenuContent OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed);

	/** Called when a node is double clicked */
	void OnNodeDoubleClicked(class UEdGraphNode* Node);

	/** True if the user may use the history back button */
	bool IsBackEnabled() const;

	/** True if the user may use the history forward button */
	bool IsForwardEnabled() const;

	/** Handler for clicking the history back button */
	FReply BackClicked();

	/** Handler for clicking the history forward button */
	FReply ForwardClicked();

	/** Handler for when the graph panel tells us to go back in history (like using the mouse thumb button) */
	void GraphNavigateHistoryBack();

	/** Handler for when the graph panel tells us to go forward in history (like using the mouse thumb button) */
	void GraphNavigateHistoryForward();

	/** Gets the tool tip text for the history back button */
	FText GetHistoryBackTooltip() const;

	/** Gets the tool tip text for the history forward button */
	FText GetHistoryForwardTooltip() const;

	/** Gets the text to be displayed in the address bar */
	FText GetAddressBarText() const;

	/** Called when the path is being edited */
	void OnAddressBarTextChanged(const FText& NewText);

	/** Sets the new path for the viewer */
	void OnAddressBarTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	void OnApplyHistoryData(const FReferenceViewerHistoryData& History);

	void OnUpdateHistoryData(FReferenceViewerHistoryData& HistoryData) const;
	
	void OnSearchDepthEnabledChanged( ECheckBoxState NewState );
	ECheckBoxState IsSearchDepthEnabledChecked() const;
	int32 GetSearchDepthCount() const;
	void OnSearchDepthCommitted(int32 NewValue);

	void OnSearchBreadthEnabledChanged( ECheckBoxState NewState );
	ECheckBoxState IsSearchBreadthEnabledChecked() const;

	void OnShowSoftReferencesChanged( ECheckBoxState NewState );
	ECheckBoxState IsShowSoftReferencesChecked() const;
	void OnShowSoftDependenciesChanged(ECheckBoxState NewState);
	ECheckBoxState IsShowSoftDependenciesChecked() const;

	void OnShowHardReferencesChanged(ECheckBoxState NewState);
	ECheckBoxState IsShowHardReferencesChecked() const;
	void OnShowHardDependenciesChanged(ECheckBoxState NewState);
	ECheckBoxState IsShowHardDependenciesChecked() const;

	int32 GetSearchBreadthCount() const;
	void OnSearchBreadthCommitted(int32 NewValue);

	void RegisterActions();
	void ShowSelectionInContentBrowser();
	void OpenSelectedInAssetEditor();
	void ReCenterGraph();
	void ListReferencedObjects();
	void ListObjectsThatReference();
	void MakeCollectionWithReferenersOrDependencies(ECollectionShareType::Type ShareType, bool bReferencers);
	void ShowSizeMap();
	void ShowReferenceTree();

	void ReCenterGraphOnNodes(const TSet<UObject*>& Nodes);

	UObject* GetObjectFromSingleSelectedNode() const;
	void GetPackageNamesFromSelectedNodes(TSet<FName>& OutNames) const;
	bool HasExactlyOneNodeSelected() const;
	bool HasAtLeastOneNodeSelected() const;

	void OnInitialAssetRegistrySearchComplete();
private:

	/** The manager that keeps track of history data for this browser */
	FReferenceViewerHistoryManager HistoryManager;

	TSharedPtr<SGraphEditor> GraphEditorPtr;

	TSharedPtr<FUICommandList> ReferenceViewerActions;

	UEdGraph_ReferenceViewer* GraphObj;

	/** The temporary copy of the path text when it is actively being edited. */
	FText TemporaryPathBeingEdited;
};
