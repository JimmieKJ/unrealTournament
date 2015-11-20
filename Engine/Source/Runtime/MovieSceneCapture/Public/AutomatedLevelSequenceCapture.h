// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

	/** Set the level sequence actor that we are to record */
	void SetLevelSequenceActor(ALevelSequenceActor* InActor);

	/** The amount of time to wait before playback and capture start. Useful for allowing Post Processing effects to settle down before capturing the animation. */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings, AdvancedDisplay, meta=(Units=Seconds, ClampMin=0))
	float PrerollAmount;

	/** Whether to stage the sequence before capturing it. This will set the sequence to its first frame for Preroll Amount seconds to allow Post Processing effects to settle. */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings, AdvancedDisplay)
	bool bStageSequence;

public:
	// UMovieSceneCapture interface
	virtual void Initialize(TWeakPtr<FSceneViewport> InViewport) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

private:

	/** Called when this capture has stopped */
	virtual void OnCaptureStopped() override;

	/** Called when the level sequence has updated the world */
	void SequenceUpdated(const ULevelSequencePlayer& Player, float CurrentTime, float PreviousTime);

	/** Delegate binding for the above callback */
	FDelegateHandle OnPlayerUpdatedBinding;

private:

	virtual void Tick(float DeltaSeconds) override;

	/** A level sequence asset to playback at runtime - used where the level sequence does not already exist in the world. */
	UPROPERTY()
	FStringAssetReference LevelSequenceAsset;

	/** The pre-existing level sequence actor to use for capture that specifies playback settings */
	UPROPERTY()
	TLazyObjectPtr<ALevelSequenceActor> LevelSequenceActor;

	/** Copy of the level sequence ID from Level Sequence. Required because JSON serialization exports the path of the object, rather that its GUID */
	UPROPERTY()
	FGuid LevelSequenceActorId;

	/** Transient amount of time that has passed since the level was started. When >= Settings.Preroll, playback will start  */
	TOptional<float> PrerollTime;

#endif
};

