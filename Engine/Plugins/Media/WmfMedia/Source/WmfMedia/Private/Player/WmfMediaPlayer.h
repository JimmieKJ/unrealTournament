// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWmfMediaResolverCallbacks.h"
#include "AllowWindowsPlatformTypes.h"


/**
 * Implements a media player using the Windows Media Foundation framework.
 */
class FWmfMediaPlayer
	: public IMediaInfo
	, public IMediaPlayer
	, protected IWmfMediaResolverCallbacks
{
public:

	/** Default constructor. */
	FWmfMediaPlayer();

	/** Destructor. */
	~FWmfMediaPlayer();

public:

	// IMediaInfo interface

	virtual FTimespan GetDuration() const override;
	virtual TRange<float> GetSupportedRates(EMediaPlaybackDirections Direction, bool Unthinned) const override;
	virtual FString GetUrl() const override;
	virtual bool SupportsRate(float Rate, bool Unthinned) const override;
	virtual bool SupportsScrubbing() const override;
	virtual bool SupportsSeeking() const override;

public:

	// IMediaPlayer interface

	virtual void Close() override;
	virtual const TArray<IMediaAudioTrackRef>& GetAudioTracks() const override;
	virtual const TArray<IMediaCaptionTrackRef>& GetCaptionTracks() const override;
	virtual const IMediaInfo& GetMediaInfo() const override;
	virtual float GetRate() const override;
	virtual FTimespan GetTime() const override;
	virtual const TArray<IMediaVideoTrackRef>& GetVideoTracks() const override;
	virtual bool IsLooping() const override;
	virtual bool IsPaused() const override;
	virtual bool IsPlaying() const override;
	virtual bool IsReady() const override;
	virtual bool Open(const FString& Url) override;
	virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl) override;
	virtual bool Seek(const FTimespan& Time) override;
	virtual bool SetLooping(bool Looping) override;
	virtual bool SetRate(float Rate) override;

	DECLARE_DERIVED_EVENT(FWmfMediaPlayer, IMediaPlayer::FOnMediaEvent, FOnMediaEvent);
	virtual FOnMediaEvent& OnMediaEvent() override
	{
		return MediaEvent;
	}

protected:

	// IWmfMediaResolverCallbacks interface

	virtual void ProcessResolveComplete(TComPtr<IUnknown> SourceObject, FString ResolvedUrl) override;
	virtual void ProcessResolveFailed(FString FailedUrl) override;

protected:

	/**
	 * Adds the specified stream to the given topology.
	 *
	 * @param StreamIndex The index number of the stream in the presentation descriptor.
	 * @param Topology The topology to add the stream to.
	 * @param PresentationDescriptor The presentation descriptor object.
	 * @param MediaSourceObject The media source object.
	 * @return true on success, false otherwise.
	 */
	void AddStreamToTopology(uint32 StreamIndex, IMFTopology* Topology, IMFPresentationDescriptor* PresentationDescriptor, IMFMediaSource* MediaSourceObject);

	/**
	 * Initializes the media session for the given media source.
	 *
	 * @param SourceObject The media source object.
	 * @param SourceUrl The original URL of the media source.
	 * @return true on success, false otherwise.
	 */
	bool InitializeMediaSession(IUnknown* SourceObject, const FString& SourceUrl);

private:

	/** Handles session errors. */
	void HandleSessionError(HRESULT Error);

	/** Handles session events. */
	void HandleSessionEvent(MediaEventType EventType);

private:

	/** The available audio tracks. */
	TArray<IMediaAudioTrackRef> AudioTracks;

	/** The available caption tracks. */
	TArray<IMediaCaptionTrackRef> CaptionTracks;

	/** The duration of the currently loaded media. */
	FTimespan Duration;

	/** Holds an event delegate that is invoked when a media event occurred. */
	FOnMediaEvent MediaEvent;

	/** Holds the asynchronous callback object for the media stream. */
	TComPtr<FWmfMediaSession> MediaSession;

	/** Holds a pointer to the media source object. */
	TComPtr<IMFMediaSource> MediaSource;

	/** The URL of the currently opened media. */
	FString MediaUrl;

	/** The media source resolver. */
	TComPtr<FWmfMediaResolver> Resolver;

	/** The available video tracks. */
	TArray<IMediaVideoTrackRef> VideoTracks;
};


#include "HideWindowsPlatformTypes.h"
