// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaInfo.h"
#include "IMediaPlayer.h"


/**
 * Cocoa class that can help us with reading player item information.
 */
@interface FMediaHelper : NSObject
{
};
/** We should only initiate a helper with a media player */
-(FMediaHelper*) initWithMediaPlayer:(AVPlayer*)InPlayer;
/** Destructor */
-(void)dealloc;

/** Reference to the media player which will be responsible for this media session */
@property(retain) AVPlayer* MediaPlayer;

/** Flag to dictate whether the media players current item is ready to play */
@property bool bIsPlayerItemReady;

@end


/**
 * Implements a media player using the AV framework.
 */
class FAvfMediaPlayer
	: public IMediaInfo
	, public IMediaPlayer
    , public FTickerObjectBase
{
public:

	/** Default constructor. */
	FAvfMediaPlayer();

	/** Destructor. */
	~FAvfMediaPlayer() { }

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
	virtual bool Open( const FString& Url ) override;
	virtual bool Open( const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl ) override;
	virtual bool Seek( const FTimespan& Time ) override;
	virtual bool SetLooping( bool Looping ) override;
	virtual bool SetRate( float Rate ) override;

	DECLARE_DERIVED_EVENT(FAvfMediaPlayer, IMediaPlayer::FOnMediaClosed, FOnMediaClosed);
	virtual FOnMediaClosed& OnClosed() override
	{
		return ClosedEvent;
	}

	DECLARE_DERIVED_EVENT(FAvfMediaPlayer, IMediaPlayer::FOnMediaOpened, FOnMediaOpened);
	virtual FOnMediaOpened& OnOpened() override
	{
		return OpenedEvent;
	}

	DECLARE_DERIVED_EVENT(FAvfMediaPlayer, IMediaPlayer::FOnMediaOpenFailed, FOnMediaOpenFailed);
	virtual FOnMediaOpenFailed& OnOpenFailed() override
	{
		return OpenFailedEvent;
	}

	DECLARE_DERIVED_EVENT(FWmfMediaPlayer, IMediaPlayer::FOnTracksChanged, FOnTracksChanged);
	virtual FOnTracksChanged& OnTracksChanged() override
	{
		return TracksChangedEvent;
	}

public:

    // FTickerObjectBase interface
    virtual bool Tick( float DeltaTime ) override;
	
protected:
	
	/**
	 * Whether the media player should tick in this frame.
	 *
	 * @return true if the player should advance, false otherwise
	 */
	bool ShouldTick() const;
	
	/**
	 * Has the video completed it's playthrough
	 */
	bool ReachedEnd() const;

private:

	/** The available audio tracks. */
	TArray<IMediaAudioTrackRef> AudioTracks;

	/** The available caption tracks. */
	TArray<IMediaCaptionTrackRef> CaptionTracks;

    /** The AVFoundation media player */
    AVPlayer* MediaPlayer;

    /** The player item which the media player uses to progress. */
    AVPlayerItem* PlayerItem;

    /** The duration of the media. */
    FTimespan Duration;

    /** The current time of the playback. */
    FTimespan CurrentTime;

    /** The URL of the currently opened media. */
    FString MediaUrl;

    /** The current playback rate. */
    float CurrentRate;

	/** Cocoa helper object we can use to keep track of ns property changes in our media items */
	FMediaHelper* MediaHelper;

	/** The available video tracks. */
	TArray<IMediaVideoTrackRef> VideoTracks;
    
    /** Should the video loop to the beginning at completion */
    bool bLoop;

private:

	/** Holds an event delegate that is invoked when media is being unloaded. */
	FOnMediaClosed ClosedEvent;

	/** Holds an event delegate that is invoked when media has been loaded. */
	FOnMediaOpened OpenedEvent;

	/** Holds an event delegate that is invoked when media failed to open. */
	FOnMediaOpenFailed OpenFailedEvent;

	/** Holds an event delegate that is invoked when the media tracks have changed. */
	FOnTracksChanged TracksChangedEvent;
};
