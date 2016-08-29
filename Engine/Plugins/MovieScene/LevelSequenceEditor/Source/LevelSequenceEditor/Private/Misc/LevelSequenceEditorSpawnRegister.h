// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LevelSequenceSpawnRegister.h"
#include "ObjectKey.h"

class AActor;
class FMovieSceneSequenceInstance;
class IMovieScenePlayer;
class ISequencer;
class UObject;
class UProperty;


/**
 * Spawn register used in the editor to add some usability features like maintaining selection states, and projecting spawned state onto spawnable defaults
 */
class FLevelSequenceEditorSpawnRegister
	: public FLevelSequenceSpawnRegister
{
public:

	/** Constructor */
	FLevelSequenceEditorSpawnRegister();

	/** Destructor. */
	~FLevelSequenceEditorSpawnRegister();

public:

	void SetSequencer(const TSharedPtr<ISequencer>& Sequencer);

public:

	// FLevelSequenceSpawnRegister interface

	virtual UObject* SpawnObject(const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance, IMovieScenePlayer& Player) override;
	virtual void PreDestroyObject(UObject& Object, const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance) override;
#if WITH_EDITOR
	virtual TValueOrError<FNewSpawnable, FText> CreateNewSpawnableType(UObject& SourceObject, UMovieScene& OwnerMovieScene) override;
#endif

private:

	/** Called when the editor selection has changed. */
	void HandleActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);

	/** Called before sequencer attempts to save the movie scene(s) it's editing */
	void OnPreSaveMovieScene(ISequencer& InSequencer);

	/** Called when a new movie scene sequence instance has been activated */
	void OnSequenceInstanceActivated(FMovieSceneSequenceInstance& ActiveInstance);

	/** Saves the default state for the specified spawnable, if an instance for it currently exists */
	void SaveDefaultSpawnableState(const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance);

	/** Check whether the specified objects are editable on the details panel. Called from the level editor */
	bool AreObjectsEditable(const TArray<TWeakObjectPtr<UObject>>& InObjects) const;
	
	/** Called from the editor when a blueprint object replacement has occurred */
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap);

private:

	/** Handles for delegates that we've bound to. */
	FDelegateHandle OnActorSelectionChangedHandle, OnAreObjectsEditableHandle;

	/** Set of spawn register keys for objects that should be selected if they are spawned. */
	TSet<FMovieSceneSpawnRegisterKey> SelectedSpawnedObjects;

	/** Set of currently spawned objects */
	TMap<FGuid, TSet<FObjectKey>> SpawnedObjects;

	/** True if we should clear the above selection cache when the editor selection has been changed. */
	bool bShouldClearSelectionCache;

	/** Weak pointer to the active sequencer. */
	TWeakPtr<ISequencer> WeakSequencer;

	/** Identifier for the current active level sequence */
	FGuid ActiveSequence;
};
