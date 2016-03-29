// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneNameableTrack.h"
#include "MovieSceneAudioTrack.generated.h"


class UMovieSceneSection;
class USoundBase;


namespace AudioTrackConstants
{
	const float ScrubDuration = 0.050f;
}


/**
 * Handles manipulation of audio.
 */
UCLASS(MinimalAPI)
class UMovieSceneAudioTrack
	: public UMovieSceneNameableTrack
{
	GENERATED_UCLASS_BODY()

public:

	/** Adds a new sound cue to the audio */
	virtual void AddNewSound(USoundBase* Sound, float Time);

	/** @return The audio sections on this track */
	const TArray<UMovieSceneSection*>& GetAudioSections() const
	{
		return AudioSections;
	}

	/** @return true if this is a master audio track */
	bool IsAMasterTrack() const;

public:

	// UMovieSceneTrack interface

	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;
	virtual void RemoveAllAnimationData() override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool IsEmpty() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual bool SupportsMultipleRows() const override;

private:

	/** List of all master audio sections */
	UPROPERTY()
	TArray<UMovieSceneSection*> AudioSections;
};
