// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSequence.h"
#include "LevelSequenceObject.h"
#include "LevelSequenceObjectReference.h"
#include "LevelSequence.generated.h"

/**
 * Movie scene animation for Actors.
 */
UCLASS(BlueprintType)
class LEVELSEQUENCE_API ULevelSequence
	: public UMovieSceneSequence
{
	GENERATED_UCLASS_BODY()

public:

	/** Pointer to the movie scene that controls this animation. */
	UPROPERTY()
	UMovieScene* MovieScene;

public:

	/** Initialize this level sequence. */
	void Initialize();

	/** Convert old-style lazy object ptrs to new-style references using the specified context */
	void ConvertPersistentBindingsToDefault(UObject* FixupContext);

public:

	// UMovieSceneSequence interface
	
	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context) override;
	virtual bool CanPossessObject(UObject& Object) const override;
	virtual UObject* FindPossessableObject(const FGuid& ObjectId, UObject* Context) const override;
	virtual FGuid FindPossessableObjectId(UObject& Object) const override;
	virtual UMovieScene* GetMovieScene() const override;
	virtual UObject* GetParentObject(UObject* Object) const override;
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) override;
	virtual bool AllowsSpawnableObjects() const override;
	virtual UObject* MakeSpawnableTemplateFromInstance(UObject& InSourceObject, FName ObjectName) override;
	

	virtual void PostLoad() override;

	/** Bind a posessable object with an explicitly-supplied ObjectReference */
	void BindPossessableObject(const FGuid& ObjectId, const FLevelSequenceObjectReference& ObjectReference);

private:

	/** Collection of possessed objects. */
	UPROPERTY()
	FLevelSequenceObjectReferenceMap ObjectReferences;

	/** Deprecated property housing old possessed object bindings */
	UPROPERTY()
	TMap<FString, FLevelSequenceObject> PossessedObjects_DEPRECATED;
};