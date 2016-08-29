// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class IMediaControls;
class IMediaOptions;
class IMediaOutput;
class IMediaTracks;


/**
 * Enumerates media player related events.
 */
enum class EMediaEvent
{
	/** The current media source has been closed. */
	MediaClosed,

	/** A new media source has been opened. */
	MediaOpened,

	/** A media source failed to open. */
	MediaOpenFailed,

	/** The end of the media (or beginning if playing in reverse) has been reached. */
	PlaybackEndReached,

	/** Playback has been resumed. */
	PlaybackResumed,

	/** Playback has been suspended. */
	PlaybackSuspended,

	/** Media tracks have changed. */
	TracksChanged
};


/**
 * Interface for media players.
 *
 * @see IMediaStream
 */
class IMediaPlayer
{
public:

	/**
	 * Close a previously opened media.
	 *
	 * Call this method to free up all resources associated with an opened
	 * media source. If no media is open, this function has no effect.
	 *
	 * The media may not necessarily be closed after this function succeeds,
	 * because closing may happen asynchronously. Subscribe to the MediaClosed
	 * event to detect when the media finished closing. This events is only
	 * triggered if Close returns true.
	 *
	 * @see IsReady, Open
	 */
	virtual void Close() = 0;

	/**
	 * Get the media playback controls for this player.
	 *
	 * @return Playback controls.
	 * @see GetOutput, GetTracks
	 */
	virtual IMediaControls& GetControls() = 0;

	/**
	 * Get debug information about the player and currently opened media.
	 *
	 * @return Information string.
	 * @see GetStats
	 */
	virtual FString GetInfo() const = 0;

	/**
	 * Get access to the media player's output.
	 *
	 * @return Media tracks interface.
	 * @see GetControls, GetTracks
	 */
	virtual IMediaOutput& GetOutput() = 0;

	/**
	 * Get playback statistics information.
	 *
	 * @return Information string.
	 * @see GetInfo
	 */
	virtual FString GetStats() const = 0;

	/**
	 * Get access to the media player's tracks.
	 *
	 * @return Media tracks interface.
	 * @see GetControls, GetOutput
	 */
	virtual IMediaTracks& GetTracks() = 0;

	/**
	 * Get the URL of the currently loaded media.
	 *
	 * @return Media URL.
	 */
	virtual FString GetUrl() const = 0;

	/**
	 * Open a media source from a URL with optional parameters.
	 *
	 * The media may not necessarily be opened after this function succeeds,
	 * because opening may happen asynchronously. Subscribe to the MediaOpened
	 * and MediaOpenFailed events to detect when the media finished or failed
	 * to open. These events are only triggered if Open returns true.
	 *
	 * The optional parameters can be used to configure aspects of media playback
	 * and are specific to the type of media source and the underlying player.
	 * Check their documentation for available keys and values.
	 *
	 * @param Url The URL of the media to open (file name or web address).
	 * @param Options Optional media parameters.
	 * @return true if the media is being opened, false otherwise.
	 * @see Close, IsReady, OnOpen, OnOpenFailed
	 */
	virtual bool Open(const FString& Url, const IMediaOptions& Options) = 0;

	/**
	 * Open a media source from a file or memory archive with optional parameters.
	 *
	 * The media may not necessarily be opened after this function succeeds,
	 * because opening may happen asynchronously. Subscribe to the MediaOpened
	 * and MediaOpenFailed events to detect when the media finished or failed
	 * to open. These events are only triggered if Open returns true.
	 *
	 * The optional parameters can be used to configure aspects of media playback
	 * and are specific to the type of media source and the underlying player.
	 * Check their documentation for available keys and values.
	 *
	 * @param Archive The archive holding the media data.
	 * @param OriginalUrl The original URL of the media that was loaded into the buffer.
	 * @param Options Optional media parameters.
	 * @return true if the media is being opened, false otherwise.
	 * @see Close, IsReady, OnOpen, OnOpenFailed
	 */
	virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions& Options) = 0;

public:

	/** Get an event delegate that is invoked when an event occurred. */
	DECLARE_EVENT_OneParam(IMediaPlayer, FOnMediaEvent, EMediaEvent /*Event*/)
	virtual FOnMediaEvent& OnMediaEvent() = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaPlayer() { }
};
