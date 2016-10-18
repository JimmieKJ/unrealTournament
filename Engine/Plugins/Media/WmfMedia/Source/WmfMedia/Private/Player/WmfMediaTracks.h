// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaOutput.h"
#include "IMediaTracks.h"
#include "AllowWindowsPlatformTypes.h"


class FWmfMediaAudioStream;
class FWmfMediaCaptionStream;
class FWmfMediaImageStream;
class FWmfMediaSampler;
class FWmfMediaVideoStream;
class IMediaAudioSink;
class IMediaStringSink;
class IMediaTextureSink;
enum class EWmfMediaSamplerClockEvent;
enum class EMediaTrackType;


class FWmfMediaTracks
	: public IMediaOutput
	, public IMediaTracks
{
	struct FTrack
	{
		FText DisplayName;
		FString Language;
		FString Name;
		bool Protected;
		TComPtr<FWmfMediaSampler> Sampler;
		TComPtr<IMFStreamDescriptor> StreamDescriptor;
		DWORD StreamIndex;	
	};
	
	struct FAudioTrack : public FTrack
	{
		uint32 NumChannels;
		uint32 SampleRate;
	};
	
	struct FCaptionTrack : public FTrack
	{
	};
	
	struct FImageTrack : public FTrack
	{
		FIntPoint Dimensions;
	};
	
	struct FVideoTrack : public FTrack
	{
		uint32 BitRate;
		FIntPoint Dimensions;
		float FrameRate;
		uint32 Pitch;
		EMediaTextureSinkFormat SinkFormat;
	};
	
public:

	/** Default constructor. */
	FWmfMediaTracks();

	/** Virtual destructor. */
	~FWmfMediaTracks();

public:

	/**
	 * Add a new track for the specified stream.
	 *
	 * @param MajorType The type of stream to add.
	 * @param MediaType The media type information of the input stream.
	 * @param OutputType The media type information of the output.
	 * @param Sampler The sample grabber callback object to use.
	 * @param StreamIndex The stream's index number in the presentation.
	 * @param StreamDescriptor The stream's descriptor object.
	 * @see HasAnyStreams
	 */
	void AddTrack(const GUID& MajorType, IMFMediaType* MediaType, IMFMediaType* OutputType, FWmfMediaSampler* Sampler, IMFStreamDescriptor* StreamDescriptor, DWORD StreamIndex);

	/**
	 * Append track statistics information to the given string.
	 *
	 * @param OutStats The string to append the statistics to.
	 */
	void AppendStats(FString &OutStats) const;

	/** Discard any pending data on the media sinks. */
	void Flush();

	/**
	 * Initialize this track collection.
	 *
	 * @param InPresentationDescriptor The presentation descriptor object.
	 */
	void Initialize(IMFPresentationDescriptor& InPresentationDescriptor);

	/** Remove all streams from the collection. */
	void Reset();

	/**
	 * Notify tracks that playback was paused or resumed.
	 *
	 * @param Paused true if playback was paused, false if resumed.
	 */
	void SetPaused(bool Paused);

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

	/** Initialize the current image sink. */
	void InitializeImageSink();

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

	/** The currently used caption sink. */
	IMediaStringSink* CaptionSink;

	/** The currently used image sink. */
	IMediaTextureSink* ImageSink;

	/** The currently used video sink. */
	IMediaTextureSink* VideoSink;

private:

	/** The available audio tracks. */
	TArray<FAudioTrack> AudioTracks;

	/** The available caption tracks. */
	TArray<FCaptionTrack> CaptionTracks;

	/** The available image tracks. */
	TArray<FImageTrack> ImageTracks;

	/** The available video tracks. */
	TArray<FVideoTrack> VideoTracks;

private:

	/** Index of the selected audio track. */
	int32 SelectedAudioTrack;

	/** Index of the selected caption track. */
	int32 SelectedCaptionTrack;

	/** Index of the selected image track. */
	int32 SelectedImageTrack;

	/** Index of the selected video track. */
	int32 SelectedVideoTrack;

private:

	/** Synchronizes write access to track arrays, selections & sinks. */
	mutable FCriticalSection CriticalSection;

	/** The presentation descriptor of the currently opened media. */
	TComPtr<IMFPresentationDescriptor> PresentationDescriptor;
};


#include "HideWindowsPlatformTypes.h"
