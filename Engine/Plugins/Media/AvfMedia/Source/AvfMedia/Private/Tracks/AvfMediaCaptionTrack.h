// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Caption Track implementation using the AV Foundation Framework.
 */
class FAvfMediaCaptionTrack
	: public FAvfMediaTrack
	, public IMediaTrackCaptionDetails
{
public:

	/** Default constructor. */
	FAvfMediaCaptionTrack()
		: FAvfMediaTrack()
	{ }

public:

	// IMediaTrackCaptionDetails interface

public:

	// IMediaTrack interface

	virtual const IMediaTrackAudioDetails& GetAudioDetails() const override
	{
		check(false); // not an audio track
		return (IMediaTrackAudioDetails&)*this;
	}

	virtual const IMediaTrackCaptionDetails& GetCaptionDetails() const override
	{
		return *this;
	}

	virtual EMediaTrackTypes GetType() const override
	{
		return EMediaTrackTypes::Caption;
	}

	virtual const IMediaTrackVideoDetails& GetVideoDetails() const override
	{
		check(false); // not a video track
		return (IMediaTrackVideoDetails&)*this;
	}
};
