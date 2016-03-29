// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneCapture.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "AutomatedLevelSequenceCapture.generated.h"

class ALevelSequenceActor;

UCLASS(config=EditorSettings)
class MOVIESCENECAPTURE_API UAutomatedLevelSequenceCapture : public UMovieSceneCapture
{
public:
	UAutomatedLevelSequenceCapture(const FObjectInitializer&);

	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	/** Set the level sequence asset that we are to record. We will spawn a new actor at runtime for this asset for playback. */
	void SetLevelSequenceAsset(FString InAssetPath);

	/** When enabled, the StartFrame setting will override the default starting frame number */
	UPROPERTY(config, EditAnywhere, Category=Animation, AdvancedDisplay)
	bool bUseCustomStartFrame;

	/** Frame number to start capturing.  The frame number range depends on whether the bUseRelativeFrameNumbers option is enabled. */
	UPROPERTY(config, EditAnywhere, Category=Animation, AdvancedDisplay, meta=(EditCondition="bUseCustomStartFrame"))
	int32 StartFrame;

	/** When enabled, the EndFrame setting will override the default ending frame number */
	UPROPERTY(config, EditAnywhere, Category=Animation, AdvancedDisplay)
	bool bUseCustomEndFrame;

	/** Frame number to end capturing.  The frame number range depends on whether the bUseRelativeFrameNumbers option is enabled. */
	UPROPERTY(config, EditAnywhere, Category=Animation, AdvancedDisplay, meta=(EditCondition="bUseCustomEndFrame"))
	int32 EndFrame;

	/** The number of extra frames to play before the sequence's start frame, to "warm up" the animation.  This is useful if your
	    animation contains particles or other runtime effects that are spawned into the scene earlier than your capture start frame */
	UPROPERTY(config, EditAnywhere, Category=Animation, AdvancedDisplay)
	int32 WarmUpFrameCount;

	/** The number of seconds to wait (in real-time) before we start playing back the warm up frames.  Useful for allowing post processing effects to settle down before capturing the animation. */
	UPROPERTY(config, EditAnywhere, Category=Animation, AdvancedDisplay, meta=(Units=Seconds, ClampMin=0))
	float DelayBeforeWarmUp;

public:
	// UMovieSceneCapture interface
	virtual void Initialize(TSharedPtr<FSceneViewport> InViewport, int32 PIEInstance = -1) override;

private:

	/** Called when the level sequence has updated the world */
	void SequenceUpdated(const ULevelSequencePlayer& Player, float CurrentTime, float PreviousTime);

	/** Called to set up the player's playback range */
	void SetupFrameRange();

	/** Delegate binding for the above callback */
	FDelegateHandle OnPlayerUpdatedBinding;

private:

	virtual void Tick(float DeltaSeconds) override;

	/** A level sequence asset to playback at runtime - used where the level sequence does not already exist in the world. */
	UPROPERTY()
	FStringAssetReference LevelSequenceAsset;

	/** The pre-existing level sequence actor to use for capture that specifies playback settings */
	UPROPERTY()
	TWeakObjectPtr<ALevelSequenceActor> LevelSequenceActor;

	/** Which state we're in right now */
	enum class ELevelSequenceCaptureState
	{
		Setup,
		DelayBeforeWarmUp,
		ReadyToWarmUp,
		WarmingUp,
		FinishedWarmUp
	} CaptureState;

	/** Time left to wait before capturing */
	float RemainingDelaySeconds;

	/** The number of warm up frames left before we actually start saving out images */
	int32 RemainingWarmUpFrames;

	/** The delta of the last sequence updat */
	float LastSequenceUpdateDelta;
	
#endif
};

