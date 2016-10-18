// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvfMediaTracks.h"
#include "IMediaControls.h"
#include "IMediaPlayer.h"


@class AVPlayer;
@class AVPlayerItem;
@class FAVPlayerDelegate;


/**
 * Implements a media player using the AV framework.
 */
class FAvfMediaPlayer
	: public FTickerObjectBase
	, public IMediaControls
	, public IMediaPlayer
{
public:

	/** Default constructor. */
	FAvfMediaPlayer();

	/** Destructor. */
	~FAvfMediaPlayer() { }

	/** Called by the delegate whenever the player item status changes. */
	void HandleStatusNotification(AVPlayerItemStatus Status);

	/** Called by the delegate when the playback reaches the end. */
	void HandleDidReachEnd();

public:

	//~ FTickerObjectBase interface

	virtual bool Tick(float DeltaTime) override;

public:

	//~ IMediaControls interface

	virtual FTimespan GetDuration() const override;
	virtual float GetRate() const override;
	virtual EMediaState GetState() const override;
	virtual TRange<float> GetSupportedRates(EMediaPlaybackDirections Direction, bool Unthinned) const override;
	virtual FTimespan GetTime() const override;
	virtual bool IsLooping() const override;
	virtual bool Seek(const FTimespan& Time) override;
	virtual bool SetLooping(bool Looping) override;
	virtual bool SetRate(float Rate) override;
	virtual bool SupportsRate(float Rate, bool Unthinned) const override;
	virtual bool SupportsScrubbing() const override;
	virtual bool SupportsSeeking() const override;

public:

	//~ IMediaPlayer interface

	virtual void Close() override;
	virtual IMediaControls& GetControls() override;
	virtual FString GetInfo() const override;
	virtual IMediaOutput& GetOutput() override;
	virtual FString GetStats() const override;
	virtual IMediaTracks& GetTracks() override;
	virtual FString GetUrl() const override;
	virtual bool Open(const FString& Url, const IMediaOptions& Options) override;
	virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions& Options) override;

	DECLARE_DERIVED_EVENT(FVlcMediaPlayer, IMediaPlayer::FOnMediaEvent, FOnMediaEvent);
	virtual FOnMediaEvent& OnMediaEvent() override
	{
		return MediaEvent;
	}

private:

	/** The current playback rate. */
	float CurrentRate;

	/** The current time of the playback. */
	FTimespan CurrentTime;

	/** The duration of the media. */
    FTimespan Duration;

	/** Holds an event delegate that is invoked when a media event occurred. */
	FOnMediaEvent MediaEvent;

	/** Cocoa helper object we can use to keep track of ns property changes in our media items */
	FAVPlayerDelegate* MediaHelper;
    
	/** The AVFoundation media player */
	AVPlayer* MediaPlayer;

	/** The URL of the currently opened media. */
	FString MediaUrl;

	/** The player item which the media player uses to progress. */
	AVPlayerItem* PlayerItem;

	/** Should the video loop to the beginning at completion */
    bool ShouldLoop;

	/** The media track collection. */
	FAvfMediaTracks Tracks;
	
	/** Media playback state. */
	EMediaState State;
	
	bool bPrerolled;
};
