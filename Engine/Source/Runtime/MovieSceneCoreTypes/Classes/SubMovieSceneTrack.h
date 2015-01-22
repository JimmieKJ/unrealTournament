// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneTrack.h"
#include "SubMovieSceneTrack.generated.h"

/**
 * Handles manipulation of float properties in a movie scene
 */
UCLASS( MinimalAPI )
class USubMovieSceneTrack : public UMovieSceneTrack
{
	GENERATED_UCLASS_BODY()
public:
	/** UMovieSceneTrack interface */
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FName GetTrackName() const override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;
	virtual TArray<UMovieSceneSection*> GetAllSections() const override;
	virtual void RemoveSection( UMovieSceneSection* Section ) override;
	virtual bool IsEmpty() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual bool HasSection( UMovieSceneSection* Section ) const override;

	/**
	 * Adds a movie scene section
	 *
	 * @param SubMovieScene	The movie scene to add
	 * @param Time	The time to add the section at
	 */
	virtual void AddMovieSceneSection( UMovieScene* SubMovieScene, float Time );
private:
	/** All movie scene sections.  */
	UPROPERTY()
	TArray<class UMovieSceneSection*> SubMovieSceneSections;
};
