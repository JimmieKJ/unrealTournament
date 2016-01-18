// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaPlayer.generated.h"


class IMediaPlayer;


/** Multicast delegate that is invoked when a media player's media has been closed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMediaPlayerMediaClosed);

/** Multicast delegate that is invoked when a media player's media has been opened. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMediaPlayerMediaOpened, FString, OpenedUrl);

/** Multicast delegate that is invoked when a media player's media has failed to open. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMediaPlayerMediaOpenFailed, FString, FailedUrl);


/**
 * Implements a media player asset that can play movies and other media.
 *
 * This class is represents a media URL along with a corresponding media player
 * for exposing media playback functionality to the Engine and to Blueprints.
 */
UCLASS(BlueprintType, hidecategories=(Object))
class MEDIAASSETS_API UMediaPlayer
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Checks whether media playback can be paused right now.
	 *
	 * Playback can be paused if the media supports pausing and if it is currently playing.
	 *
	 * @return true if pausing playback can be paused, false otherwise.
	 * @see CanPlay, Pause
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool CanPause() const;

	/**
	 * Checks whether media playback can be started right now.
	 *
	 * @return true if playback can be started, false otherwise.
	 * @see CanPause, Play
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool CanPlay() const;

	/**
	 * Close the currently open media, if any.
	 *
	 * @see OnMediaClosed, Open, Pause, Play
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	void Close();

	/**
	 * Gets the media's duration.
	 *
	 * @return A time span representing the duration.
	 * @see GetTime, Seek
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	FTimespan GetDuration() const;

	/**
	 * Gets the media's current playback rate.
	 *
	 * @return The playback rate.
	 * @see SetRate, SupportsRate
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	float GetRate() const;

	/**
	 * Gets the media's current playback time.
	 *
	 * @return Playback time.
	 * @see GetDuration, Seek
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	FTimespan GetTime() const;

	/**
	 * Gets the URL of the currently loaded media, if any.
	 *
	 * @return Media URL, or empty string if no media was loaded.
	 * @see OpenUrl
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	const FString& GetUrl() const;

	/**
	 * Checks whether playback is looping.
	 *
	 * @return true if looping, false otherwise.
	 * @see SetLooping
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool IsLooping() const;

	/**
	 * Checks whether playback is currently paused.
	 *
	 * @return true if playback is paused, false otherwise.
	 * @see CanPause, IsPlaying, IsStopped, Pause
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool IsPaused() const;

	/**
	 * Checks whether playback has started.
	 *
	 * @return true if playback has started, false otherwise.
	 * @see CanPlay, IsPaused, IsStopped, Play
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool IsPlaying() const;

	/**
	 * Checks whether playback has stopped.
	 *
	 * @return true if playback has stopped, false otherwise.
	 * @see IsPaused, IsPlaying, Stop
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool IsStopped() const;

	/**
	 * Opens the specified media URL.
	 *
	 * @param NewUrl The URL to open.
	 * @return true on success, false otherwise.
	 * @see GetUrl, Close
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool OpenUrl( const FString& NewUrl );

	/**
	 * Pauses media playback.
	 *
	 * This is the same as setting the playback rate to 0.0.
	 *
	 * @return true if playback is being paused, false otherwise.
	 * @see CanPause, Close, Play, Rewind, Seek, SetRate
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool Pause();

	/**
	 * Starts media playback.
	 *
	 * This is the same as setting the playback rate to 1.0.
	 *
	 * @return true if playback is starting, false otherwise.
	 * @see CanPlay, Close, Pause, Rewind, Seek, SetRate
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool Play();

	/**
	 * Rewinds the media to the beginning.
	 *
	 * This is the same as seeking to zero time.
	 *
	 * @return true if rewinding, false otherwise.
	 * @see GetTime, Close, Pause, Play, Seek
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool Rewind();

	/**
	 * Seeks to the specified playback time.
	 *
	 * @param InTime The playback time to set.
	 * @return true on success, false otherwise.
	 * @see GetTime, Rewind
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool Seek(const FTimespan& InTime);

	/**
	 * Enables or disables playback looping.
	 *
	 * @param Looping Whether playback should be looped.
	 * @return true on success, false otherwise.
	 * @see IsLooping
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool SetLooping(bool InLooping);

	/**
	 * Changes the media's playback rate.
	 *
	 * @param Rate The playback rate to set.
	 * @return true on success, false otherwise.
	 * @see GetRate, SupportsRate
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool SetRate(float Rate);

	/**
	 * Checks whether the specified playback rate is supported.
	 *
	 * @param Rate The playback rate to check.
	 * @param Unthinned Whether no frames should be dropped at the given rate.
	 * @see SupportsScrubbing, SupportsSeeking
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool SupportsRate(float Rate, bool Unthinned) const;

	/**
	 * Checks whether the currently loaded media supports scrubbing.
	 *
	 * @return true if scrubbing is supported, false otherwise.
	 * @see SupportsRate, SupportsSeeking
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool SupportsScrubbing() const;

	/**
	 * Checks whether the currently loaded media can jump to a certain position.
	 *
	 * @return true if seeking is supported, false otherwise.
	 * @see SupportsRate, SupportsScrubbing
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaPlayer")
	bool SupportsSeeking() const;

public:

	/** Gets an event delegate that is invoked when media has been opened or closed. */
	DECLARE_EVENT(UMediaPlayer, FOnMediaChanged)
	FOnMediaChanged& OnMediaChanged()
	{
		return MediaChangedEvent;
	}

	/** Gets an event delegate that is invoked when media tracks have changed. */
	DECLARE_EVENT(UMediaPlayer, FOnTracksChanged)
	FOnTracksChanged& OnTracksChanged()
	{
		return TracksChangedEvent;
	}

	/** Holds a delegate that is invoked when a media source has been closed. */
	UPROPERTY(BlueprintAssignable, Category="Media|MediaPlayer")
	FOnMediaPlayerMediaClosed OnMediaClosed;

	/** Holds a delegate that is invoked when a media source has been opened. */
	UPROPERTY(BlueprintAssignable, Category="Media|MediaPlayer")
	FOnMediaPlayerMediaOpened OnMediaOpened;

	/** Holds a delegate that is invoked when a media source has failed to open. */
	UPROPERTY(BlueprintAssignable, Category="Media|MediaPlayer")
	FOnMediaPlayerMediaOpenFailed OnMediaOpenFailed;

public:

	/**
	 * Gets the low-level player associated with this object.
	 *
	 * @return The player, or nullptr if no player was created.
	 */
	TSharedPtr<IMediaPlayer> GetPlayer() const
	{
		return Player;
	}

public:

	// UObject overrides.

	virtual void BeginDestroy() override;
	virtual FString GetDesc() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:

	/** Initializes the media player. */
	void InitializePlayer();

protected:

	/** Whether playback should loop when it reaches the end. */
	UPROPERTY(EditAnywhere, Category=Playback)
	uint32 Looping:1;

	/** The path or URL to the media file to be played. */
	UPROPERTY(EditAnywhere, Category=Source)
	FString URL;

private:

	/** Callback for when the player has closed a media source. */
	void HandlePlayerMediaClosed();

	/** Callback for when the player has opened a new media source. */
	void HandlePlayerMediaOpened(FString OpenedUrl);

	/** Callback for when the player has failed to open a new media source. */
	void HandlePlayerMediaOpenFailed(FString FailedUrl);

	/** Callback for when the player's tracks changed. */
	void HandlePlayerTracksChanged();

private:

	/** Holds the URL of the currently loaded media source. */
	FString CurrentUrl;

	/** Holds the low-level player used to play the media source. */
	TSharedPtr<IMediaPlayer> Player;

private:

	/** Holds a delegate that is executed when media has been opened or closed. */
	FOnMediaChanged MediaChangedEvent;

	/** Holds a delegate that is executed when media tracks have changed. */
	FOnTracksChanged TracksChangedEvent;
};
