// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaInfo.h"
#include "IMediaStream.h"


class IMediaStream;
class IMediaAudioTrack;
class IMediaCaptionTrack;
class IMediaVideoTrack;


/**
 * Enumerates media player related events.
 */
enum class EMediaEvent
{
	/** Unknown event. */
	Unknown,

	/** The current media source has been closed. */
	MediaClosed,

	/** A new media source has been opened. */
	MediaOpened,

	/** A media source failed to open. */
	MediaOpenFailed,

	/** The end of the media (or beginning if playing in reverse) has been reached. */
	PlaybackEndReached,

	/** Playback has been resumed. */
	PlaybackResumed,

	/** Playback has been suspended. */
	PlaybackSuspended,

	/** Media tracks have changed. */
	TracksChanged
};


/**
 * Enumerates directions for seeking in media.
 */
enum class EMediaSeekDirection
{
	/** Seek backwards from current position. */
	Backward,

	/** Seek from the beginning of the media. */
	Beginning,

	/** Seek from the end of the media. */
	End,

	/** Seek forward from current position. */
	Forward
};


/**
 * Interface for media players.
 *
 * @see IMediaStream
 */
class IMediaPlayer
{
public:

	/**
	 * Closes a previously opened media.
	 *
	 * If no media has been opened, this function has no effect.
	 *
	 * @see IsReady, Open
	 */
	virtual void Close() = 0;

	/**
	 * Get the media's audio tracks.
	 *
	 * @return Collection of audio tracks.
	 * @see GetCaptionTracks, GetVideoTracks
	 */
	virtual const TArray<TSharedRef<IMediaAudioTrack, ESPMode::ThreadSafe>>& GetAudioTracks() const = 0;

	/**
	 * Get the media's caption tracks.
	 *
	 * @return Collection of caption tracks.
	 * @see GetAudioTracks, GetVideoTracks
	 */
	virtual const TArray<TSharedRef<IMediaCaptionTrack, ESPMode::ThreadSafe>>& GetCaptionTracks() const = 0;

	/**
	 * Gets the last error that occurred during media loading or playback.
	 *
	 * @return The error string, or an empty string if no error occurred.
	 */
//	virtual FString GetLastError() const = 0;

	/**
	 * Gets information about the currently loaded media.
	 *
	 * @return Interface to media information.
	 */
	virtual const IMediaInfo& GetMediaInfo() const = 0;

	/**
	 * Gets the nominal playback rate, i.e. 1.0 for real time.
	 *
	 * @return Playback rate.
	 * @see Pause, Play, SetRate
	 */
	virtual float GetRate() const = 0;

	/**
	 * Gets the player's current playback time.
	 *
	 * @return Playback time.
	 * @see Seek
	 */
	virtual FTimespan GetTime() const = 0;

	/**
	 * Get the media's video tracks.
	 *
	 * @return Collection of video tracks.
	 * @see GetAudioTracks, GetCaptionTracks
	 */
	virtual const TArray<TSharedRef<IMediaVideoTrack, ESPMode::ThreadSafe>>& GetVideoTracks() const = 0;

	/**
	 * Checks whether playback is currently looping.
	 *
	 * @return true if playback is looping, false otherwise.
	 * @see SetLooping
	 */
	virtual bool IsLooping() const = 0;

	/**
	 * Checks whether media playback is currently paused.
	 *
	 * @return true if playback is paused, false otherwise.
	 * @see IsPlaying, IsStopped, Pause, Play, Stop
	 */
	virtual bool IsPaused() const = 0;

	/**
	 * Checks whether media playback has currently in progress.
	 *
	 * @return true if playback is in progress, false otherwise.
	 * @see IsPaused, IsStopped, Pause, Play, Stop
	 */
	virtual bool IsPlaying() const = 0;

	/**
	 * Checks whether this player is ready for playback.
	 *
	 * A media player is considered ready if some media has been opened
	 * successfully using the Open method and no error occurred during
	 * loading or playback.
	 *
	 * @return true if ready, false otherwise.
	 * @see Close, Open
	 */
	virtual bool IsReady() const = 0;

	/**
	 * Opens a media from a URL, possibly asynchronously.
	 *
	 * The media may not necessarily be opened after this function succeeds,
	 * because opening may happen asynchronously. Subscribe to the OnOpened
	 * OnOpenFailed events to detect when the media finished or failed to
	 * open. These events are only triggered if Open returns true.
	 *
	 * @param Url The URL of the media to open (file name or web address).
	 * @return true if the media is being opened, false otherwise.
	 * @see Close, IsReady, OnOpen, OnOpenFailed
	 */
	virtual bool Open(const FString& Url) = 0;

	/**
	 * Opens a media from a file or memory archive, possibly asynchronously.
	 *
	 * The media may not necessarily be opened after this function succeeds,
	 * because opening may happen asynchronously. Subscribe to the OnOpened
	 * OnOpenFailed events to detect when the media finished or failed to
	 * open. These events are only triggered if Open returns true.
	 *
	 * @param Archive The archive holding the media data.
	 * @param OriginalUrl The original URL of the media that was loaded into the buffer.
	 * @return true if the media is being opened, false otherwise.
	 * @see Close, IsReady, OnOpen, OnOpenFailed
	 */
	virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl) = 0;

	/**
	 * Changes the media's playback time.
	 *
	 * @param Time The playback time to set.
	 * @return true on success, false otherwise.
	 * @see GetTime
	 */
	virtual bool Seek(const FTimespan& Time) = 0;

	/**
	 * Sets whether playback should be looping.
	 *
	 * @param Looping Enables or disables looping.
	 * @see IsLooping
	 */
	virtual bool SetLooping(bool Looping) = 0;

	/**
	 * Sets the current playback rate.
	 *
	 * A playback rate of 1.0 will play the media normally at real-time.
	 * A rate of 0.0 corresponds to pausing playback. A negative rate, if
	 * supported, plays the media in reverse, and a rate larger than 1.0
	 * fast forwards playback.
	 *
	 * @param Rate The playback rate to set.
	 * @return true on success, false otherwise.
	 * @see GetRate, Pause, Play
	 */
	virtual bool SetRate( float Rate ) = 0;

public:

	/** Gets an event delegate that is invoked when some interesting event occurred. */
	DECLARE_EVENT_OneParam(IMediaPlayer, FOnMediaEvent, EMediaEvent /*Event*/)
	virtual FOnMediaEvent& OnMediaEvent() = 0;

public:

	/**
	 * Pauses media playback.
	 *
	 * This is the same as setting the playback rate to 0.0.
	 *
	 * @return true if the media is being paused, false otherwise.
	 * @see Play, Stop
	 */
	FORCEINLINE bool Pause()
	{
		return SetRate(0.0f);
	}

	/**
	 * Starts media playback at the default rate of 1.0.
	 *
	 * This is the same as setting the playback rate to 1.0.
	 *
	 * @return true if playback is starting, false otherwise.
	 * @see Pause, Stop
	 */
	FORCEINLINE bool Play()
	{
		return SetRate(1.0f);
	}

	/**
	 * Changes the playback time of the media by a relative offset in the given direction.
	 *
	 * @param TimeOffset The offset to apply to the time.
	 * @param Direction The direction to seek in.
	 * @return true on success, false otherwise.
	 * @see GetDuration, GetTime
	 */
	bool Seek(const FTimespan& TimeOffset, EMediaSeekDirection Direction)
	{
		FTimespan SeekTime;

		switch (Direction)
		{
		case EMediaSeekDirection::Backward:
			SeekTime = GetTime() - TimeOffset;
			break;

		case EMediaSeekDirection::Beginning:
			SeekTime = TimeOffset;
			break;

		case EMediaSeekDirection::End:
			SeekTime = GetMediaInfo().GetDuration() - TimeOffset;
			break;

		case EMediaSeekDirection::Forward:
			SeekTime = GetTime() + TimeOffset;
			break;
		}

		return Seek(SeekTime);
	}

public:

	/** Virtual destructor. */
	virtual ~IMediaPlayer() { }
};


/** Type definition for shared pointers to instances of IMediaPlayer. */
typedef TSharedPtr<IMediaPlayer> IMediaPlayerPtr;

/** Type definition for shared references to instances of IMediaPlayer. */
typedef TSharedRef<IMediaPlayer> IMediaPlayerRef;
