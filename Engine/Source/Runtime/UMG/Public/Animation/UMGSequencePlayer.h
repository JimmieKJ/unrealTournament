// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieScenePlayer.h"
#include "UMGSequencePlayer.generated.h"

class UWidgetAnimation;
class UMovieSceneBindings;

UCLASS(transient)
class UMG_API UUMGSequencePlayer : public UObject, public IMovieScenePlayer
{
	GENERATED_UCLASS_BODY()

public:
	void InitSequencePlayer( const UWidgetAnimation& InAnimation, UUserWidget& UserWidget );

	/** Updates the running movie */
	void Tick( float DeltaTime );

	/** Begins playing or restarts an animation */
	void Play( float StartAtTime, int32 InNumLoopsToPlay, EUMGSequencePlayMode::Type InPlayMode);

	/** Stops a running animation and resets time */
	void Stop();

	/** Pauses a running animation */
	void Pause();

	/** Gets the current time position in the player (in seconds). */
	double GetTimeCursorPosition() const { return TimeCursorPosition; }

	/** @return The current animation being played */
	const UWidgetAnimation* GetAnimation() const { return Animation; }

	/** IMovieScenePlayer interface */
	virtual void GetRuntimeObjects( TSharedRef<FMovieSceneInstance> MovieSceneInstance, const FGuid& ObjectHandle, TArray< UObject* >& OutObjects ) const override;
	virtual void UpdatePreviewViewports(UObject* ObjectToViewThrough) const override {}
	virtual void AddMovieSceneInstance( class UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneInstance> InstanceToAdd ) override {}
	virtual void RemoveMovieSceneInstance( class UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneInstance> InstanceToRemove ) override {}
	virtual TSharedRef<FMovieSceneInstance> GetRootMovieSceneInstance() const override { return RootMovieSceneInstance.ToSharedRef(); }
	virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const override;

	DECLARE_EVENT_OneParam(UUMGSequencePlayer, FOnSequenceFinishedPlaying, UUMGSequencePlayer&);
	FOnSequenceFinishedPlaying& OnSequenceFinishedPlaying() { return OnSequenceFinishedPlayingEvent; }

private:
	/** Animation being played */
	UPROPERTY()
	const UWidgetAnimation* Animation;

	/** Bindings to actual live objects */
	UPROPERTY()
	UMovieSceneBindings* RuntimeBindings;

	/** The root movie scene instance to update when playing. */
	TSharedPtr<class FMovieSceneInstance> RootMovieSceneInstance;

	/** Time range of the animation */
	TRange<float> TimeRange;

	/** The current time cursor position within the sequence (in seconds) */
	double TimeCursorPosition;

	/** Status of the player (e.g play, stopped) */
	EMovieScenePlayerStatus::Type PlayerStatus;

	/** Delegate to call when a sequence has finished playing */
	FOnSequenceFinishedPlaying OnSequenceFinishedPlayingEvent;

	/** The number of times to loop the animation playback */
	int32 NumLoopsToPlay;

	/** The number of loops completed since the last call to Play() */
	int32 NumLoopsCompleted;

	/** The current playback mode. */
	EUMGSequencePlayMode::Type PlayMode;

	/** True if the animation is playing forward, otherwise false and it's playing in reverse. */
	bool bIsPlayingForward;
};
