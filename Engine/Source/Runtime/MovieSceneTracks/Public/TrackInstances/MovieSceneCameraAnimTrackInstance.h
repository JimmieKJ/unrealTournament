// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"

class UMovieSceneCameraAnimTrack;
class UMovieSceneCameraAnimSection;

struct FMovieSceneCameraAnimSectionInstanceData
{
	TWeakObjectPtr<UCameraAnimInst> CameraAnimInst;
	FTransform AdditiveOffset;		// AdditiveCamToBaseCam
	float AdditiveFOVOffset;
	float PostProcessingBlendWeight;
	FPostProcessSettings PostProcessingSettings;
};

/**
 * Instance of a UMovieSceneCameraAnimTrack
 */
class FMovieSceneCameraAnimTrackInstance
	: public IMovieSceneTrackInstance
{
public:

	FMovieSceneCameraAnimTrackInstance(UMovieSceneCameraAnimTrack& InAnimationTrack);
	virtual ~FMovieSceneCameraAnimTrackInstance();

	/** IMovieSceneTrackInstance interface */
	virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}
	virtual void RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}
	virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}

	virtual EMovieSceneUpdatePass HasUpdatePasses() override { return (EMovieSceneUpdatePass)(MSUP_PreUpdate | MSUP_Update); }

private:
	/** Track that is being instanced */
	UMovieSceneCameraAnimTrack* CameraAnimTrack;

	/** Keep track of the currently playing montage */
	TWeakObjectPtr<UCameraAnimInst> CurrentlyPlayingCameraAnimInst;

	TWeakObjectPtr<ACameraActor> TempCameraActor;

	ACameraActor* GetTempCameraActor(IMovieScenePlayer& InMovieScenePlayer);

	TMap<UMovieSceneCameraAnimSection*, FMovieSceneCameraAnimSectionInstanceData> SectionInstanceDataMap;
};
