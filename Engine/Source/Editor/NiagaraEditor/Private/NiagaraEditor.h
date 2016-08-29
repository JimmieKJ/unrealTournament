// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "INiagaraEditor.h"
#include "Toolkits/AssetEditorToolkit.h"

class UNiagaraScriptSource;

/** Viewer/editor for a DataTable */
class FNiagaraEditor : public INiagaraEditor, public FNotifyHook, public FEditorUndoClient
{

public:

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/** Edits the specified Niagara Script */
	void InitNiagaraEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraScript* Script );

	/** Destructor */
	virtual ~FNiagaraEditor();

	//~ Begin IToolkit Interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	//~ End IToolkit Interface

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	// End of FEditorUndoClient

	// Widget Accessors
	TSharedRef<class IDetailsView> GetDetailView() const { return NiagaraDetailsView.ToSharedRef(); }
protected:

	/** Called when the selection changes in the GraphEditor */
	void OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection);

private:
	/** Create widget for graph editing */
	TSharedRef<class SGraphEditor> CreateGraphEditorWidget(UEdGraph* InGraph, bool bEditable = true);

	/** Spawns the tab with the update graph inside */
	TSharedRef<SDockTab> SpawnTab_NodeGraph(const FSpawnTabArgs& Args);

	/** Spawns the tab with the flattened graph inside */
	TSharedRef<SDockTab> SpawnTab_FlattenedNodeGraph(const FSpawnTabArgs& Args);

	/** Spawns the tab with the node details inside. */
	TSharedRef<SDockTab> SpawnTab_NodeProperties(const FSpawnTabArgs& Args);

	/** Builds the toolbar widget */
	void ExtendToolbar();


	// Delegates for graph editor commands
	FReply OnCompileClicked();
	void SelectAllNodes();
	bool CanSelectAllNodes() const;
	void DeleteSelectedNodes();
	bool CanDeleteNodes() const;
	void DeleteSelectedDuplicatableNodes();
	void CutSelectedNodes();
	bool CanCutNodes() const;
	void CopySelectedNodes();
	bool CanCopyNodes() const;
	void PasteNodes();
	void PasteNodesHere(const FVector2D& Location);
	bool CanPasteNodes() const;
	void DuplicateNodes();
	bool CanDuplicateNodes() const;

private:

	/* The Script being edited */
	UNiagaraScript*				Script;
	/** Source of script being edited */
	UNiagaraScriptSource*		Source;
	/** */
	TWeakPtr<SGraphEditor>		NodeGraphEditorPtr;
	TWeakPtr<SGraphEditor>		FlattenedNodeGraphEditorPtr;

	/** The command list for this editor */
	TSharedPtr<FUICommandList> GraphEditorCommands;

	/** Property View */
	TSharedPtr<class IDetailsView> NiagaraDetailsView;

	/**	The tab ids for the Niagara editor */
	static const FName NodeGraphTabId; 
	static const FName FlattenedNodeGraphTabId; 
	static const FName PropertiesTabId;

};
