// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FMovieSceneInstance;
class UMovieScene;

class ISequencerObjectBindingManager
{
public:
	virtual ~ISequencerObjectBindingManager() {}
	
	/**
	 * @return true to allow objects to be spawned by sequencer, false if only existing objects can be possessed
	 */
	virtual bool AllowsSpawnableObjects() const = 0;
	
	/**
	 * Finds an existing guid for a bound object
	 *
	 * @param MovieScene The movie scene that may contain the binding
	 * @param The object to get the guid for
	 * @return The object guid or an invalid guid if not found
	 */
	virtual FGuid FindGuidForObject( const UMovieScene& MovieScene, UObject& Object ) const = 0;
	
	/**
	 * Called to spawn or destroy objects for a movie instance
	 *
	 * @param MovieSceneInstance	The instance to spawn or destroy objects for
	 * @param bDestroyAll			If true, destroy all spawned objects for the instance, if false only destroy unused objects
	 */
	virtual void SpawnOrDestroyObjectsForInstance( TSharedRef<FMovieSceneInstance> MovieSceneInstance, bool bDestroyAll ) = 0;
	
	virtual void DestroyAllSpawnedObjects() = 0;
	
	virtual bool CanPossessObject( UObject& Object ) const = 0;
	
	/**
	 * Called when Sequencer has created an object binding for a possessable object
	 * 
	 * @param PossessableGuid	The guid used to map to the possessable object.  Note the guid can be bound to multiple objects at once
	 * @param PossessedObject	The runtime object which was possessed.
	 */
	virtual void BindPossessableObject( const FGuid& PossessableGuid, UObject& PossessedObject ) = 0;
	
	/**
	 * Unbinds all possessable objects from the provided guid
	 *
	 * @param PossessableGuid	The guid bound to possessable objects that should be removed
	 */
	virtual void UnbindPossessableObjects( const FGuid& PossessableGuid ) = 0;
	
	virtual void GetRuntimeObjects( const TSharedRef<FMovieSceneInstance>& MovieSceneInstance, const FGuid& ObjectGuid, TArray<UObject*>& OutRuntimeObjects ) const = 0;

	/**
	 * 
	 * Tries to get a display name for the binding represented by the guid.
	 *
	 * @param ObjectGuid	the guid for the object binding.
	 * @param DisplayName	the display name for the object binding.
	 * @returns true if DisplayName has been set to a valid display name, otherwise false.
	 */
	virtual bool TryGetObjectBindingDisplayName( const FGuid& ObjectGuid, FText& DisplayName ) const = 0;
};
