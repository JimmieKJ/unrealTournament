// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneSpawnRegister.h"
#include "MovieSceneSequenceInstance.h"
#include "MovieSceneSpawnable.h"

/** Movie scene spawn register that knows how to handle spawning objects (actors) for a level sequence  */
class LEVELSEQUENCE_API FLevelSequenceSpawnRegister : public IMovieSceneSpawnRegister
{
public:
	FLevelSequenceSpawnRegister()
	{
		CurrentlyUpdatingSequenceCount = 0;
	}

	/** ~ IMovieSceneSpawnRegister interface */
	virtual UObject* SpawnObject(const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance, IMovieScenePlayer&) override;
	virtual void DestroySpawnedObject(const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance, IMovieScenePlayer&) override;
	virtual void DestroyObjectsByPredicate(IMovieScenePlayer& Player, const TFunctionRef<bool(const FGuid&, ESpawnOwnership, FMovieSceneSequenceInstance&)>& Predicate) override;
	virtual void ForgetExternallyOwnedSpawnedObjects(IMovieScenePlayer& Player) override;
	virtual void PreUpdateSequenceInstance(FMovieSceneSequenceInstance& Instance, IMovieScenePlayer&) override;
	virtual void PostUpdateSequenceInstance(FMovieSceneSequenceInstance& Instance, IMovieScenePlayer&) override;

protected:

	/** Structure holding information pertaining to a spawned object */
	struct FSpawnedObject
	{
		FSpawnedObject(const FGuid& InGuid, UObject& InObject, ESpawnOwnership InOwnership)
			: Guid(InGuid)
			, Object(&InObject)
			, Ownership(InOwnership)
		{}

		/** The ID of the sequencer object binding that this object relates to */
		FGuid Guid;

		/** The object that has been spawned */
		TWeakObjectPtr<UObject> Object;

		/** What level of ownership this object was spawned with */
		ESpawnOwnership Ownership;
	};

protected:
	/** Called right before a spawned object with the specified ID and sequence instance is destroyed */
	virtual void PreDestroyObject(UObject& Object, const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance);

	/** Implementation function that destroys the specified object if it is an actor */
	void DestroySpawnedObject(UObject& Object);

	/** Destroy any objects that pass the specified predicate */
	void DestroyObjects(IMovieScenePlayer& Player, TFunctionRef<bool(FMovieSceneSequenceInstance&, FSpawnedObject&)> Pred);
	
	
	/** Sets of sequence instances that are active for the current update, and were active for the previous update */
	TSet<TWeakPtr<FMovieSceneSequenceInstance>> ActiveInstances, PreviouslyActiveInstances;

	/** The number of currently updating sequence instances. Used to keep track of the outermost sequence instance that triggered an update */
	int32 CurrentlyUpdatingSequenceCount;

	/** Register of spawned objects */
	TMap<FMovieSceneSpawnRegisterKey, FSpawnedObject> Register;
};