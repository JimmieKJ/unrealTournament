// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

	/**
	 * Bind this level sequence to an arbitrary context. For now we only support binding to a UWorld
	 *
	 * @param InContext The context to use for object binding resolution.
	 */
	void BindToContext(UObject* InContext);

public:

	// UMovieSceneSequence interface
	
	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject) override;
	virtual bool CanPossessObject(UObject& Object) const override;
	virtual UObject* FindObject(const FGuid& ObjectId) const override;
	virtual FGuid FindObjectId(UObject& Object) const override;
	virtual UMovieScene* GetMovieScene() const override;
	virtual UObject* GetParentObject(UObject* Object) const override;
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) override;
	virtual bool AllowsSpawnableObjects() const override;
	virtual UObject* SpawnObject(const FGuid& ObjectId) override;
	virtual void DestroySpawnedObject(const FGuid& ObjectId) override;
	virtual void DestroyAllSpawnedObjects() override;

#if WITH_EDITOR
	virtual bool TryGetObjectDisplayName(const FGuid& ObjectId, FText& OutDisplayName) const override;
#endif

	virtual bool Rename(const TCHAR* NewName = nullptr, UObject* NewOuter = nullptr, ERenameFlags Flags = REN_None) override;
	
private:

	/** Collection of possessed objects. */
	UPROPERTY()
	FLevelSequenceObjectReferenceMap ObjectReferences;

	/** Deprecated property housing old possessed object bindings */
	UPROPERTY()
	TMap<FString, FLevelSequenceObject> PossessedObjects_DEPRECATED;

private:

	/** A context to use for resolving object bindings */
	TWeakObjectPtr<UObject> ResolutionContext;

	/** A transient map of cached object bindings */
	mutable TMap<FGuid, TWeakObjectPtr<UObject>> CachedObjectBindings;

	/** A map of object ID -> spawned object */
	TMap<FGuid, TWeakObjectPtr<UObject>> SpawnedObjects;
};