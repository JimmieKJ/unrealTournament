// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSequence.generated.h"


class FMovieSceneSequenceInstance;
class UMovieScene;
class UObject;


/**
 * Abstract base class for movie scene animations (C++ version).
 */
UCLASS(MinimalAPI)
class UMovieSceneSequence
	: public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Called when Sequencer has created an object binding for a possessable object
	 * 
	 * @param ObjectId The guid used to map to the possessable object.  Note the guid can be bound to multiple objects at once
	 * @param PossessedObject The runtime object which was possessed.
	 * @see UnbindPossessableObjects
	 */
	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject) PURE_VIRTUAL(UMovieSceneSequence::BindPossessableObject,);
	
	/**
	 * Check whether the given object can be possessed by this animation.
	 *
	 * @param Object The object to check.
	 * @return true if the object can be possessed, false otherwise.
	 */
	virtual bool CanPossessObject(UObject& Object) const PURE_VIRTUAL(UMovieSceneSequence::CanPossessObject, return false;);

	/**
	 * Gets the possessed or spawned object for the specified identifier.
	 *
	 * @param ObjectId The unique identifier of the object.
	 * @return The object, or nullptr if not found.
	 */
	virtual UObject* FindObject(const FGuid& ObjectId) const PURE_VIRTUAL(UMovieSceneSequence::FindObject, return nullptr;);

	/**
	 * Find the identifier for a possessed or spawned object.
	 *
	 * @param The object to get the identifier for.
	 * @return The object identifier, or an invalid GUID if not found.
	 */
	virtual FGuid FindObjectId( UObject& Object) const PURE_VIRTUAL(UMovieSceneSequence::FindGuidForObject, return FGuid(););

	/**
	 * Get the movie scene that controls this animation.
	 *
	 * The returned movie scene represents the root movie scene.
	 * It may contain additional child movie scenes.
	 *
	 * @return The movie scene.
	 */
	virtual UMovieScene* GetMovieScene() const PURE_VIRTUAL(UMovieSceneSequence::GetMovieScene(), return nullptr;);

	/**
	 * Get the logical parent object for the supplied object (not necessarily its outer).
	 *
	 * @param Object The object whose parent to get.
	 * @return The parent object, or nullptr if the object has no logical parent.
	 */
	virtual UObject* GetParentObject(UObject* Object) const PURE_VIRTUAL(UMovieSceneSequence::GetParentObject(), return nullptr;);

	/**
	 * Whether objects can be spawned at run-time.
	 *
	 * @return true if objects can be spawned by sequencer, false if only existing objects can be possessed.
	 */
	virtual bool AllowsSpawnableObjects() const { return false; }
	
	/**
	 * Spawn an object relating to the specified object ID
	 * 
	 * @param ObjectId The ID of the object to spawn
	 * @return The newly spawned or previously-spawned object, or nullptr
	 */
	virtual UObject* SpawnObject(const FGuid& ObjectId) { return nullptr; }

	/**
	 * Destroy a previously spawned object relating to the specified object ID
	 * 
	 * @param ObjectId The ID of the object to destroy
	 */
	virtual void DestroySpawnedObject(const FGuid& ObjectId) { }

	/**
	 * Destroy a previously spawned object relating to the specified GUID
	 *
	 * @return true if the object was destroyed, false otherwise
	 */
	virtual void DestroyAllSpawnedObjects() {}

	/**
	 * Unbinds all possessable objects from the provided GUID.
	 *
	 * @param ObjectId The guid bound to possessable objects that should be removed.
	 * @see BindPossessableObject
	 */
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) PURE_VIRTUAL(UMovieSceneSequence::UnbindPossessableObjects,);

public:

#if WITH_EDITOR
	/**
	 * 
	 * Tries to get a display name for the binding represented by the guid.
	 *
	 * @param ObjectGuid The guid for the object binding.
	 * @param DisplayName The display name for the object binding.
	 * @returns true if DisplayName has been set to a valid display name, otherwise false.
	 */
	virtual bool TryGetObjectDisplayName(const FGuid& ObjectId, FText& OutDisplayName) const PURE_VIRTUAL(UMovieSceneSequence::TryGetObjectDisplayName, return false;);

	/**
	 * Get the display name for this movie sequence
	 */
	virtual FText GetDisplayName() const { return FText::FromName(GetFName()); }
#endif
};
