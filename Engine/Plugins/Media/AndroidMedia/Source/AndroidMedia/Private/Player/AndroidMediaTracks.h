// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaOutput.h"
#include "IMediaTracks.h"
#include "AndroidJavaMediaPlayer.h"



/**
 * Implements the track collection for Android based media players.
 */
class FAndroidMediaTracks
	: public IMediaOutput
	, public IMediaTracks
{

public:

	/** Default constructor. */
	FAndroidMediaTracks();

	/** Destructor. */
	~FAndroidMediaTracks();

public:

	/**
	 * Initialize this object for the specified VLC media player.
	 *
	 * @param InJavaMediaPlayer The java player to use.
	 */
	void Initialize(TSharedRef<FJavaAndroidMediaPlayer, ESPMode::ThreadSafe> InJavaMediaPlayer);

	/** Reset this object. */
	void Reset();

	/** Called each frame. */
	void Tick();

	/** Called to check if playback looped */
	bool DidPlaybackLoop();

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

	/** Send the latest sample data to the audio sink. */
	void UpdateAudioSink();

	/** Send the latest sample data to the caption sink. */
	void UpdateCaptionSink();

	/** Send the latest frame to the video sink. */
	void UpdateVideoSink();

private:

	/** Index of the selected audio track. */
	int32 SelectedAudioTrack;

	/** Index of the selected audio track. */
	int32 SelectedCaptionTrack;

	/** Index of the selected video track. */
	int32 SelectedVideoTrack;

private:

	/** The audio sink. */
	IMediaAudioSink* AudioSink;

	/** The caption sink. */
	IMediaStringSink* CaptionSink;

	/** The video sink. */
	IMediaTextureSink* VideoSink;

private:

	/** Audio track descriptors. */
	TArray<FJavaAndroidMediaPlayer::FAudioTrack> AudioTracks;

	/** Caption track descriptors. */
	TArray<FJavaAndroidMediaPlayer::FCaptionTrack> CaptionTracks;

	/** Video track descriptors. */
	TArray<FJavaAndroidMediaPlayer::FVideoTrack> VideoTracks;

private:

	/** Synchronizes write access to track arrays, selections & sinks. */
	FCriticalSection CriticalSection;

	/** The Java side media interface. */
	TSharedPtr<FJavaAndroidMediaPlayer, ESPMode::ThreadSafe> JavaMediaPlayer;

	/** Position of the last video frame being displayed. */
	int32 LastFramePosition;

	/** Flag for playback looping detected. */
	bool PlaybackLooped;
};
