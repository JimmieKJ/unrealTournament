// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"


class UAudioComponent;
class UMovieSceneAudioTrack;


/**
 * Instance of a UMovieSceneAudioTrack
 */
class FMovieSceneAudioTrackInstance
	: public IMovieSceneTrackInstance
{
public:

	FMovieSceneAudioTrackInstance( UMovieSceneAudioTrack& InAudioTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}
	virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}

private:

	/** Plays the sound of the given section at the given time */
	void PlaySound(class UMovieSceneAudioSection* AudioSection, TWeakObjectPtr<UAudioComponent> Component, float Time);
	
	/** Stops all components for the given row index */
	void StopSound(int32 RowIndex);

	/** Stops all components from playing */
	void StopAllSounds();

	/** Gets the audio component component for the actor and row index, creating it if necessary */
	TWeakObjectPtr<UAudioComponent> GetAudioComponent(IMovieScenePlayer& Player, AActor* Actor, int32 RowIndex);

	/** Utility function for getting actors from objects array */
	TArray<AActor*> GetRuntimeActors(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects) const;

private:

	/** Track that is being instanced */
	UMovieSceneAudioTrack* AudioTrack;

	/** Audio components to play our audio tracks with, one per row per actor */
	TArray< TMap<AActor*, TWeakObjectPtr<UAudioComponent> > > PlaybackAudioComponents;
};
