// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"

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

	/** Save state of the objects that this movie scene controls. */
	MOVIESCENE_API void SaveState(class IMovieScenePlayer& Player);

	/** Restore state of the objects that this movie scene controls. */
	MOVIESCENE_API void RestoreState(class IMovieScenePlayer& Player);

	/**
	 * Updates this movie scene.
	 *
	 * @param Position The local playback position.
	 * @param Player Movie scene player interface for interaction with runtime data.
	 */
	MOVIESCENE_API void Update(float Position, float LastPosition, class IMovieScenePlayer& Player);

	/**
	 * Refreshes the existing instance.
	 *
	 * Called when something significant about movie scene data changes (like adding or removing a track).
	 * Instantiates all new tracks found and removes instances for tracks that no longer exist.
	 */
	MOVIESCENE_API void RefreshInstance(class IMovieScenePlayer& Player);

	/**
	 * Spawn an object relating to the specified object ID
	 * 
	 * @param ObjectId The ID of the object to spawn
	 * @param Player The movie scene player responsible for this instance
	 * @return The newly spawned or previously-spawned object, or nullptr
	 */
	MOVIESCENE_API void SpawnObject(const FGuid& ObjectId, IMovieScenePlayer& Player);

	/**
	 * Destroy a previously spawned object relating to the specified object ID
	 * 
	 * @param ObjectId The ID of the object to destroy
	 * @param Player The movie scene player responsible for this instance
	 */
	MOVIESCENE_API void DestroySpawnedObject(const FGuid& ObjectId, IMovieScenePlayer& Player);

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

protected:

	void RefreshInstanceMap(const TArray<UMovieSceneTrack*>& Tracks, const TArray<UObject*>& RuntimeObjects, FMovieSceneInstanceMap& TrackInstances, class IMovieScenePlayer& Player);

	void UpdateInternal(float Position, float LastPosition, class IMovieScenePlayer& Player, EMovieSceneUpdatePass UpdatePass);

	/** Update the object binding instance for the specified object */
	void UpdateObjectBinding(const FGuid& ObjectId, IMovieScenePlayer& Player);

private:

	/** A paring of a runtime object guid to a runtime data and track instances animating the runtime objects */
	struct FMovieSceneObjectBindingInstance
	{
		/** Runtime object guid for looking up runtime UObjects */
		FGuid ObjectGuid;

		/** Actual runtime objects */	
		TArray<UObject*> RuntimeObjects;

		/** Instances that animate the runtime objects */
		FMovieSceneInstanceMap TrackInstances;
	};


	/** MovieScene that is instanced */
	const TWeakObjectPtr<UMovieSceneSequence> MovieSceneSequence;

	/** The shot track instance map */
	TSharedPtr<IMovieSceneTrackInstance> ShotTrackInstance;

	/** All Master track instances */
	FMovieSceneInstanceMap MasterTrackInstances;

	/** All object binding instances */
	TMap<FGuid, FMovieSceneObjectBindingInstance> ObjectBindingInstances;

	/** Cached time range for the movie scene */
	TRange<float> TimeRange;
};
