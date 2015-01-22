// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneParticleTrack.generated.h"


/**
 * Handles triggering of particle emitters
 */
UCLASS( MinimalAPI )
class UMovieSceneParticleTrack : public UMovieSceneTrack
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
	virtual bool SupportsMultipleRows() const override { return false; }
	virtual TArray<UMovieSceneSection*> GetAllSections() const override;
	
	virtual void AddNewParticleSystem(float KeyTime, bool bTrigger);

	virtual TArray<UMovieSceneSection*> GetAllParticleSections() const {return ParticleSections;}

private:
	/** List of all particle sections */
	UPROPERTY()
	TArray<UMovieSceneSection*> ParticleSections;
};
