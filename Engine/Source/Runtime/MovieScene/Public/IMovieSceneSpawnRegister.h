// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSequenceInstance.h"
#include "MovieSceneSpawnable.h"
#include "ValueOrError.h"

 enum class ESpawnOwnership : uint8;

class IMovieScenePlayer;
class FMovieSceneSequenceInstance;
class UMovieScene;
struct FMovieSceneSpawnable;

/** Struct used for defining a new spawnable type */
struct FNewSpawnable
{
	FNewSpawnable() : ObjectTemplate(nullptr) {}
	FNewSpawnable(UObject* InObjectTemplate, FString InName) : ObjectTemplate(InObjectTemplate), Name(MoveTemp(InName)) {}

	/** The archetype object template that defines the spawnable */
	UObject* ObjectTemplate;

	/** The desired name of the new spawnable */
	FString Name;
};

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
	 * Destroy spawned objects using a custom predicate
	 *
	 * @param Player 		Movie scene player that is ultimately responsible for destroying the objects
	 * @param Predicate		Predicate used for testing whether an object should be destroyed. Returns true for destruction, false to skip.
	 */
	virtual void DestroyObjectsByPredicate(IMovieScenePlayer& Player, const TFunctionRef<bool(const FGuid&, ESpawnOwnership, FMovieSceneSequenceInstance&)>& Predicate) = 0;

	/**
	 * Purge any memory of any objects that are considered externally owned

	 * @param Player 		Movie scene player that is ultimately responsible for destroying the objects
	 */
	virtual void ForgetExternallyOwnedSpawnedObjects(IMovieScenePlayer& Player) = 0;

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

#if WITH_EDITOR
	/**
	 * Create a new spawnable type from the given source object
	 *
	 * @param SourceObject		The source object to create the spawnable from
	 * @param OwnerMovieScene	The owner movie scene that this spawnable type should reside in
	 * @return the new spawnable type, or error
	 */
	virtual TValueOrError<FNewSpawnable, FText> CreateNewSpawnableType(UObject& SourceObject, UMovieScene& OwnerMovieScene) { return MakeError(NSLOCTEXT("SpawnRegister", "NotSupported", "Not supported")); }

	/**
	 * Called to test whether an expired object can be destroyed or not
	 *
	 * @param Object 			ID of the object to destroy
	 * @param Ownership 		The ownership of the object
	 * @param SequenceInstance	The sequence instance that spawned this object
	 * @return true if it can be destroyed, false otherwise
	 */
	virtual bool CanDestroyExpiredObject(const FGuid& ObjectId, ESpawnOwnership Ownership, FMovieSceneSequenceInstance& SequenceInstance) { return true; }

#endif

public:

	/**
	 * Called to indiscriminately clean up any spawned objects
	 */
	void CleanUp(IMovieScenePlayer& Player)
	{
		DestroyObjectsByPredicate(Player, [&](const FGuid&, ESpawnOwnership, FMovieSceneSequenceInstance&){
			return true;
		});
	}

	/**
	 * Called to clean up any non-externally owned spawnables that were spawned from the specified instance
	 */
	void CleanUpSequence(FMovieSceneSequenceInstance& Instance, IMovieScenePlayer& Player)
	{
		DestroyObjectsByPredicate(Player, [&](const FGuid&, ESpawnOwnership, FMovieSceneSequenceInstance& SequenceInstance){
			return &Instance == &SequenceInstance;
		});
	}

	/**
	 * Called when the current time has moved beyond the specified sequence's play range
	 */
	void OnSequenceExpired(FMovieSceneSequenceInstance& Instance, IMovieScenePlayer& Player)
	{
		DestroyObjectsByPredicate(Player, [&](const FGuid& ObjectId, ESpawnOwnership Ownership, FMovieSceneSequenceInstance& SequenceInstance){
			return	(Ownership == ESpawnOwnership::InnerSequence)
				&&	(&Instance == &SequenceInstance)
#if WITH_EDITOR
				&&	CanDestroyExpiredObject(ObjectId, Ownership, SequenceInstance)
#endif
				;
		});
	}

#if WITH_EDITOR
	/**
	 * Called when the current time has moved beyond the master sequence's play range
	 */
	void OnMasterSequenceExpired(IMovieScenePlayer& Player)
	{
		DestroyObjectsByPredicate(Player, [&](const FGuid& ObjectId, ESpawnOwnership Ownership, FMovieSceneSequenceInstance& SequenceInstance){
			return Ownership != ESpawnOwnership::External && CanDestroyExpiredObject(ObjectId, Ownership, SequenceInstance);
		});
	}
#endif
};

class FNullMovieSceneSpawnRegister : public IMovieSceneSpawnRegister
{
public:
	virtual UObject* SpawnObject(const FGuid& Object, FMovieSceneSequenceInstance&, IMovieScenePlayer&) override { check(false); return nullptr; }
	virtual void DestroySpawnedObject(const FGuid& Object, FMovieSceneSequenceInstance&, IMovieScenePlayer&) override { }
	virtual void DestroyObjectsByPredicate(IMovieScenePlayer& Player, const TFunctionRef<bool(const FGuid&, ESpawnOwnership, FMovieSceneSequenceInstance&)>& Predicate) { }
	virtual void ForgetExternallyOwnedSpawnedObjects(IMovieScenePlayer& Player) override { }
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