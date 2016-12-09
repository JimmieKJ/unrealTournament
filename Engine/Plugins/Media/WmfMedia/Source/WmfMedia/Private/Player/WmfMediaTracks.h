// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../WmfMediaPrivate.h"
#include "IMediaOutput.h"
#include "IMediaTracks.h"

#if WMFMEDIA_SUPPORTED_PLATFORM

#include "AllowWindowsPlatformTypes.h"

class FWmfMediaSampler;
class IMediaAudioSink;
class IMediaStringSink;
class IMediaTextureSink;
enum class EWmfMediaSamplerClockEvent;
enum class EMediaTrackType;


class FWmfMediaTracks
	: public IMediaOutput
	, public IMediaTracks
{
	struct FStreamInfo
	{
		TComPtr<IMFStreamDescriptor> Descriptor;
		FText DisplayName;
		FString Language;
		FString Name;
		TComPtr<IMFMediaType> OutputType;
		bool Protected;
		TComPtr<FWmfMediaSampler> Sampler;
		DWORD StreamIndex;
	};
	
	struct FAudioTrack : public FStreamInfo
	{
		uint32 BitsPerSample;
		uint32 NumChannels;
		uint32 SampleRate;
	};
	
	struct FCaptionTrack : public FStreamInfo
	{
	};
		
	struct FVideoTrack : public FStreamInfo
	{
		uint32 BitRate;
		FIntPoint BufferDim;
		float FrameRate;
		FIntPoint OutputDim;
		EMediaTextureSinkFormat SinkFormat;
	};
	
public:

	/** Default constructor. */
	FWmfMediaTracks();

	/** Virtual destructor. */
	~FWmfMediaTracks();

public:

	/**
	 * Append track statistics information to the given string.
	 *
	 * @param OutStats The string to append the statistics to.
	 */
	void AppendStats(FString &OutStats) const;

	/**
	 * Create the playback topology for the current track selection.
	 *
	 * @return The new topology.
	 */
	TComPtr<IMFTopology> CreateTopology() const;

	/** Discard any pending data on the media sinks. */
	void FlushSinks();

	/**
	 * Initialize the track collection.
	 *
	 * @param InMediaSource The media source object.
	 * @param InPresentationDescriptor The presentation descriptor object.
	 * @param OutInfo Will contain information about the available media tracks.
	 * @return true on success, false otherwise.
	 */
	bool Initialize(IMFMediaSource& InMediaSource, IMFPresentationDescriptor& InPresentationDescriptor, FString& OutInfo);

	/** Remove all streams from the track collection. */
	void Reset();

	/**
	 * Notify tracks that playback was paused or resumed.
	 *
	 * @param Paused true if playback was paused, false if resumed.
	 */
	void SetPaused(bool Paused);

public:

	/** Get an event that gets fired when the track selection changed. */
	DECLARE_EVENT(FWmfMediaTracks, FOnSelectionChanged)
	FOnSelectionChanged& OnSelectionChanged()
	{
		return SelectionChangedEvent;
	}

public:

	//~ IMediaOutput interface

	virtual void SetAudioSink(IMediaAudioSink* Sink) override;
	virtual void SetOverlaySink(IMediaOverlaySink* Sink) override;
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

	/**
	 * Add the given stream to the specified playback topology.
	 *
	 * @param Stream The stream to add.
	 * @param Topology The playback topology.
	 * @return true on success, false otherwise.
	 */
	bool AddStreamToTopology(const FStreamInfo& Stream, IMFTopology& Topology) const;

	/**
	 * Add the specified stream to the track collection.
	 *
	 * @param StreamIndex The index of the stream to add.
	 * @param InMediaSource The media source containing the stream.
	 * @param InPresentationDescriptor The presentation descriptor containing the stream's details.
	 * @param OutInfo Will contain appended debug information.
	 */
	void AddStreamToTracks(uint32 StreamIndex, IMFMediaSource& InMediaSource, IMFPresentationDescriptor& InPresentationDescriptor, FString& OutInfo);

	/** Initialize the current audio sink. */
	void InitializeAudioSink();

	/** Initialize the current text overlay sink. */
	void InitializeOverlaySink();

	/** Initialize the current subtitle sink. */
	void InitializeSubtitleSink();

	/** Initialize the current video sink. */
	void InitializeVideoSink();

private:

	/** Callback for handling media sampler pauses. */
	void HandleMediaSamplerClock(EWmfMediaSamplerClockEvent Event, EMediaTrackType TrackType);

	/** Callback for handling new samples from the streams' media sample buffers. */
	void HandleMediaSamplerSample(const uint8* Buffer, uint32 Size, FTimespan Duration, FTimespan Time, EMediaTrackType TrackType);

private:

	/** The currently used audio sink. */
	IMediaAudioSink* AudioSink;

	/** The currently used text overlay sink. */
	IMediaOverlaySink* OverlaySink;

	/** The currently used video sink. */
	IMediaTextureSink* VideoSink;

private:

	/** The available audio tracks. */
	TArray<FAudioTrack> AudioTracks;

	/** The available caption tracks. */
	TArray<FCaptionTrack> CaptionTracks;

	/** The available video tracks. */
	TArray<FVideoTrack> VideoTracks;

private:

	/** Index of the selected audio track. */
	int32 SelectedAudioTrack;

	/** Index of the selected caption track. */
	int32 SelectedCaptionTrack;

	/** Index of the selected video track. */
	int32 SelectedVideoTrack;

private:

	/** Synchronizes write access to track arrays, selections & sinks. */
	mutable FCriticalSection CriticalSection;

	/** The currently opened media. */
	TComPtr<IMFMediaSource> MediaSource;

	/** The presentation descriptor of the currently opened media. */
	TComPtr<IMFPresentationDescriptor> PresentationDescriptor;

	/** Event that gets fired when the track selection changed. */
	FOnSelectionChanged SelectionChangedEvent;
};


#include "HideWindowsPlatformTypes.h"

#endif
