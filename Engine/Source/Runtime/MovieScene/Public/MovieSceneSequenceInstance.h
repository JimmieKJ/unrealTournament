// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"

class IMovieScenePlayer;
class IMovieSceneTrackInstance;

class UMovieSceneTrack;
class UMovieSceneSequence;

typedef TMap<TWeakObjectPtr<UMovieSceneTrack>, TSharedPtr<class IMovieSceneTrackInstance>> FMovieSceneInstanceMap;

/**
 * A sequence instance holds the live objects bound to the tracks in a sequence.  This is completely transient
 * There is one instance per sequence.  If a sequence holds multiple sub-sequences each sub-sequence will have it's own instance even if they are the same sequence asset
 * A sequence instance also creates and manages all track instances for the tracks in a sequence
 */
class FMovieSceneSequenceInstance
	: public TSharedFromThis<FMovieSceneSequenceInstance>
{
public:

	/**
	 * Creates and initializes a new instance from a Sequence
	 *
	 * @param InMovieSceneSequence The sequence that this instance represents.
	 */
	MOVIESCENE_API FMovieSceneSequenceInstance(const UMovieSceneSequence& InMovieSceneSequence);

	/** 
	 * Creates and initializes a new instance that is primed for recording 
	 * 
	 * @param InMovieSceneSubTrack The subtrack we are primed for recording in
	 */
	MOVIESCENE_API FMovieSceneSequenceInstance(const UMovieSceneTrack& InMovieSceneSubTrack);

	MOVIESCENE_API ~FMovieSceneSequenceInstance();

	/**
	 * Find the identifier for a possessed or spawned object.
	 *
	 * @param Object The object to get the identifier for.
	 * @return The object identifier, or an invalid GUID if not found.
	 */
	MOVIESCENE_API FGuid FindObjectId(UObject& Object) const;

	/**
	 * Find the identifier for the parent of the specified object
	 *
	 * @param Object The object to get the parent identifier for.
	 * @return The parent's identifier, or an invalid GUID if not applicable.
	 */
	MOVIESCENE_API FGuid FindParentObjectId(UObject& Object) const;
	
	/**
	 * Find the object relating to the specified GUID
	 *
	 * @param ObjectId The object identifier
	 * @return The object, or nullptr
	 */
	MOVIESCENE_API UObject* FindObject(const FGuid& ObjectId, const IMovieScenePlayer& Player) const;

	/**
	 * Find the spawned object relating to the specified GUID
	 *
	 * @param ObjectId The object identifier
	 * @return The object, or nullptr
	 */
	MOVIESCENE_API UObject* FindSpawnedObject(const FGuid& ObjectId) const;

	/** Save state of the objects that this movie scene controls. */
	MOVIESCENE_API void SaveState(IMovieScenePlayer& Player);

	/** Restore state of the objects that this movie scene controls. */
	MOVIESCENE_API void RestoreState(IMovieScenePlayer& Player);

	/** Restore state of a specific object track that this movie scene controls. */
	MOVIESCENE_API void RestoreSpecificState(const FGuid& ObjectGuid, IMovieScenePlayer& Player);

	/**
	 * Updates this movie scene.
	 *
	 * @param UpdateData The current and previous position of the moviescene that is playing.
	 * @param Player Movie scene player interface for interaction with runtime data.
	 */
	MOVIESCENE_API void Update(EMovieSceneUpdateData& UpdateData, IMovieScenePlayer& Player);

	/*
	 * Pre update - spawnables, any stale runtime objects.
	 */
	MOVIESCENE_API void PreUpdate(IMovieScenePlayer& Player);
	
	/*
	 * Update all the passes
	 */
	MOVIESCENE_API void UpdatePasses(EMovieSceneUpdateData& UpdateData, IMovieScenePlayer& Player);

	/*
	 * Post update - clean up spawnables.
	 */
	MOVIESCENE_API void PostUpdate(IMovieScenePlayer& Player);

	/*
	 * Update a single pass
	 */
	MOVIESCENE_API void UpdatePassSingle(EMovieSceneUpdateData& UpdateData, IMovieScenePlayer& Player);

	MOVIESCENE_API void UpdateFromSubSceneDeactivate();

	/**
	 * Refreshes the existing instance.
	 *
	 * Called when something significant about movie scene data changes (like adding or removing a track).
	 * Instantiates all new tracks found and removes instances for tracks that no longer exist.
	 */
	MOVIESCENE_API void RefreshInstance(IMovieScenePlayer& Player);

	/**
	 * Spawn an object relating to the specified object ID
	 * 
	 * @param ObjectId The ID of the object to spawn
	 * @param Player The movie scene player responsible for this instance
	 * @return The newly spawned or previously-spawned object, or nullptr
	 */
	MOVIESCENE_API void OnObjectSpawned(const FGuid& ObjectId, UObject& SpawnedObject, IMovieScenePlayer& Player);

	/**
	 * Destroy a previously spawned object relating to the specified object ID
	 * 
	 * @param ObjectId The ID of the object to destroy
	 * @param Player The movie scene player responsible for this instance
	 */
	MOVIESCENE_API void OnSpawnedObjectDestroyed(const FGuid& ObjectId, IMovieScenePlayer& Player);

	/**
	 * Get the sequence associated with this instance.
	 *
	 * @return The movie scene sequence object.
	 */
	UMovieSceneSequence* GetSequence()
	{
		return MovieSceneSequence.Get();
	}

	/** 
	 * Get the time range for the movie scene.
	 *
	 * Note: This is not necessarily how long the instance will be playing.
	 * The parent could have changed the length.
	 *
	 * @return The time range.
	 */
	TRange<float> GetTimeRange() const
	{
		return TimeRange;
	}

	/** Handle the sequence this instance refers to changing */
	MOVIESCENE_API void HandleSequenceSectionChanged(UMovieSceneSequence* Sequence);

#if WITH_EDITOR
	/**
	 * @return a transient but, unique identifier for this instance
	 */
	const FGuid& GetInstanceId() const { return InstanceId; }
#endif

protected:

	void RefreshInstanceMap(const TArray<UMovieSceneTrack*>& Tracks, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, FMovieSceneInstanceMap& TrackInstances, IMovieScenePlayer& Player);

	/** Update the object binding instance for the specified object */
	void UpdateObjectBinding(const FGuid& ObjectId, IMovieScenePlayer& Player);

private:

	/** A paring of a runtime object guid to a runtime data and track instances animating the runtime objects */
	struct FMovieSceneObjectBindingInstance
	{
		/** Runtime object guid for looking up runtime UObjects */
		FGuid ObjectGuid;

		/** Actual runtime objects */	
		TArray<TWeakObjectPtr<UObject>> RuntimeObjects;

		/** Instances that animate the runtime objects */
		FMovieSceneInstanceMap TrackInstances;
	};


	/** MovieScene that is instanced */
	TWeakObjectPtr<UMovieSceneSequence> MovieSceneSequence;

	/** The camera cut track instance map */
	TSharedPtr<IMovieSceneTrackInstance> CameraCutTrackInstance;

	/** All Master track instances */
	FMovieSceneInstanceMap MasterTrackInstances;

	/** All object binding instances */
	TMap<FGuid, FMovieSceneObjectBindingInstance> ObjectBindingInstances;

	/** A map of object ID -> spawned object */
	TMap<FGuid, TWeakObjectPtr<UObject>> SpawnedObjects;

	/** Cached time range for the movie scene */
	TRange<float> TimeRange;

#if WITH_EDITOR
	FGuid InstanceId;
#endif
};
