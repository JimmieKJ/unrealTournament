// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Type of text overlay.
 */
enum class EMediaOverlayType
{
	/** Caption text for hearing impaired users. */
	Caption,

	/** Subtitle text for non-native speakers. */
	Subtitle,

	/** Generic text. */
	Text,
};


/**
 * Interface for media sinks that receive text overlays, such as captions and subtitles.
 *
 * Implementations of this interface must be thread-safe as these
 * methods will be called from some random media decoder thread.
 */
class IMediaOverlaySink
{
public:

	/**
	 * Initialize the sink.
	 *
	 * @return true on success, false otherwise.
	 * @see ShutdownOverlaySink
	 */
	virtual bool InitializeOverlaySink() = 0;

	/**
	 * Add the given text to the list of visible text overlays.
	 *
	 * @param Text The text to display (may contain formatting).
	 * @param Type The type of string to display.
	 * @param Time The time at which to display the string (relative to the beginning of the media).
	 * @param Duration The duration for which the string is displayed.
	 * @param Position The position at which to display the text (relative to top-left corner, in pixels).
	 * @see ClearOverlaySinkText
	 */
	virtual void AddOverlaySinkText(const FText& Text, EMediaOverlayType Type, FTimespan Time, FTimespan Duration, TOptional<FVector2D> Position) = 0;

	/**
	 * Remove all existing text overlays (regardless of their durations).
	 *
	 * @see AddOverlaySinkText
	 */
	virtual void ClearOverlaySinkText() = 0;

	/**
	 * Shut down the sink.
	 *
	 * @see InitializeOverlaySink
	 */
	virtual void ShutdownOverlaySink() = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaOverlaySink() { }
};
