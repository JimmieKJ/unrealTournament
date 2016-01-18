// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaSink.h"


/**
 * Implements a media sample sink that buffers the last sample.
 */
class FMediaSampleBuffer
	: public TQueue<TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>>
	, public IMediaSink
{
public:

	/** Default constructor. */
	FMediaSampleBuffer()
		: CurrentSampleTime(FTimespan::MinValue())
	{ }

	/** Virtual destructor. */
	virtual ~FMediaSampleBuffer() { }

public:

	/**
	 * Gets the sample data of the current media sample.
	 *
	 * @return The current sample.
	 * @see GetCurrentSampleTime
	 */
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> GetCurrentSample() const
	{
		FScopeLock Lock(&CriticalSection);
		return CurrentSample;
	}
	
	/**
	 * Gets the playback time of the currently available media sample.
	 *
	 * @return Sample playback time.
	 * @see GetCurrentSample
	 */
	FTimespan GetCurrentSampleTime() const
	{
		return CurrentSampleTime;
	}

public:

	// IMediaSink interface

	void ProcessMediaSample( const void* Buffer, uint32 BufferSize, FTimespan Duration, FTimespan Time ) override
	{
		TArray<uint8>* Sample = new TArray<uint8>();
		Sample->AddUninitialized(BufferSize);
		FMemory::Memcpy(Sample->GetData(), Buffer, BufferSize);

		FScopeLock Lock(&CriticalSection);
		CurrentSample = MakeShareable(Sample);
		CurrentSampleTime = Time;
	}
	
private:

	/** Critical section for locking access to CurrentSample. */
	mutable FCriticalSection CriticalSection;

	/** Holds the sample data of the current media sample. */
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> CurrentSample;

	/** The playback time of the currently available media sample. */
	FTimespan CurrentSampleTime;
};


/** Type definition for shared pointers to instances of FMediaSampleBuffer. */
typedef TSharedPtr<FMediaSampleBuffer, ESPMode::ThreadSafe> FMediaSampleBufferPtr;

/** Type definition for shared references to instances of FMediaSampleBuffer. */
typedef TSharedRef<FMediaSampleBuffer, ESPMode::ThreadSafe> FMediaSampleBufferRef;
