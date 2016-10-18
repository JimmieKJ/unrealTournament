// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Enumerates available media track types.
 */
enum class EMediaTrackType
{
	/** Audio tracks. */
	Audio,

	/** Binary data tracks. */
	Binary,

	/** Caption (subtitle) tracks) */
	Caption,

	/** Still image tracks. */
	Image,

	/** Script tracks. */
	Script,

	/** Video tracks. */
	Video
};


/**
 * Interface for access to a media player's tracks.
 *
 * @see IMediaControls, IMediaOutput
 */
class IMediaTracks
{
public:

	/**
	 * Get the number of channels in the specified audio track.
	 *
	 * @param TrackIndex Index of the audio track.
	 * @return Number of channels.
	 * @see GetAudioTrackSamplesPerSeconds
	 */
	virtual uint32 GetAudioTrackChannels(int32 TrackIndex) const = 0;

	/**
	 * Get the sample rate of the specified audio track.
	 *
	 * @param TrackIndex Index of the audio track.
	 * @return Samples per second.
	 * @see GetAudioTrackChannels
	 */
	virtual uint32 GetAudioTrackSampleRate(int32 TrackIndex) const = 0;

	/**
	 * Get the number of media tracks of the given type.
	 *
	 * @param TrackType The type of media tracks.
	 * @return Number of tracks.
	 * @see GetSelectedTrack, SelectTrack
	 */
	virtual int32 GetNumTracks(EMediaTrackType TrackType) const = 0;

	/**
	 * Get the index of the currently selected track of the given type.
	 *
	 * @param TrackType The type of track to get.
	 * @return The index of the selected track, or INDEX_NONE if no track is active.
	 * @see GetNumTracks, SelectTrack
	 */
	virtual int32 GetSelectedTrack(EMediaTrackType TrackType) const = 0;

	/**
	 * Get the human readable name of the specified track.
	 *
	 * @param TrackType The type of track.
	 * @param TrackIndex The index of the track.
	 * @return Display name.
	 * @see GetTrackLanguage, GetTrackName
	 */
	virtual FText GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const = 0;

	/**
	 * Get the language tag of the specified track.
	 *
	 * @param TrackType The type of track.
	 * @param TrackIndex The index of the track.
	 * @return Language tag, i.e. "en-US" for English, or "und" for undefined.
	 * @see GetTrackDisplayName, GetTrackName
	 */
	virtual FString GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const = 0;

	/**
	 * Get the internal name of the specified track.
	 *
	 * @param TrackType The type of track.
	 * @param TrackIndex The index of the track.
	 * @return Track name.
	 * @see GetTrackDisplayName, GetTrackLanguage
	 */
	virtual FString GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const = 0;

	/**
	 * Get the average data rate of the specified video track.
	 *
	 * @param TrackIndex The index of the track.
	 * @return Data rate (in bits per second).
	 * @see GetVideoTrackAspectRatio, GetVideoTrackDimensions, GetVideoTrackFrameRate
	 */
	virtual uint32 GetVideoTrackBitRate(int32 TrackIndex) const = 0;

	/**
	 * Get the dimensions of the specified video track.
	 *
	 * @param TrackType The type of track.
	 * @param TrackIndex The index of the track.
	 * @return Video dimensions (in pixels).
	 * @see GetVideoTrackAspectRatio, GetVideoTrackBitRate, GetVideoTrackFrameRate
	 */
	virtual FIntPoint GetVideoTrackDimensions(int32 TrackIndex) const = 0;

	/**
	 * Get the frame rate of the specified video track.
	 *
	 * @param TrackType The type of track.
	 * @param TrackIndex The index of the track.
	 * @return Frame rate (in frames per second).
	 * @see GetVideoTrackAspectRatio, GetVideoTrackBitRate, GetVideoTrackDimensions
	 */
	virtual float GetVideoTrackFrameRate(int32 TrackIndex) const = 0;

	/**
	 * Select the active track of the given type.
	 *
	 * Only one track of a given type can be active at any time.
	 *
	 * @param TrackType The type of track to select.
	 * @param TrackIndex The index of the track to select, or INDEX_NONE to deselect.
	 * @return true if the track was selected, false otherwise.
	 * @see GetNumTracks, GetSelectedTrack
	 */
	virtual bool SelectTrack(EMediaTrackType TrackType, int32 TrackIndex) = 0;

public:

	/**
	 * Get the video's aspect ratio of the specified video track.
	 *
	 * @param TrackIndex Index of the video track.
	 * @return Aspect ratio.
	 * @see GetVideoTrackBitRate, GetVideoTrackDimensions, GetVideoTrackFrameRate
	 */
	float GetVideoTrackAspectRatio(int32 TrackIndex) const
	{
		const FIntPoint Dimensions = GetVideoTrackDimensions(TrackIndex);

		if (Dimensions.Y == 0)
		{
			return 0.0f;
		}

		return Dimensions.X / Dimensions.Y;
	}

public:

	/** Virtual destructor. */
	virtual ~IMediaTracks() { }
};
