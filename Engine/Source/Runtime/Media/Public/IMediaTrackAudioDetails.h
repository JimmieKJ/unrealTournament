// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Interface for audio track details.
 */
class IMediaTrackAudioDetails
{
public:

	/**
	 * Gets the number of audio channels.
	 *
	 * @return Number of channels.
	 */
	virtual uint32 GetNumChannels() const = 0;

	/**
	 * Gets the number of audio samples per second.
	 *
	 * @return Samples per second.
	 */
	virtual uint32 GetSamplesPerSecond() const = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaTrackAudioDetails() { }
};
