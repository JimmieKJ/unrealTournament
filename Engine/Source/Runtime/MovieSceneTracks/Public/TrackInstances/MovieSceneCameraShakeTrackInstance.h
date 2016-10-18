// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"

class UMovieSceneCameraShakeTrack;
class UMovieSceneCameraShakeSection;

struct FMovieSceneCameraShakeSectionInstanceData
{
	TWeakObjectPtr<UCameraShake> CameraShakeInst;
	FTransform AdditiveOffset;		// AdditiveCamToBaseCam
	float AdditiveFOVOffset;
	float PostProcessingBlendWeight;
	FPostProcessSettings PostProcessingSettings;
};

/**
* Instance of a UMovieSceneCameraShakeTrack
*/
class FMovieSceneCameraShakeTrackInstance
	: public IMovieSceneTrackInstance
{
public:

	FMovieSceneCameraShakeTrackInstance(UMovieSceneCameraShakeTrack& InShakeTrack);
	virtual ~FMovieSceneCameraShakeTrackInstance();

	/** IMovieSceneTrackInstance interface */
	virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}
	virtual void RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}
	virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}

	virtual EMovieSceneUpdatePass HasUpdatePasses() override { return (EMovieSceneUpdatePass)(MSUP_PreUpdate | MSUP_Update); }

private:
	/** Track that is being instanced */
	UMovieSceneCameraShakeTrack* CameraShakeTrack;

	/** Keep track of the currently playing montage */
	TWeakObjectPtr<UCameraShake> CurrentlyPlayingCameraShakeInst;

	TWeakObjectPtr<ACameraActor> TempCameraActor;

	TMap<UMovieSceneCameraShakeSection*, FMovieSceneCameraShakeSectionInstanceData> SectionInstanceDataMap;

	ACameraActor* GetTempCameraActor(IMovieScenePlayer& InMovieScenePlayer);
};
