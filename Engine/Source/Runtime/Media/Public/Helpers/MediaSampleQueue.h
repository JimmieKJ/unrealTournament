// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaSink.h"


/**
 * Implements a media sample sink that queues up samples.
 */
class FMediaSampleQueue
	: public TQueue<TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>>
	, public IMediaSink
{
public:

	/** Virtual destructor. */
	virtual ~FMediaSampleQueue() { }

public:

	// IMediaSink interface

	void ProcessMediaSample( const void* Buffer, uint32 BufferSize, FTimespan Duration, FTimespan Time ) override
	{
		TArray<uint8>* Sample = new TArray<uint8>();
		Sample->AddUninitialized(BufferSize);
		FMemory::Memcpy(Sample->GetData(), Buffer, BufferSize);

		Enqueue(MakeShareable(Sample));
	}
};


/** Type definition for shared pointers to instances of FMediaSampleQueue. */
typedef TSharedPtr<FMediaSampleQueue, ESPMode::ThreadSafe> FMediaSampleQueuePtr;

/** Type definition for shared references to instances of FMediaSampleQueue. */
typedef TSharedRef<FMediaSampleQueue, ESPMode::ThreadSafe> FMediaSampleQueueRef;
