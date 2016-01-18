// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/AssetEditorToolkit.h"
#include "LevelSequence.h"

enum class EMapChangeType : uint8;
class FTabManager;
class ILevelViewport;
class ISequencer;
class IToolkitHost;
class SWidget;
class UWorld;
struct FSequencerViewParams;


/**
 * Implements an Editor toolkit for level sequences.
 */
class FLevelSequenceEditorToolkit
	: public FAssetEditorToolkit
	, public FGCObject
{ 
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InStyle The style set to use.
	 */
	FLevelSequenceEditorToolkit(const TSharedRef<ISlateStyle>& InStyle);

	/** Virtual destructor */
	virtual ~FLevelSequenceEditorToolkit();

public:

	/**
	 * Initialize this asset editor.
	 *
	 * @param Mode Asset editing mode for this editor (standalone or world-centric).
	 * @param InitToolkitHost When Mode is WorldCentric, this is the level editor instance to spawn this editor within.
	 * @param LevelSequence The animation to edit.
	 * @param TrackEditorDelegates Delegates to call to create auto-key handlers for this sequencer.
	 * @param bEditWithinLevelEditor Whether or not sequencer should be edited within the level editor.
	 */
	void Initialize(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, ULevelSequence* LevelSequence, bool bEditWithinLevelEditor);

public:

	// IToolkit interface

	virtual FText GetBaseToolkitName() const override;
	virtual FName GetToolkitFName() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;

	void UpdateViewports(AActor* ActorToViewThrough) const;

	void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject(LevelSequence);
	}

private:

	/** Callback for executing the Add Component action. */
	void HandleAddComponentActionExecute(UActorComponent* Component);

	/** Callback for executing the add component material track. */
	void HandleAddComponentMaterialActionExecute(UPrimitiveComponent* Component, int32 MaterialIndex);

	/** Callback for map changes. */
	void HandleMapChanged(UWorld* NewWorld, EMapChangeType MapChangeType);

	/** Callback for the menu extensibility manager. */
	TSharedRef<FExtender> HandleMenuExtensibilityGetExtender(const TSharedRef<FUICommandList> CommandList, const TArray<UObject*> ContextSensitiveObjects);

	/** Callback for spawning tabs. */
	TSharedRef<SDockTab> HandleTabManagerSpawnTab(const FSpawnTabArgs& Args);	

	/** Callback for the track menu extender. */
	void HandleTrackMenuExtensionAddTrack(FMenuBuilder& AddTrackMenuBuilder, TArray<UObject*> ContextObjects);

	/** Menu extension callback for the add menu */
	void AddPosessActorMenuExtensions(FMenuBuilder& MenuBuilder);

	/** Add the specified actors to the sequencer */
	void AddActorsToSequencer(AActor*const* InActors, int32 NumActors);

private:

	/** Level sequence for our edit operation. */
	ULevelSequence* LevelSequence;

	/** The sequencer used by this editor. */
	TSharedPtr<ISequencer> Sequencer;

	/** A map of all the transport controls to viewports that this sequencer has made */
	TMap<TWeakPtr<ILevelViewport>, TSharedPtr<SWidget>> TransportControls;

	FDelegateHandle SequencerExtenderHandle;

	/** Pointer to the style set to use for toolkits. */
	TSharedRef<ISlateStyle> Style;

private:

	/**	The tab ids for all the tabs used */
	static const FName SequencerMainTabId;
};
