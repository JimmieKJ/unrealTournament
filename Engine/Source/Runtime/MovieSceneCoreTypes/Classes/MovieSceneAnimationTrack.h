// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneAnimationTrack.generated.h"


/**
 * Handles animation of skeletal mesh actors
 */
UCLASS( MinimalAPI )
class UMovieSceneAnimationTrack : public UMovieSceneTrack
{
	GENERATED_UCLASS_BODY()

public:
	/** UMovieSceneTrack interface */
	virtual FName GetTrackName() const override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;
	virtual void RemoveAllAnimationData() override;
	virtual bool HasSection( UMovieSceneSection* Section ) const override;
	virtual void RemoveSection( UMovieSceneSection* Section ) override;
	virtual bool IsEmpty() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual bool SupportsMultipleRows() const override { return true; }
	virtual TArray<UMovieSceneSection*> GetAllSections() const override;

	/** Adds a new animation to this track */
	virtual void AddNewAnimation(float KeyTime, class UAnimSequence* AnimSequence);

	/** Gets the animation section at a certain time, or NULL if there is none */
	class UMovieSceneSection* GetAnimSectionAtTime(float Time);

private:
	/** List of all animation sections */
	UPROPERTY()
	TArray<UMovieSceneSection*> AnimationSections;
};
