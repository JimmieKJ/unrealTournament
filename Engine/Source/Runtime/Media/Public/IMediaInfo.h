// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
 * Interface for information about media loaded into a media player.
 */
class IMediaInfo
{
public:

	/**
	 * Gets the media's duration.
	 *
	 * @return A time span representing the duration.
	 * @see GetTime
	 */
	virtual FTimespan GetDuration() const = 0;

	/**
	 * Gets the range of supported playback rates in the specified playback direction.
	 *
	 * @param Direction The playback direction.
	 * @param Unthinned Whether the rates are for unthinned playback.
	 * @return The range of supported rates.
	 * @see SupportsRate
	 */
	virtual TRange<float> GetSupportedRates( EMediaPlaybackDirections Direction, bool Unthinned ) const = 0;

	/**
	 * Gets the URL of the currently loaded media.
	 *
	 * @return Media URL.
	 */
	virtual FString GetUrl() const = 0;

	/**
	 * Checks whether the specified playback rate is supported.
	 *
	 * @param Rate The rate to check (can be negative for reverse play).
	 * @param Unthinned Whether no frames should be dropped at the given rate.
	 * @return true if the rate is supported, false otherwise.
	 * @see GetSupportedRates, SupportsScrubbing, SupportsSeeking
	 */
	virtual bool SupportsRate( float Rate, bool Unthinned ) const = 0;

	/**
	 * Checks whether scrubbing is supported.
	 *
	 * Scrubbing is the ability to decode video frames while seeking in a media item at a playback rate of 0.0.
	 *
	 * @return true if scrubbing is supported, false otherwise.
	 * @see SupportsRate, SupportsSeeking
	 */
	virtual bool SupportsScrubbing() const = 0;

	/**
	 * Checks whether the currently loaded media can jump to certain times.
	 *
	 * @return true if seeking is supported, false otherwise.
	 * @see SupportsRate, SupportsScrubbing
	 */
	virtual bool SupportsSeeking() const = 0;
};
