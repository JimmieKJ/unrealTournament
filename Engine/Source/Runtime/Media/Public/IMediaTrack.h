// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class IMediaSink;
class IMediaTrackAudioDetails;
class IMediaTrackCaptionDetails;
class IMediaTrackVideoDetails;


/**
 * Enumerates supported media track types.
 *
 * The Media framework currently supports audio, caption and video tracks.
 * We may implement additional types, such as HTML, still image and custom
 * binary data tracks in the future.
 */
enum class EMediaTrackTypes
{
	/** Audio track. */
	Audio,

	/** Caption track (for subtitles). */
	Caption,

	/** Video track. */
	Video
};


/**
 * Interface for media tracks.
 *
 * A media track is a sequence of data of a uniform media type, such as audio or video.
 * Media file containers may consist of a single track or they may contain multiple
 * tracks of different types, for example video tracks with different encodings or
 * multiple audio and caption text tracks for different languages.
 *
 * Track processing can be enabled and disabled while media playback is in progress.
 * Use the Enable() and Disable() methods respectively. Whether a track is enabled by
 * default is determined by the media file. For video files the video track as well
 * as the audio track for the default language are usually enabled by default.
 *
 * Some track types may be mutually exclusive, which means that the media player can
 * play only one track of a certain type at a time. For example, if a video contains
 * multiple audio tracks, only one of them may be enabled. Use the IsMutuallyExclusive
 * method to determine whether two tracks are mutually exclusive.
 *
 * @see IMediaPlayer
 */
class IMediaTrack
{
public:

	/**
	 * Adds the given media sink to this track.
	 *
	 * @param Sink The sink to add.
	 * @see RemoveSink
	 */
	virtual void AddSink( const TSharedRef<IMediaSink, ESPMode::ThreadSafe>& Sink ) = 0;

	/**
	 * Disables this track.
	 *
	 * If a track is disabled, no data will be decoded for it during media playback.
	 *
	 * @return true on success, false otherwise.
	 * @see Enable, IsEnabled
	 */
	virtual bool Disable() = 0;

	/**
	 * Disables this track.
	 *
	 * If a track is enabled, data will be decoded for it during media playback.
	 * Enabling a track may fail if the track is mutually exclusive with another track.
	 * Use the IsMututallyExclusive method to check for mutual exclusion of tracks.
	 *
	 * @return true on success, false otherwise.
	 * @see Disable, IsEnabled
	 */
	virtual bool Enable() = 0;

	/**
	 * Gets audio track related details.
	 *
	 * This method will assert if called on a track that it is not actually an audio track.
	 * Use the GetType method to determine the type of track before calling this method.
	 *
	 * @return Audio track details.
	 * @see GetCaptionDetails, GetVideoDetails, GetType
	 */
	virtual const IMediaTrackAudioDetails& GetAudioDetails() const = 0;

	/**
	 * Gets caption track related details.
	 *
	 * This method will assert if called on a track that it is not actually an caption track.
	 * Use the GetType method to determine the type of track before calling this method.
	 *
	 * @return Caption track details.
	 * @see GetAudioDetails, GetVideoDetails, GetType
	 */
	virtual const IMediaTrackCaptionDetails& GetCaptionDetails() const = 0;

	/**
	 * Generates the track's display name.
	 *
	 * Unlike GetName(), which may return an empty string for unnamed tracks, this method always
	 * returns a human readable display text for the track's name that will be generated, if needed.
	 *
	 * @return Display name text.
	 * @see GetName
	 */
	virtual FText GetDisplayName() const = 0;

	/**
	 * Gets the track's index number.
	 *
	 * @return Index number.
	 */
	virtual uint32 GetIndex() const = 0;

	/**
	 * Gets the track's language tag, i.e. "en-US" for English.
	 *
	 * @return Language tag.
	 */
	virtual FString GetLanguage() const = 0;

	/**
	 * Gets the name of the track.
	 *
	 * @return Track name string.
	 * @see GetDisplayName
	 */
	virtual FString GetName() const = 0;

	/**
	 * Gets the track's type.
	 *
	 * @return Track type.
	 */
	virtual EMediaTrackTypes GetType() const = 0;

	/**
	 * Gets video track related details.
	 *
	 * This method will assert if called on a track that it is not actually an video track.
	 * Use the GetType method to determine the type of track before calling this method.
	 *
	 * @return Video track details.
	 * @see GetAudioDetails, GetCaptionDetails, GetType
	 */
	virtual const IMediaTrackVideoDetails& GetVideoDetails() const = 0;

	/**
	 * Checks whether this track is currently enabled.
	 *
	 * If a track is enabled, data will be decoded for it during media playback.
	 *
	 * @return true if the track is enabled, false otherwise.
	 * @see Disable, Enable
	 */
	virtual bool IsEnabled() const = 0;

	/**
	 * Checks whether this track is mutually exclusive with another.
	 *
	 * @param Other The other track to check with.
	 * @return true if the tracks are mutually exclusive, false otherwise.
	 */
	virtual bool IsMutuallyExclusive( const TSharedRef<IMediaTrack, ESPMode::ThreadSafe>& Other ) const = 0;

	/**
	 * Checks whether this track has protected content, i.e. DRM.
	 *
	 * @return true if the content is protected, false otherwise.
	 */
	virtual bool IsProtected() const = 0;

	/**
	 * Removes the given media sink from this track.
	 *
	 * @param Sink The sink to remove.
	 */
	virtual void RemoveSink( const TSharedRef<IMediaSink, ESPMode::ThreadSafe>& Sink ) = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaTrack() { }
};


/** Type definition for shared pointers to instances of IMediaTrack. */
typedef TSharedPtr<IMediaTrack, ESPMode::ThreadSafe> IMediaTrackPtr;

/** Type definition for shared references to instances of IMediaTrack. */
typedef TSharedRef<IMediaTrack, ESPMode::ThreadSafe> IMediaTrackRef;
