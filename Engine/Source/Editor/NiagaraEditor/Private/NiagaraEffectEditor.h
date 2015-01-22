// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "INiagaraEffectEditor.h"
#include "Toolkits/AssetEditorToolkit.h"



/** Viewer/editor for a NiagaraEffect
*/
class FNiagaraEffectEditor : public INiagaraEffectEditor
{

public:

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/** Edits the specified Niagara Script */
	void InitNiagaraEffectEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraEffect* Effect);

	/** Destructor */
	virtual ~FNiagaraEffectEditor();

	// Begin IToolkit interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	// End IToolkit interface


	virtual UNiagaraEffect *GetEffect() const	override { return Effect; }	
private:
	/** Create widget for graph editing */
	TSharedRef<class SNiagaraEffectEditorWidget> CreateEditorWidget(UNiagaraEffect* InEffect);

	/** Spawns the tab with the update graph inside */
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);

	/** Builds the toolbar widget */
	void ExtendToolbar();
	FReply OnAddEmitterClicked();
private:

	/* The Effect being edited */
	UNiagaraEffect	*Effect;
	class FNiagaraEffectInstance *EffectInstance;


	/** The command list for this editor */
	TSharedPtr<FUICommandList> EditorCommands;

	TWeakPtr<SNiagaraEffectEditorWidget>		UpdateEditorPtr;

	/**	Graph editor tab */
	static const FName UpdateTabId;
};
