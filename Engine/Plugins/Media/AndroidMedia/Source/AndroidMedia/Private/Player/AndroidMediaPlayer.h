// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AndroidMediaTracks.h"
#include "IMediaControls.h"
#include "IMediaPlayer.h"


class FJavaAndroidMediaPlayer;


/*
	Implement media playback using the Android MediaPlayer interface.
*/
class FAndroidMediaPlayer
	: public FTickerObjectBase
	, public IMediaControls
	, public IMediaPlayer
{
public:
	
	/** Default constructor. */
	FAndroidMediaPlayer();

	/** Virtual destructor. */
	virtual ~FAndroidMediaPlayer();

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

	/** The Java side media interface. */
	TSharedPtr<FJavaAndroidMediaPlayer, ESPMode::ThreadSafe> JavaMediaPlayer;

	/** Holds an event delegate that is invoked when a media event occurred. */
	FOnMediaEvent MediaEvent;

	/** Currently opened media. */
	FString MediaUrl;

	/** Our understanding of the state of the Java media player. */
	EMediaState State;

	/** Track collection. */
	FAndroidMediaTracks Tracks;
};
