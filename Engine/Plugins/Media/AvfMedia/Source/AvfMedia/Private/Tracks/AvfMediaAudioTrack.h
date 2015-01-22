// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Audio Track implementation using the AV Foundation Framework.
 */
class FAvfMediaAudioTrack
	: public FAvfMediaTrack
	, public IMediaTrackAudioDetails
{
public:

	/** Default constructor. */
	FAvfMediaAudioTrack()
		: FAvfMediaTrack()
	{
		NumChannels = 0;
		SamplesPerSecond = 0;
	}

public:

	// IMediaTrackAudioDetails interface
	
	virtual uint32 GetNumChannels() const override
	{
		return NumChannels;
	}

	virtual uint32 GetSamplesPerSecond() const override
	{
		return SamplesPerSecond;
	}

public:

	// IMediaTrack interface

	virtual const IMediaTrackAudioDetails& GetAudioDetails() const override
	{
		return *this;
	}

	virtual const IMediaTrackCaptionDetails& GetCaptionDetails() const override
	{
		check(false); // not a caption track
		return (IMediaTrackCaptionDetails&)*this;
	}

	virtual EMediaTrackTypes GetType() const override
	{
		return EMediaTrackTypes::Audio;
	}

	virtual const IMediaTrackVideoDetails& GetVideoDetails() const override
	{
		check(false); // not an video track
		return (IMediaTrackVideoDetails&)*this;
	}

private:

	/** The number of channels. */
	uint32 NumChannels;

	/** The number of samples per second. */
	uint32 SamplesPerSecond;
};
