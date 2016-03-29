// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSubTrackInstance.h"

class UMovieSceneCameraCutTrack;


/**
 * Instance of a UMovieSceneCameraCutTrack
 */
class FMovieSceneCameraCutTrackInstance
	: public IMovieSceneTrackInstance
{
public:

	FMovieSceneCameraCutTrackInstance(UMovieSceneCameraCutTrack& InCameraCutTrack);

public:

	// IMovieSceneTrackInstance interface

	virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override { }
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;

protected:

	/**
	 * Acquire the camera object for the specified camera identifier.
	 *
	 * @param CameraCutIndex The index of the CameraCut section that needs the camera.
	 * @param CameraGuid The unique identifier of the camera object to get.
	 * @param SequenceInstance The sequence instance that owns this track instance.
	 * @return The camera object, or nullptr if not found.
	 */
	UObject* AcquireCameraForCameraCut(int32 CameraCutIndex, const FGuid& CameraGuid, FMovieSceneSequenceInstance& SequenceInstance, const IMovieScenePlayer& Player);

private:
	
	/** Runtime camera objects.  One for each CameraCut.  Must be the same number of entries as sections */
	TArray<TWeakObjectPtr<UObject>> CachedCameraObjects;

	/** Current camera object we are looking through.  Used to determine when making a new cut */
	TWeakObjectPtr<UObject> LastCameraObject;

	/** Track that is being instanced */
	TWeakObjectPtr<UMovieSceneCameraCutTrack> CameraCutTrack;
};
