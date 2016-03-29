// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"
#include "ObjectKey.h"

class FTrackInstancePropertyBindings;
class UMovieSceneParticleParameterTrack;

/**
 * A movie scene track instance for material tracks.
 */
class FMovieSceneParticleParameterTrackInstance
	: public IMovieSceneTrackInstance
{
private:

	struct FInitialParameterValues
	{
		TMap<FName, float> ScalarNameToValueMap;
		TMap<FName, FVector> VectorNameToValueMap;
		TMap<FName, FLinearColor> ColorNameToValueMap;
	};

public:
	FMovieSceneParticleParameterTrackInstance( UMovieSceneParticleParameterTrack& InParticleParameterTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override;
	virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override;

private:

	/** Track that is being instanced */
	UMovieSceneParticleParameterTrack* ParticleParameterTrack;

	/** The initial values of instance parameters for the objects controlled by this track. */
	TMap<FObjectKey, TSharedPtr<FInitialParameterValues>> ObjectToInitialParameterValuesMap;

	/** The ParticleSystemComponents to be animated on update. */
	TArray<UParticleSystemComponent*> ParticleSystemComponents;
};
