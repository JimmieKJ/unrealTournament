// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "INiagaraEffectEditor.h"
#include "Toolkits/AssetEditorToolkit.h"

#include "SNiagaraEffectEditorViewport.h"
#include "SNiagaraEffectEditorWidget.h"
#include "NiagaraSequencer.h"

class UNiagaraSequence;

/** Viewer/editor for a NiagaraEffect
*/
class FNiagaraEffectEditor : public INiagaraEffectEditor, public FNotifyHook, public FGCObject
{

public:

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/** Edits the specified Niagara Script */
	void InitNiagaraEffectEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraEffect* Effect);

	/** Destructor */
	virtual ~FNiagaraEffectEditor();

	//~ Begin IToolkit Interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	//~ End IToolkit Interface

	// Delegates for the individual widgets
	FReply OnEmitterSelected(TSharedPtr<FNiagaraSimulation> SelectedItem, ESelectInfo::Type SelType);


	virtual UNiagaraEffect *GetEffect() const	override { return Effect; }	

	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
	{
		return MakeShareable(new FNiagaraTrackEditor(InSequencer) );
	}

	virtual void NotifyPreChange(UProperty* PropertyAboutToChanged)override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged) override;

	FReply OnDeleteEmitterClicked(TSharedPtr<FNiagaraSimulation> Emitter);
	FReply OnDuplicateEmitterClicked(TSharedPtr<FNiagaraSimulation> Emitter);

	/** FGCObject interface */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

private:
	/** Create widget for graph editing */
	TSharedRef<class SNiagaraEffectEditorWidget> CreateEditorWidget(UNiagaraEffect* InEffect);

	/** Spawns the tab with the update graph inside */
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);

	TSharedPtr<SNiagaraEffectEditorViewport>	Viewport;
	TSharedPtr<SNiagaraEffectEditorWidget>	EmitterEditorWidget;
	TSharedPtr<SNiagaraEffectEditorWidget>	DevEmitterEditorWidget;
	TSharedPtr< SNiagaraTimeline > TimeLine;
	
	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_EmitterList(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_DevEmitterList(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_CurveEd(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Sequencer(const FSpawnTabArgs& Args);

	/** Builds the toolbar widget */
	void ExtendToolbar();
	FReply OnAddEmitterClicked();

private:

	/* The Effect being edited */
	UNiagaraEffect	*Effect;
	TSharedPtr<FNiagaraEffectInstance> EffectInstance;

	/* stuff needed by the Sequencer */
	UNiagaraSequence *NiagaraSequence;
	TSharedPtr<ISequencer> Sequencer;


	/** The command list for this editor */
	TSharedPtr<FUICommandList> EditorCommands;

	TWeakPtr<SNiagaraEffectEditorWidget>		UpdateEditorPtr;

	/**	Graph editor tab */
	static const FName UpdateTabId;
	static const FName ViewportTabID;
	static const FName EmitterEditorTabID;
	static const FName DevEmitterEditorTabID;
	static const FName CurveEditorTabID;
	static const FName SequencerTabID;
};
