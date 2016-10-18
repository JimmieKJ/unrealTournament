// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"


class UMovieSceneSkeletalAnimationTrack;
class UAnimSequenceBase;

/**
 * Instance of a UMovieSceneSkeletalAnimationTrack
 */
class FMovieSceneSkeletalAnimationTrackInstance
	: public IMovieSceneTrackInstance
{
public:

	FMovieSceneSkeletalAnimationTrackInstance( UMovieSceneSkeletalAnimationTrack& InAnimationTrack );
	virtual ~FMovieSceneSkeletalAnimationTrackInstance();

	/** IMovieSceneTrackInstance interface */
	virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {}

private:

	void BeginAnimControl(USkeletalMeshComponent* SkeletalMeshComponent);

	bool CanPlayAnimation(USkeletalMeshComponent* SkeletalMeshComponent, UAnimSequenceBase* AnimAssetBase = nullptr) const;

	void SetAnimPosition(USkeletalMeshComponent* SkeletalMeshComponent, FName SlotName, int32 ChannelIndex, UAnimSequenceBase* InAnimSequence, float InPosition, bool bLooping, bool bFireNotifies);

	void FinishAnimControl(USkeletalMeshComponent* SkeletalMeshComponent);

	void PreviewBeginAnimControl(USkeletalMeshComponent* SkeletalMeshComponent);

	void PreviewFinishAnimControl(USkeletalMeshComponent* SkeletalMeshComponent);

	void PreviewSetAnimPosition(USkeletalMeshComponent* SkeletalMeshComponent, FName SlotName, int32 ChannelIndex, UAnimSequenceBase* InAnimSequence, float InPosition, bool bLooping, bool bFireNotifies, float DeltaTime, bool bPlaying, bool bResetDynamics);

	bool ShouldUsePreviewPlayback(class IMovieScenePlayer& Player, UObject* RuntimeObject) const;

private:

	/** Track that is being instanced */
	UMovieSceneSkeletalAnimationTrack* AnimationTrack;

	/** Keep track of the currently playing montage */
	TWeakObjectPtr<UAnimMontage> CurrentlyPlayingMontage;
};
