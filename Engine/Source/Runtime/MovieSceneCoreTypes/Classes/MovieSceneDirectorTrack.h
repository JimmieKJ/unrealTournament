// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneDirectorTrack.generated.h"

/**
 * Handles manipulation of shot properties in a movie scene
 */
UCLASS( MinimalAPI )
class UMovieSceneDirectorTrack : public UMovieSceneTrack
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

	/** Adds a new shot at the specified time */
	virtual void AddNewShot(FGuid CameraHandle, float Time);

	/** @return The shot sections on this track */
	const TArray<UMovieSceneSection*>& GetShotSections() const { return ShotSections; }

private:
	/** List of all shot sections */
	UPROPERTY()
	TArray<UMovieSceneSection*> ShotSections;
};
