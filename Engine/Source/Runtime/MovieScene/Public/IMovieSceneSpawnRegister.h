// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSequenceInstance.h"

class IMovieScenePlayer;
class FMovieSceneSequenceInstance;

/**
 * Class responsible for managing spawnables in a movie scene
 */
class IMovieSceneSpawnRegister : public TSharedFromThis<IMovieSceneSpawnRegister>
{
public:
	virtual ~IMovieSceneSpawnRegister() {}

	/**
	 * Spawn an object for the specified GUID, from the specified sequence instance.
	 *
	 * @param Object 		ID of the object to spawn
	 * @param Instance 		Sequence instance that is spawning the object
	 * @param Player 		Movie scene player that is ultimately responsible for spawning the object
	 * @return the spawned object, or nullptr on failure
	 */
	virtual UObject* SpawnObject(const FGuid& Object, FMovieSceneSequenceInstance& Instance, IMovieScenePlayer& Player) = 0;

	/**
	 * Destroy a specific previously spawned object
	 *
	 * @param Object 		ID of the object to destroy
	 * @param Instance 		Sequence instance that is destroying the object
	 * @param Player 		Movie scene player that is ultimately responsible for destroying the object
	 */
	virtual void DestroySpawnedObject(const FGuid& Object, FMovieSceneSequenceInstance& Instance, IMovieScenePlayer& Player) = 0;

	/**
	 * Destroy all objects spawned, and owned, by the specified sequence instance
	 * @note this only destorys objects that the specified sequence instance has direct ownership of (ownership == InnerSequence)
	 *
	 * @param Instance 		Sequence instance that is destroying the objects
	 * @param Player 		Movie scene player that is ultimately responsible for destroying the objects
	 */
	virtual void DestroyObjectsOwnedByInstance(FMovieSceneSequenceInstance& Instance, IMovieScenePlayer& Player) = 0;

	/**
	 * Destroy all objects that have ever been spawned by the specified sequence instance (regardless of ownership)
	 *
	 * @param Instance 		Sequence instance that is destroying the objects
	 * @param Player 		Movie scene player that is ultimately responsible for destroying the objects
	 */
	virtual void DestroyObjectsSpawnedByInstance(FMovieSceneSequenceInstance& Instance, IMovieScenePlayer& Player) = 0;

	/**
	 * Purge any memory of any objects that are considered externally owned

	 * @param Player 		Movie scene player that is ultimately responsible for destroying the objects
	 */
	virtual void ForgetExternallyOwnedSpawnedObjects(IMovieScenePlayer& Player) = 0;

	/**
	 * Destroy all objects that this spawn register has responsibility for (Ownership != External)
	 *
	 * @param Player 		Movie scene player that is ultimately responsible for destroying the objects
	 */
	virtual void DestroyAllOwnedObjects(IMovieScenePlayer& Player) = 0;

	/**
	 * Destroy all objects that we have ever spawned
	 *
	 * @param Player 		Movie scene player that is ultimately responsible for destroying the objects
	 */
	virtual void DestroyAllObjects(IMovieScenePlayer& Player) = 0;

	/**
	 * Called before a sequence instance is about to update
	 *
	 * @param Instance 		The sequence instance that is about to update
	 * @param Player 		The player that is playing the movie scene
	 */
	virtual void PreUpdateSequenceInstance(FMovieSceneSequenceInstance& Instance, IMovieScenePlayer& Player) { }

	/**
	 * Called after a sequence instance has updated
	 *
	 * @param Instance 		The sequence instance that has updated
	 * @param Player 		The player that is playing the movie scene
	 */
	virtual void PostUpdateSequenceInstance(FMovieSceneSequenceInstance& Instance, IMovieScenePlayer& Player) { }
};


class FNullMovieSceneSpawnRegister : public IMovieSceneSpawnRegister
{
public:
	// These 2 functions ensure because they should never be called for sequences that don't support spawnables
	virtual UObject* SpawnObject(const FGuid& Object, FMovieSceneSequenceInstance&, IMovieScenePlayer&) override { check(false); return nullptr; }
	virtual void DestroySpawnedObject(const FGuid& Object, FMovieSceneSequenceInstance&, IMovieScenePlayer&) override { check(false); }
	virtual void DestroyObjectsOwnedByInstance(FMovieSceneSequenceInstance&, IMovieScenePlayer&) override { }
	virtual void DestroyObjectsSpawnedByInstance(FMovieSceneSequenceInstance&, IMovieScenePlayer&) override { }
	virtual void DestroyAllOwnedObjects(IMovieScenePlayer&) override { }
	virtual void ForgetExternallyOwnedSpawnedObjects(IMovieScenePlayer& Player) override { }
	virtual void DestroyAllObjects(IMovieScenePlayer&) override { }
};

/**
 *	Helper key type for mapping a guid and sequence instance to a specific value
 */
struct FMovieSceneSpawnRegisterKey
{
	FMovieSceneSpawnRegisterKey(const FGuid& InBindingId, FMovieSceneSequenceInstance& InSequenceInstance)
		: BindingId(InBindingId)
		, SequenceInstance(InSequenceInstance.AsShared())
	{
	}

	bool operator==(const FMovieSceneSpawnRegisterKey& Other) const
	{
		return BindingId == Other.BindingId && SequenceInstance == Other.SequenceInstance;
	}
	
	friend uint32 GetTypeHash(const FMovieSceneSpawnRegisterKey& Key)
	{
		return HashCombine(GetTypeHash(Key.BindingId), GetTypeHash(Key.SequenceInstance));
	}

	/** BindingId of the object binding */
	FGuid BindingId;

	/** Sequence instance that spawned the object */
	TWeakPtr<FMovieSceneSequenceInstance> SequenceInstance;
};