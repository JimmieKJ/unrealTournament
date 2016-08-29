// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Enumerates directions for playback rate information.
 */
enum class EMediaPlaybackDirections
{
	/** Forward playback rates. */
	Forward,

	/** Reverse playback rates. */
	Reverse
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
 * Enumerates possible states of media playback.
 */
enum class EMediaState
{
	/** Media has been closed and cannot be played again. */
	Closed,

	/** Unrecoverable error occurred during playback. */
	Error,

	/** Playback has been paused, but can be resumed. */
	Paused,

	/** Media is currently playing. */
	Playing,

	/** Media is loading or buffering. */
	Preparing,

	/** Playback has been stopped, but can be restarted. */
	Stopped
};


/**
 * Interface for controlling media playback.
 *
 * @see IMediaOutput, IMediaTracks
 */
class IMediaControls
{
public:

	/**
	 * Get the media's duration.
	 *
	 * @return A time span representing the duration.
	 * @see GetTime
	 */
	virtual FTimespan GetDuration() const = 0;

	/**
	 * Get the nominal playback rate, i.e. 1.0 for real time.
	 *
	 * @return Playback rate.
	 * @see Pause, Play, SetRate
	 */
	virtual float GetRate() const = 0;

	/**
	 * Get the state of the media.
	 *
	 * @return Media state.
	 */
	virtual EMediaState GetState() const = 0;

	/**
	 * Get the range of supported playback rates in the specified playback direction.
	 *
	 * @param Direction The playback direction.
	 * @param Unthinned Whether the rates are for unthinned playback.
	 * @return The range of supported rates.
	 * @see SupportsRate, SetRate
	 */
	virtual TRange<float> GetSupportedRates(EMediaPlaybackDirections Direction, bool Unthinned) const = 0;

	/**
	 * Get the player's current playback time.
	 *
	 * @return Playback time.
	 * @see Seek
	 */
	virtual FTimespan GetTime() const = 0;

	/**
	 * Check whether playback is currently looping.
	 *
	 * @return true if playback is looping, false otherwise.
	 * @see SetLooping
	 */
	virtual bool IsLooping() const = 0;

	/**
	 * Change the media's playback time.
	 *
	 * @param Time The playback time to set.
	 * @return true on success, false otherwise.
	 * @see GetTime
	 */
	virtual bool Seek(const FTimespan& Time) = 0;

	/**
	 * Set whether playback should be looping.
	 *
	 * @param Looping Enables or disables looping.
	 * @see IsLooping
	 */
	virtual bool SetLooping(bool Looping) = 0;

	/**
	 * Set the current playback rate.
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
	virtual bool SetRate(float Rate) = 0;

	/**
	 * Check whether the specified playback rate is supported.
	 *
	 * @param Rate The rate to check (can be negative for reverse play).
	 * @param Unthinned Whether no frames should be dropped at the given rate.
	 * @return true if the rate is supported, false otherwise.
	 * @see GetSupportedRates, SetRate, SupportsScrubbing, SupportsSeeking
	 */
	virtual bool SupportsRate(float Rate, bool Unthinned) const = 0;

	/**
	 * Check whether scrubbing is supported.
	 *
	 * Scrubbing is the ability to decode video frames while seeking in a media item at a playback rate of 0.0.
	 *
	 * @return true if scrubbing is supported, false otherwise.
	 * @see SupportsRate, SupportsSeeking
	 */
	virtual bool SupportsScrubbing() const = 0;

	/**
	 * Check whether the currently loaded media can jump to certain times.
	 *
	 * @return true if seeking is supported, false otherwise.
	 * @see SupportsRate, SupportsScrubbing
	 */
	virtual bool SupportsSeeking() const = 0;

public:

	/**
	 * Pause media playback.
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
	 * Start media playback at the default rate of 1.0.
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
	 * Change the playback time of the media by a relative offset in the given direction.
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
			SeekTime = GetDuration() - TimeOffset;
			break;

		case EMediaSeekDirection::Forward:
			SeekTime = GetTime() + TimeOffset;
			break;
		}

		return Seek(SeekTime);
	}

public:

	/** Virtual destructor. */
	virtual ~IMediaControls() { }
};
