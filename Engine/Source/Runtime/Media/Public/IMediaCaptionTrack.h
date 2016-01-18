// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class IMediaStream;


/**
 * Interface for caption tracks.
 */
class IMediaCaptionTrack
{
public:

	/**
	 * Get the underlying media stream.
	 *
	 * @return Media stream object.
	 */
	virtual IMediaStream& GetStream() = 0;

public:

	/** Virtual destructor. */
	~IMediaCaptionTrack() { }
};


/** Type definition for shared pointers to instances of IMediaCaptionTrack. */
typedef TSharedPtr<IMediaCaptionTrack, ESPMode::ThreadSafe> IMediaCaptionTrackPtr;

/** Type definition for shared references to instances of IMediaCaptionTrack. */
typedef TSharedRef<IMediaCaptionTrack, ESPMode::ThreadSafe> IMediaCaptionTrackRef;
