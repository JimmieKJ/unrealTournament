// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneAudioTrack.generated.h"

namespace AudioTrackConstants
{
	const float ScrubDuration = 0.050f;
	const FName UniqueTrackName("MasterAudio");
}

/**
 * Handles manipulation of audio
 */
UCLASS( MinimalAPI )
class UMovieSceneAudioTrack : public UMovieSceneTrack
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

	/** Adds a new sound cue to the audio */
	virtual void AddNewSound(class USoundBase* Sound, float Time);

	/** @return The audio sections on this track */
	const TArray<UMovieSceneSection*>& GetAudioSections() const { return AudioSections; }

	/** @return true if this is a master audio track */
	bool IsAMasterTrack() const;
private:
	/** List of all master audio sections */
	UPROPERTY()
	TArray<UMovieSceneSection*> AudioSections;
};
