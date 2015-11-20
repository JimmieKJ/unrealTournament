// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class IMediaStream;


/**
 * Interface for audio tracks.
 */
class IMediaAudioTrack
{
public:

	/**
	 * Gets the number of audio channels.
	 *
	 * @return Number of channels.
	 * @see GetSamplesPerSecond
	 */
	virtual uint32 GetNumChannels() const = 0;

	/**
	 * Gets the number of audio samples per second.
	 *
	 * @return Samples per second.
	 * @see GetNumChannels
	 */
	virtual uint32 GetSamplesPerSecond() const = 0;

	/**
	 * Get the underlying media stream.
	 *
	 * @return Media stream object.
	 */
	virtual IMediaStream& GetStream() = 0;

public:

	/** Virtual destructor. */
	~IMediaAudioTrack() { }
};


/** Type definition for shared pointers to instances of IMediaAudioTrack. */
typedef TSharedPtr<IMediaAudioTrack, ESPMode::ThreadSafe> IMediaAudioTrackPtr;

/** Type definition for shared references to instances of IMediaAudioTrack. */
typedef TSharedRef<IMediaAudioTrack, ESPMode::ThreadSafe> IMediaAudioTrackRef;
