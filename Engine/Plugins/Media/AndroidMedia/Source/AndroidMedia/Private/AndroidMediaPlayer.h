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

	// IMediaInfo interface

	virtual FTimespan GetDuration() const override;
	virtual TRange<float> GetSupportedRates(
		EMediaPlaybackDirections Direction,
		bool Unthinned) const override;
	virtual FString GetUrl() const override;
	virtual bool SupportsRate(float Rate, bool Unthinned) const override;
	virtual bool SupportsScrubbing() const override;
	virtual bool SupportsSeeking() const override;

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
	virtual bool Open(const FString& Url) override;
	virtual bool Open(const TSharedRef<TArray<uint8>>& Buffer,
		const FString& OriginalUrl) override;
	virtual bool Seek(const FTimespan& Time) override;
	virtual bool SetLooping(bool Looping) override;
	virtual bool SetRate(float Rate) override;

	DECLARE_DERIVED_EVENT(
		FAndroidMediaPlayer, IMediaPlayer::FOnMediaClosed, FOnMediaClosed);
	virtual FOnMediaClosed& OnClosed() override
	{
		return ClosedEvent;
	}

	DECLARE_DERIVED_EVENT(
		FAndroidMediaPlayer, IMediaPlayer::FOnMediaOpened, FOnMediaOpened);
	virtual FOnMediaOpened& OnOpened() override
	{
		return OpenedEvent;
	}

	// FTickableObjectRenderThread

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

private:

	class MediaTrack;
	class VideoTrack;
	class AudioTrack;

	// Local state to track where the Java side media player is at.
	enum class EMediaState
	{
		Idle, Initialized, Preparing, Prepared, Started,
		Paused, Stopped, PlaybackCompleted, End, Error
	};

	// Our understanding of the state of the Java media player.
	EMediaState MediaState;

	// Holds an event delegate that is invoked when media has been closed.
	FOnMediaClosed ClosedEvent;

	// Holds an event delegate that is invoked when media has been opened.
	FOnMediaOpened OpenedEvent;

	// The Java side media interface.
	TSharedPtr<FJavaAndroidMediaPlayer> JavaMediaPlayer;

	// Currently opened media.
	FString MediaUrl;

	// The pseudo-tracks in the media.
	TArray<IMediaTrackRef> Tracks;
};
