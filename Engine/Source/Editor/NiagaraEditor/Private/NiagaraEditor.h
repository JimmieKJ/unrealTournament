// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "INiagaraEditor.h"
#include "Toolkits/AssetEditorToolkit.h"


/** Viewer/editor for a DataTable */
class FNiagaraEditor : public INiagaraEditor, public FNotifyHook
{

public:

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/** Edits the specified Niagara Script */
	void InitNiagaraEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraScript* Script );

	/** Destructor */
	virtual ~FNiagaraEditor();

	// Begin IToolkit interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	// End IToolkit interface

	// Delegates
	FReply OnCompileClicked();
	void DeleteSelectedNodes();
	bool CanDeleteNodes() const;

	// Widget Accessors
	TSharedRef<class IDetailsView> GetDetailView() const { return NiagaraDetailsView.ToSharedRef(); }
protected:

	/** Called when the selection changes in the GraphEditor */
	void OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection);

private:
	/** Create widget for graph editing */
	TSharedRef<class SGraphEditor> CreateGraphEditorWidget(UEdGraph* InGraph);

	/** Spawns the tab with the update graph inside */
	TSharedRef<SDockTab> SpawnTab_NodeGraph(const FSpawnTabArgs& Args);

	/** Spawns the tab with the node details inside. */
	TSharedRef<SDockTab> SpawnTab_NodeProperties(const FSpawnTabArgs& Args);

	/** Builds the toolbar widget */
	void ExtendToolbar();
private:

	/* The Script being edited */
	UNiagaraScript*				Script;
	/** Source of script being edited */
	UNiagaraScriptSource*		Source;
	/** */
	TWeakPtr<SGraphEditor>		NodeGraphEditorPtr;

	/** The command list for this editor */
	TSharedPtr<FUICommandList> GraphEditorCommands;

	/** Property View */
	TSharedPtr<class IDetailsView> NiagaraDetailsView;

	/**	The tab ids for the Niagara editor */
	static const FName NodeGraphTabId;
	static const FName PropertiesTabId;

};
