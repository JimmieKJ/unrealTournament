// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSkeletalAnimationTrack.generated.h"


class UMovieSceneSection;


/**
 * Handles animation of skeletal mesh actors
 */
UCLASS( MinimalAPI )
class UMovieSceneSkeletalAnimationTrack
	: public UMovieSceneTrack
{
	GENERATED_UCLASS_BODY()

public:

	/** Adds a new animation to this track */
	virtual void AddNewAnimation(float KeyTime, class UAnimSequence* AnimSequence);

	/** Gets the animation section at a certain time, or NULL if there is none */
	UMovieSceneSection* GetAnimSectionAtTime(float Time);

public:

	// UMovieSceneTrack interface

	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;
	virtual void RemoveAllAnimationData() override;
	virtual bool HasSection(const UMovieSceneSection& Section ) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool IsEmpty() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual bool SupportsMultipleRows() const override { return true; }
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDisplayName() const override;
#endif

private:

	/** List of all animation sections */
	UPROPERTY()
	TArray<UMovieSceneSection*> AnimationSections;
};
