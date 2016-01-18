// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tickable.h"
#include "AndroidJavaMediaPlayer.h"


/*
	Implement media playback using the Android MediaPlayer interface.
*/
class FAndroidMediaPlayer
	: public IMediaInfo
	, public IMediaPlayer
	, public FTickableGameObject
{
public:
	
	FAndroidMediaPlayer();
	~FAndroidMediaPlayer();

public:

	// IMediaInfo interface

	virtual FTimespan GetDuration() const override;
	virtual TRange<float> GetSupportedRates(
		EMediaPlaybackDirections Direction,
		bool Unthinned) const override;
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

	DECLARE_DERIVED_EVENT(FAndroidMediaPlayer, IMediaPlayer::FOnMediaClosed, FOnMediaClosed);
	virtual FOnMediaClosed& OnClosed() override
	{
		return ClosedEvent;
	}

	DECLARE_DERIVED_EVENT(FAndroidMediaPlayer, IMediaPlayer::FOnMediaOpened, FOnMediaOpened);
	virtual FOnMediaOpened& OnOpened() override
	{
		return OpenedEvent;
	}

	DECLARE_DERIVED_EVENT(FAndroidMediaPlayer, IMediaPlayer::FOnMediaOpenFailed, FOnMediaOpenFailed);
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

	// FTickableObjectRenderThread

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	class MediaTrack;
	class VideoTrack;
	class AudioTrack;

private:

	// Local state to track where the Java side media player is at.
	enum class EMediaState
	{
		Idle, Initialized, Preparing, Prepared, Started,
		Paused, Stopped, PlaybackCompleted, End, Error
	};

	/** The available audio tracks. */
	TArray<IMediaAudioTrackRef> AudioTracks;

	/** The available caption tracks. */
	TArray<IMediaCaptionTrackRef> CaptionTracks;

	/** Holds an event delegate that is invoked when media has been closed. */
	FOnMediaClosed ClosedEvent;

	/** The Java side media interface. */
	TSharedPtr<FJavaAndroidMediaPlayer> JavaMediaPlayer;

	/** Our understanding of the state of the Java media player. */
	EMediaState MediaState;

	/** Holds an event delegate that is invoked when media has been opened. */
	FOnMediaOpened OpenedEvent;

	/** Holds an event delegate that is invoked when media failed to open. */
	FOnMediaOpenFailed OpenFailedEvent;

	/** Holds an event delegate that is invoked when the media tracks have changed. */
	FOnTracksChanged TracksChangedEvent;

	/** Currently opened media. */
	FString MediaUrl;

	/** The available video tracks. */
	TArray<IMediaVideoTrackRef> VideoTracks;
};
