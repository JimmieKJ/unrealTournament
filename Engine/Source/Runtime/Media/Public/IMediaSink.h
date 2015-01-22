// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Interface for recipients of media samples.
 */
class IMediaSink
{
public:

	/**
	 * Receives the given media sample data.
	 *
	 * Implementers of this method must be thread-safe, because it may get called from any thread.
	 *
	 * @param Buffer The buffer holding the sample data.
	 * @param BufferSize The size of the buffer (in bytes).
	 * @param Duration The duration of the sample data (MaxValue means no duration).
	 * @param Time The time of the sample relative to the beginning of the media.
	 */
	virtual void ProcessMediaSample( const void* Buffer, uint32 BufferSize, FTimespan Duration, FTimespan Time ) = 0;

public:

	/** Virtual destructor. */
	~IMediaSink() { }
};


/** Type definition for shared pointers to instances of IMediaSink. */
typedef TSharedPtr<IMediaSink, ESPMode::ThreadSafe> IMediaSinkPtr;

/** Type definition for shared references to instances of IMediaSink. */
typedef TSharedRef<IMediaSink, ESPMode::ThreadSafe> IMediaSinkRef;

/** Type definition for weak pointers to instances of IMediaSink. */
typedef TWeakPtr<IMediaSink, ESPMode::ThreadSafe> IMediaSinkWeakPtr;
