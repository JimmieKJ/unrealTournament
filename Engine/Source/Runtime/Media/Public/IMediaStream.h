// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class IMediaSink;


/**
 * Interface for media streams.
 *
 * A media stream is a sequence of data of a uniform media type, such as audio or video.
 * Media file containers may consist of a single stream or they may contain multiple
 * streams of different types, for example video streams with different encodings or
 * multiple audio and caption text streams for different languages.
 *
 * Track processing can be enabled and disabled while media playback is in progress.
 * Use the Enable() and Disable() methods respectively. Whether a stream is enabled by
 * default is determined by the media file. For video files the video stream as well
 * as the audio stream for the default language are usually enabled by default.
 *
 * Some stream types may be mutually exclusive, which means that the media player can
 * play only one stream of a certain type at a time. For example, if a video contains
 * multiple audio streams, only one of them may be enabled. Use the IsMutuallyExclusive
 * method to determine whether two streams are mutually exclusive.
 *
 * @see IMediaPlayer
 */
class IMediaStream
{
public:

	/**
	 * Adds the given media sink to this stream.
	 *
	 * This method uses deferred CPU copies to update the resource, which can be slow.
	 *
	 * @param Sink The sink to add.
	 * @see RemoveSink
	 */
	virtual void AddSink(const TSharedRef<IMediaSink, ESPMode::ThreadSafe>& Sink) = 0;

	/**
	 * Disables this stream.
	 *
	 * If a stream is disabled, no data will be decoded for it during media playback.
	 *
	 * @return true on success, false otherwise.
	 * @see Enable, IsEnabled
	 */
	virtual bool Disable() = 0;

	/**
	 * Disables this stream.
	 *
	 * If a stream is enabled, data will be decoded for it during media playback.
	 * Enabling a stream may fail if the stream is mutually exclusive with another stream.
	 * Use the IsMututallyExclusive method to check for mutual exclusion of streams.
	 *
	 * @return true on success, false otherwise.
	 * @see Disable, IsEnabled
	 */
	virtual bool Enable() = 0;

	/**
	 * Generates the stream's display name.
	 *
	 * Unlike GetName(), which may return an empty string for unnamed streams, this method always
	 * returns a human readable display text for the stream's name that will be generated, if needed.
	 *
	 * @return Display name text.
	 * @see GetName
	 */
	virtual FText GetDisplayName() const = 0;

	/**
	 * Gets the stream's language tag, i.e. "en-US" for English.
	 *
	 * @return Language tag.
	 */
	virtual FString GetLanguage() const = 0;

	/**
	 * Gets the name of the stream.
	 *
	 * @return Track name string.
	 * @see GetDisplayName
	 */
	virtual FString GetName() const = 0;

	/**
	 * Checks whether this stream is currently enabled.
	 *
	 * If a stream is enabled, data will be decoded for it during media playback.
	 *
	 * @return true if the stream is enabled, false otherwise.
	 * @see Disable, Enable
	 */
	virtual bool IsEnabled() const = 0;

	/**
	 * Checks whether this stream is mutually exclusive with another.
	 *
	 * @param Other The other stream to check with.
	 * @return true if the streams are mutually exclusive, false otherwise.
	 */
	virtual bool IsMutuallyExclusive(const TSharedRef<IMediaStream, ESPMode::ThreadSafe>& Other) const = 0;

	/**
	 * Checks whether this stream has protected content, i.e. DRM.
	 *
	 * @return true if the content is protected, false otherwise.
	 */
	virtual bool IsProtected() const = 0;

	/**
	 * Removes the given media sink from this stream.
	 *
	 * @param Sink The sink to remove.
	 */
	virtual void RemoveSink(const TSharedRef<IMediaSink, ESPMode::ThreadSafe>& Sink) = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaStream() { }
};


/** Type definition for shared pointers to instances of IMediaStream. */
typedef TSharedPtr<IMediaStream, ESPMode::ThreadSafe> IMediaStreamPtr;

/** Type definition for shared references to instances of IMediaStream. */
typedef TSharedRef<IMediaStream, ESPMode::ThreadSafe> IMediaStreamRef;
