// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	 * @param Context Optional context required to bind the specified object (for instance, a parent spawnable object)
	 * @see UnbindPossessableObjects
	 */
	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context) PURE_VIRTUAL(UMovieSceneSequence::BindPossessableObject,);
	
	/**
	 * Check whether the given object can be possessed by this animation.
	 *
	 * @param Object The object to check.
	 * @return true if the object can be possessed, false otherwise.
	 */
	virtual bool CanPossessObject(UObject& Object) const PURE_VIRTUAL(UMovieSceneSequence::CanPossessObject, return false;);

	/**
	 * Finds the possessed for the specified identifier.
	 *
	 * @param ObjectId The unique identifier of the object.
	 * @param Context Optional context to use to find the required object (for instance, a parent spawnable object)
	 * @return The object, or nullptr if not found.
	 */
	virtual UObject* FindPossessableObject(const FGuid& ObjectId, UObject* Context) const PURE_VIRTUAL(UMovieSceneSequence::FindPossessedObject, return nullptr;);

	/**
	 * Finds an ID for the specified the object
	 *
	 * @param Object The object to find an id for
	 * @return The object's ID, or an invalid guid if not found
	 */
	virtual FGuid FindPossessableObjectId(UObject& Object) const PURE_VIRTUAL(UMovieSceneSequence::FindPossessableObjectId, return FGuid(););

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
	 * Unbinds all possessable objects from the provided GUID.
	 *
	 * @param ObjectId The guid bound to possessable objects that should be removed.
	 * @see BindPossessableObject
	 */
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) PURE_VIRTUAL(UMovieSceneSequence::UnbindPossessableObjects,);

	/**
	 * Create a spawnable object template from the specified source object
	 *
	 * @param InSourceObject The source object to copy
	 * @param ObjectName The name of the object
	 * @return A new object template of the specified name
	 */
	virtual UObject* MakeSpawnableTemplateFromInstance(UObject& InSourceObject, FName ObjectName) { return nullptr; }

public:

#if WITH_EDITOR

	/**
	 * Get the display name for this movie sequence
	 */
	virtual FText GetDisplayName() const { return FText::FromName(GetFName()); }
#endif
};
