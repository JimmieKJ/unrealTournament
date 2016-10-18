// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieScenePlayer.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.generated.h"

class FLevelSequenceSpawnRegister;
class FMovieSceneSequenceInstance;
class ULevel;
class UMovieSceneBindings;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLevelSequencePlayerEvent);

USTRUCT(BlueprintType)
struct FLevelSequenceSnapshotSettings
{
	GENERATED_BODY()

	FLevelSequenceSnapshotSettings()
		: ZeroPadAmount(4), FrameRate(30)
	{}

	FLevelSequenceSnapshotSettings(int32 InZeroPadAmount, float InFrameRate)
		: ZeroPadAmount(InZeroPadAmount), FrameRate(InFrameRate)
	{}

	/** Zero pad frames */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	uint8 ZeroPadAmount;

	/** Playback framerate */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	float FrameRate;
};

/**
 * Frame snapshot information for a level sequence
 */
USTRUCT(BlueprintType)
struct FLevelSequencePlayerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FText MasterName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	float MasterTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FText CurrentShotName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	float CurrentShotLocalTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FLevelSequenceSnapshotSettings Settings;
};

/**
 * Settings for the level sequence player actor.
 */
USTRUCT(BlueprintType)
struct FLevelSequencePlaybackSettings
{
	FLevelSequencePlaybackSettings()
		: LoopCount(0)
		, PlayRate(1.f)
	{ }

	GENERATED_BODY()

	/** Number of times to loop playback. -1 for infinite, else the number of times to loop before stopping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback", meta=(UIMin=1, DisplayName="Loop"))
	int32 LoopCount;

	/** The rate at which to playback the animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback", meta=(Units=Multiplier))
	float PlayRate;
};

/**
 * ULevelSequencePlayer is used to actually "play" an level sequence asset at runtime.
 *
 * This class keeps track of playback state and provides functions for manipulating
 * an level sequence while its playing.
 */
UCLASS(BlueprintType)
class LEVELSEQUENCE_API ULevelSequencePlayer
	: public UObject
	, public IMovieScenePlayer
{
public:
	ULevelSequencePlayer(const FObjectInitializer&);

	GENERATED_BODY()

	/**
	 * Initialize the player.
	 *
	 * @param InLevelSequence The level sequence to play.
	 * @param InWorld The world that the animation is played in.
	 * @param Settings The desired playback settings
	 */
	void Initialize(ULevelSequence* InLevelSequence, UWorld* InWorld, const FLevelSequencePlaybackSettings& Settings);

public:

	/**
	 * Create a new level sequence player.
	 *
	 * @param WorldContextObject Context object from which to retrieve a UWorld.
	 * @param LevelSequence The level sequence to play.
	 * @param Settings The desired playback settings
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", meta=(WorldContext="WorldContextObject"))
	static ULevelSequencePlayer* CreateLevelSequencePlayer(UObject* WorldContextObject, ULevelSequence* LevelSequence, FLevelSequencePlaybackSettings Settings);

	/** Start playback forwards from the current time cursor position, using the current play rate. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void Play();

	/** Reverse playback. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void PlayReverse();

	/** Changes the direction of playback (go in reverse if it was going forward, or vice versa) */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void ChangePlaybackDirection();

	/**
	 * Start playback from the current time cursor position, looping the specified number of times.
	 * @param NumLoops - The number of loops to play. -1 indicates infinite looping.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void PlayLooping(int32 NumLoops = -1);

	/** Start playback from the current time cursor position, using the current play rate. Does not update the animation until next tick. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void StartPlayingNextTick();
	
	/** Pause playback. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void Pause();
	
	/** Stop playback. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void Stop();

	/** Get the current playback position */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	float GetPlaybackPosition() const;

	/**
	 * Set the current playback position
	 * @param NewPlaybackPosition - The new playback position to set.
	 * If the animation is currently playing, it will continue to do so from the new position
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetPlaybackPosition(float NewPlaybackPosition);

	/** Check whether the sequence is actively playing. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	bool IsPlaying() const;

	/** Get the playback length of the sequence */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	float GetLength() const;

	/** Get the playback rate of this player. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	float GetPlayRate() const;

	/**
	 * Set the playback rate of this player. Negative values will play the animation in reverse.
	 * @param PlayRate - The new rate of playback for the animation.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetPlayRate(float PlayRate);

	/**
	 * Sets the range in time to be played back by this player, overriding the default range stored in the asset
	 *
	 * @param	NewStartTime	The new starting time for playback
	 * @param	NewEndTime		The new ending time for playback.  Must be larger than the start time.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetPlaybackRange( const float NewStartTime, const float NewEndTime );

	/** Get the offset within the level sequence to start playing */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	float GetPlaybackStart() const { return StartTime; }

	/** Get the offset within the level sequence to finish playing */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	float GetPlaybackEnd() const { return EndTime; }

	/** Set the settings used to capture snapshots with */
	void SetSnapshotSettings(const FLevelSequenceSnapshotSettings& InSettings) { SnapshotSettings = InSettings; }

public:

	/**
	 * Access the level sequence this player is playing
	 * @return the level sequence currently assigned to this player
	 */
	ULevelSequence* GetLevelSequence() const { return LevelSequence; }
	
protected:

	// IMovieScenePlayer interface
	virtual void GetRuntimeObjects(TSharedRef<FMovieSceneSequenceInstance> MovieSceneInstance, const FGuid& ObjectHandle, TArray<TWeakObjectPtr<UObject>>& OutObjects) const override;
	virtual void UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut) override;
	virtual void SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) override;
	virtual void GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const override;
	virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const override;
	virtual void SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus) override;
	virtual void AddOrUpdateMovieSceneInstance(UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneSequenceInstance> InstanceToAdd) override;
	virtual void RemoveMovieSceneInstance(UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneSequenceInstance> InstanceToRemove) override;
	virtual TSharedRef<FMovieSceneSequenceInstance> GetRootMovieSceneSequenceInstance() const override;
	virtual IMovieSceneSpawnRegister& GetSpawnRegister() override;
	virtual UObject* GetPlaybackContext() const override;
	virtual TArray<UObject*> GetEventContexts() const override;

public:

	/** Populate the specified array with any given event contexts for the specified world */
	static void GetEventContexts(UWorld& InWorld, TArray<UObject*>& OutContexts);

	void Update(const float DeltaSeconds);

	/** Take a snapshot of the current state of this player */
	void TakeFrameSnapshot(FLevelSequencePlayerSnapshot& OutSnapshot) const;

private:

	/** Internal play function. */
	void PlayInternal();

	/** Update the movie scene instance from the specified previous position, to the specified time position. */
	void UpdateMovieSceneInstance(float CurrentPosition, float PreviousPosition);

	/** Update the time cursor position and handle stopping & looping. */
	void UpdateTimeCursorPosition(float NewPosition);

	/** Add tick prerequisites so that the level sequence actor ticks before all the actors it controls */
	void SetTickPrerequisites(bool bAddTickPrerequisites);
	
	/** Returns whether playback should be stopped or looped because it has gone out of the playback bounds. */
	bool ShouldStopOrLoop(float NewPosition);

private:

	/** The level sequence to play. */
	UPROPERTY(transient)
	ULevelSequence* LevelSequence;

	/** The level sequence player. */
	UPROPERTY(transient)
	ULevelSequencePlayer* CurrentPlayer;

	/** Whether we're currently playing. If false, then sequence playback is paused or was never started. */
	UPROPERTY()
	bool bIsPlaying;

	/** Whether we're currently playing in reverse. */
	UPROPERTY()
	bool bReversePlayback;

	/** True where we're waiting for the first update of the sequence after calling StartPlayingNextTick. */
	bool bPendingFirstUpdate;

	/** The current time cursor position within the sequence (in seconds) */
	UPROPERTY()
	float TimeCursorPosition;

	/** The time cursor position in the previous update. */
	float LastCursorPosition;

	/** Time time at which to start playing the sequence (defaults to the lower bound of the sequence's play range) */
	float StartTime;

	/** Time time at which to end playing the sequence (defaults to the upper bound of the sequence's play range) */
	float EndTime;

	/** Specific playback settings for the animation. */
	UPROPERTY()
	FLevelSequencePlaybackSettings PlaybackSettings;

	/** The number of times we have looped in the current playback */
	int32 CurrentNumLoops;

	/** Whether this player has cleaned up the level sequence after it has stopped playing or not */
	bool bHasCleanedUpSequence;

private:

	/** The root movie scene instance to update when playing. */
	TSharedPtr<FMovieSceneSequenceInstance> RootMovieSceneInstance;

	/** The world this player will spawn actors in, if needed */
	TWeakObjectPtr<UWorld> World;

	/** Register responsible for managing spawned objects */
	TSharedPtr<FLevelSequenceSpawnRegister> SpawnRegister;

	/** The last view target to reset to when updating camera cuts to null */
	TWeakObjectPtr<AActor> LastViewTarget;

protected:

	/** How to take snapshots */
	FLevelSequenceSnapshotSettings SnapshotSettings;

public:

	/** An event that is broadcast each time this level sequence player is updated */
	DECLARE_EVENT_ThreeParams( ULevelSequencePlayer, FOnLevelSequencePlayerUpdated, const ULevelSequencePlayer&, float /*current time*/, float /*previous time*/ );
	FOnLevelSequencePlayerUpdated& OnSequenceUpdated() const { return OnLevelSequencePlayerUpdate; }

	/** Event triggered when the level sequence player is played */
	UPROPERTY(BlueprintAssignable, Category="Game|Cinematic")
	FOnLevelSequencePlayerEvent OnPlay;

	/** Event triggered when the level sequence player is stopped */
	UPROPERTY(BlueprintAssignable, Category="Game|Cinematic")
	FOnLevelSequencePlayerEvent OnStop;

	/** Event triggered when the level sequence player is paused */
	UPROPERTY(BlueprintAssignable, Category="Game|Cinematic")
	FOnLevelSequencePlayerEvent OnPause;

private:

	/** The event that will be broadcast every time the sequence is updated */
	mutable FOnLevelSequencePlayerUpdated OnLevelSequencePlayerUpdate;
};
