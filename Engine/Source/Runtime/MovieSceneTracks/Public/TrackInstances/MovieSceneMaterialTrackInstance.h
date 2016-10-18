// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"
#include "ObjectKey.h"

class FTrackInstancePropertyBindings;
class UMovieSceneMaterialTrack;
class UMovieSceneComponentMaterialTrack;

/**
 * A movie scene track instance for material tracks.
 */
class MOVIESCENETRACKS_API FMovieSceneMaterialTrackInstance
	: public IMovieSceneTrackInstance
{
public:
	FMovieSceneMaterialTrackInstance( UMovieSceneMaterialTrack& InMaterialTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override;
	virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override;

protected:
	/**
	 * Gets the material for a specific runtime object.
	 */
	virtual UMaterialInterface* GetMaterialForObject( UObject* Object ) const = 0;

	/**
	 * Sets the material for a specific runtime object.
	 */
	virtual void SetMaterialForObject( UObject* Object, UMaterialInterface* Material) = 0;

private:
	/** Track that is being instanced */
	UMovieSceneMaterialTrack* MaterialTrack;

	/** Map from runtime object to original material */
	TMap<FObjectKey, TWeakObjectPtr<UMaterialInterface>> RuntimeObjectToOriginalMaterialMap;

	/** Map from dynamic material to original material */
	TMap<FObjectKey, TWeakObjectPtr<UMaterialInterface>> DynamicMaterialToOriginalMaterialMap;

	/** A list of dynamic material instances which need to be updated. */
	TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> DynamicMaterialInstances;
};

/** A material track instance specialized for component materials. */
class FMovieSceneComponentMaterialTrackInstance : public FMovieSceneMaterialTrackInstance
{
public:
	/** Creates a new track instance with a specific component material track. */
	FMovieSceneComponentMaterialTrackInstance( UMovieSceneComponentMaterialTrack& InMaterialTrack );

protected:
	/** FMovieSceneMaterialTrackInstance interface. */
	virtual UMaterialInterface* GetMaterialForObject( UObject* Object ) const override;
	virtual void SetMaterialForObject( UObject* Object, UMaterialInterface* Material ) override;

private:
	/** The index of the material on the associated component. */
	int32 MaterialIndex;
};
