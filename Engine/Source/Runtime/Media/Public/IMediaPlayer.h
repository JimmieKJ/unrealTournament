// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaInfo.h"
#include "IMediaTrack.h"


class IMediaTrack;


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
 * @see IMediaTrack
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
	 * Gets access to the media's audio, video and other tracks.
	 *
	 * @return Media tracks interface.
	 * @see GetFirstTrack, GetTrack
	 */
	virtual const TArray<TSharedRef<IMediaTrack, ESPMode::ThreadSafe>>& GetTracks() const = 0;

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
	 * Opens a media from a file on disk.
	 *
	 * @param Url The URL of the media to open (file name or web address).
	 * @return true if the media was opened successfully, false otherwise.
	 * @see Close, IsReady
	 */
	virtual bool Open( const FString& Url ) = 0;

	/**
	 * Opens a media from a buffer.
	 *
	 * @param Buffer The buffer holding the media data.
	 * @param OriginalUrl The original URL of the media that was loaded into the buffer.
	 * @return true if the media was opened, false otherwise.
	 * @see Close, IsReady
	 */
	virtual bool Open( const TSharedRef<TArray<uint8>>& Buffer, const FString& OriginalUrl ) = 0;

	/**
	 * Changes the media's playback time.
	 *
	 * @param Time The playback time to set.
	 * @return true on success, false otherwise.
	 * @see GetTime
	 */
	virtual bool Seek( const FTimespan& Time ) = 0;

	/**
	 * Sets whether playback should be looping.
	 *
	 * @param Looping Enables or disables looping.
	 * @see IsLooping
	 */
	virtual bool SetLooping( bool Looping ) = 0;

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

	/** Gets an event delegate that is invoked when media has been closed. */
	DECLARE_EVENT(IMediaPlayer, FOnMediaClosed)
	virtual FOnMediaClosed& OnClosed() = 0;

	/** Gets an event delegate that is invoked when media has been opened. */
	DECLARE_EVENT_OneParam(IMediaPlayer, FOnMediaOpened, FString /*OpenedUrl*/)
	virtual FOnMediaOpened& OnOpened() = 0;

public:

	/**
	 * Gets the first media track matching the specified type.
	 *
	 * @param TrackType The expected type of the track, i.e. audio or video.
	 * @return The first matching track, nullptr otherwise.
	 * @see GetTrack, GetTracks
	 */
	TSharedPtr<IMediaTrack, ESPMode::ThreadSafe> GetFirstTrack( EMediaTrackTypes TrackType )
	{
		for (const TSharedRef<IMediaTrack, ESPMode::ThreadSafe>& Track : GetTracks())
		{
			if (Track->GetType() == TrackType)
			{
				return Track;
			}
		}

		return nullptr;
	}

	/**
	 * Gets the media track with the specified index and type.
	 *
	 * @param TrackIndex The index of the track to get.
	 * @param TrackType The expected type of the track, i.e. audio or video.
	 * @return The track if it exists, nullptr otherwise.
	 * @see GetFirstTrack, GetTracks
	 */
	TSharedPtr<IMediaTrack, ESPMode::ThreadSafe> GetTrack( int32 TrackIndex, EMediaTrackTypes TrackType )
	{
		if (GetTracks().IsValidIndex(TrackIndex))
		{
			TSharedPtr<IMediaTrack, ESPMode::ThreadSafe> Track = GetTracks()[TrackIndex];

			if (Track->GetType() == TrackType)
			{
				return Track;
			}
		}

		return nullptr;
	}

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
	bool Seek( const FTimespan& TimeOffset, EMediaSeekDirection Direction )
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
