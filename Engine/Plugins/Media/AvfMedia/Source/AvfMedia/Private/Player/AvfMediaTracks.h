// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaOutput.h"
#include "IMediaTracks.h"


class FAvfMediaTracks
	: public IMediaOutput
	, public IMediaTracks
{
	enum ESyncStatus
	{
		Default,    // Starting state
		Behind,     // Frame is behind playback cursor.
		Ready,      // Frame is within tolerance of playback cursor.
		Ahead,      // Frame is ahead of playback cursor.
	};

	struct FTrack
	{
		AVAssetTrack* AssetTrack;
		FText DisplayName;
		bool Loaded;
		FString Name;
		NSObject* Output;
		AVAssetReader* Reader;
		AudioConverterRef Converter;
	};

public:

	/** Default constructor. */
	FAvfMediaTracks();

	/** Virtual destructor. */
	virtual ~FAvfMediaTracks();

public:

	/** Discard any pending data on the media sinks. */
	void Flush();

	/**
	 * Initialize the track collection.
	 *
	 * @param PlayerItem The player item containing the track information.
	 */
	void Initialize(AVPlayerItem* InPlayerItem);

	/** Reset the stream collection. */
	void Reset();

	/**
	 * Notify tracks that playback rate was changed.
	 *
	 * @param Rate The new playback rate.
	 */
	void SetRate(float Rate);
	
	/**
	 * Notify tracks that playback is seeking.
	 *
	 * @param Time The time to seek to.
	 */
	void Seek(const FTimespan& Time);
	
	/**
	 * Append track statistics information to the given string.
	 *
	 * @param OutStats The string to append the statistics to.
	 */
	void AppendStats(FString &OutStats) const;

	/**
	 * Update the playback of tracks - must only be called when playing.
	 *
	 * @param DeltaTime Time between ticks.
	 */
	bool Tick(float DeltaTime);
	
	/**
	 * Called by the caption track delegate to provide the attributed strings for each timecode to the caption sink.
	 *
	 * @param Output The caption track output that generated the strings.
	 * @param Strings The attributed caption strings.
	 * @param ItemTime The display time for the strings.
	 */
	void HandleSubtitleCaptionStrings(AVPlayerItemLegibleOutput* Output, NSArray<NSAttributedString*>* Strings, CMTime ItemTime);

public:

	//~ IMediaOutput interface

	virtual void SetAudioSink(IMediaAudioSink* Sink) override;
	virtual void SetCaptionSink(IMediaStringSink* Sink) override;
	virtual void SetImageSink(IMediaTextureSink* Sink) override;
	virtual void SetVideoSink(IMediaTextureSink* Sink) override;

public:

	//~ IMediaTracks interface

	virtual uint32 GetAudioTrackChannels(int32 TrackIndex) const override;
	virtual uint32 GetAudioTrackSampleRate(int32 TrackIndex) const override;
	virtual int32 GetNumTracks(EMediaTrackType TrackType) const override;
	virtual int32 GetSelectedTrack(EMediaTrackType TrackType) const override;
	virtual FText GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual uint32 GetVideoTrackBitRate(int32 TrackIndex) const override;
	virtual FIntPoint GetVideoTrackDimensions(int32 TrackIndex) const override;
	virtual float GetVideoTrackFrameRate(int32 TrackIndex) const override;
	virtual bool SelectTrack(EMediaTrackType TrackType, int32 TrackIndex) override;

protected:

	/** Initialize the current audio sink. */
	void InitializeAudioSink();

	/** Initialize the current caption sink. */
	void InitializeCaptionSink();

	/** Initialize the current video sink. */
	void InitializeVideoSink();
	
	/** Update the current video sink. */
	void UpdateVideoSink();

private:

	/** The currently used audio sink. */
	IMediaAudioSink* AudioSink;

	/** The currently used caption sink. */
	IMediaStringSink* CaptionSink;

	/** The currently used video sink. */
	IMediaTextureSink* VideoSink;

private:

	/** The available audio tracks. */
	TArray<FTrack> AudioTracks;

	/** The available caption tracks. */
	TArray<FTrack> CaptionTracks;

	/** The available video tracks. */
	TArray<FTrack> VideoTracks;

private:

	/** Synchronizes write access to track arrays, selections & sinks. */
	mutable FCriticalSection CriticalSection;

	/** The player item containing the track information. */
	AVPlayerItem* PlayerItem;

	/** Index of the selected audio track. */
	int32 SelectedAudioTrack;

	/** Index of the selected caption track. */
	int32 SelectedCaptionTrack;

	/** Index of the selected video track. */
	int32 SelectedVideoTrack;

private:
	
	/** Target desciption for audio output required by Media framework audio sinks. */
	AudioStreamBasicDescription TargetDesc;
	
	/** Whether the audio is currently paused. */
	bool bAudioPaused;
	
	/** The last audio sample provided to the sink. */
	CMTime LastAudioSample;
	
	/** Seek to this time. */
	double SeekTime;
	
	/** Has been played with fast/slow rate? */
	bool bZoomed;
	
#if WITH_ENGINE && !PLATFORM_MAC
private:
	/** The Metal texture cache for unbuffered texture uploads. */
	CVMetalTextureCacheRef MetalTextureCache;
#endif
};
