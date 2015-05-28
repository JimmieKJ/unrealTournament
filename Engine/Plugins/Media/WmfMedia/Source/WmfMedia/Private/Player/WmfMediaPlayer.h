// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AllowWindowsPlatformTypes.h"


/**
 * Implements a media player using the Windows Media Foundation framework.
 */
class FWmfMediaPlayer
	: public IMediaInfo
	, public IMediaPlayer
{
public:

	/** Default constructor. */
	FWmfMediaPlayer();

	/** Destructor. */
	~FWmfMediaPlayer();

public:

	// IMediaInfo interface

	virtual FTimespan GetDuration() const override;
	virtual TRange<float> GetSupportedRates( EMediaPlaybackDirections Direction, bool Unthinned ) const override;
	virtual FString GetUrl() const override;
	virtual bool SupportsRate( float Rate, bool Unthinned ) const override;
	virtual bool SupportsScrubbing() const override;
	virtual bool SupportsSeeking() const override;

public:

	// IMediaPlayer interface

	virtual void Close() override;
	virtual const IMediaInfo& GetMediaInfo() const override;
	virtual float GetRate() const override;
	virtual FTimespan GetTime() const override;
	virtual const TArray<IMediaTrackRef>& GetTracks() const override;
	virtual bool IsLooping() const override;
	virtual bool IsPaused() const override;
	virtual bool IsPlaying() const override;
	virtual bool IsReady() const override;
	virtual bool Open( const FString& Url ) override;
	virtual bool Open( const TSharedRef<TArray<uint8>>& Buffer, const FString& OriginalUrl ) override;
	virtual bool Seek( const FTimespan& Time ) override;
	virtual bool SetLooping( bool Looping ) override;
	virtual bool SetRate( float Rate ) override;

	DECLARE_DERIVED_EVENT(FWmfMediaPlayer, IMediaPlayer::FOnMediaClosed, FOnMediaClosed);
	virtual FOnMediaClosed& OnClosed() override
	{
		return ClosedEvent;
	}

	DECLARE_DERIVED_EVENT(FWmfMediaPlayer, IMediaPlayer::FOnMediaOpened, FOnMediaOpened);
	virtual FOnMediaOpened& OnOpened() override
	{
		return OpenedEvent;
	}

protected:

	/**
	 * Adds the specified stream to the given topology.
	 *
	 * @param StreamIndex The index number of the stream in the presentation descriptor.
	 * @param Topology The topology to add the stream to.
	 * @param PresentationDescriptor The presentation descriptor object.
	 * @param MediaSource The media source object.
	 * @return true on success, false otherwise.
	 */
	void AddStreamToTopology( uint32 StreamIndex, IMFTopology* Topology, IMFPresentationDescriptor* PresentationDescriptor, IMFMediaSource* MediaSource );

	/**
	 * Initializes the media session for the given media source.
	 *
	 * @param SourceObject The media source object.
	 * @param SourceUrl The original URL of the media source.
	 * @return true on success, false otherwise.
	 */
	bool InitializeMediaSession( IUnknown* SourceObject, const FString& SourceUrl );

private:

	/** Handles session errors. */
	void HandleSessionError(HRESULT Error);

private:

	/** The available media tracks. */
	TArray<IMediaTrackRef> Tracks;

	/** The duration of the currently loaded media. */
	FTimespan Duration;

	/** Holds the asynchronous callback object for the media stream. */
	TComPtr<FWmfMediaSession> MediaSession;

	/** Holds a pointer to the media source object. */
	TComPtr<IMFMediaSource> MediaSource;

	/** The URL of the currently opened media. */
	FString MediaUrl;

private:

	/** Holds an event delegate that is invoked when media has been closed. */
	FOnMediaClosed ClosedEvent;

	/** Holds an event delegate that is invoked when media has been opened. */
	FOnMediaOpened OpenedEvent;
};


#include "HideWindowsPlatformTypes.h"
