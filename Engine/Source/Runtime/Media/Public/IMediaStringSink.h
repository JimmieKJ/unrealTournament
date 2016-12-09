// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Timespan.h"

class FString;

/**
 * Interface for media sinks that receive string data.
 *
 * Implementations of this interface must be thread-safe as these
 * methods will be called from some random media decoder thread.
 */
class IMediaStringSink
{
public:

	/**
	 * Initialize the sink.
	 *
	 * @return true on success, false otherwise.
	 * @see ShutdownStringSink
	 */
	virtual bool InitializeStringSink() = 0;

	/**
	 * Display the given string.
	 *
	 * @param String The string to display.
	 * @param Time The time at which to display the string (relative to the beginning of the media).
	 */
	virtual void DisplayStringSinkString(const FString& String, FTimespan Time) = 0;

	/**
	 * Shut down the sink.
	 *
	 * @see InitializeStringSink
	 */
	virtual void ShutdownStringSink() = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaStringSink() { }
};
