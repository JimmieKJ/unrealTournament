// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaAudioTrack.h"


/**
 * Audio Track implementation using the AV Foundation Framework.
 */
class FAvfMediaAudioTrack
	: public FAvfMediaTrack
	, public IMediaAudioTrack
{
public:

	/** Default constructor. */
	FAvfMediaAudioTrack()
		: FAvfMediaTrack()
		, NumChannels(0)
		, SamplesPerSecond(0)
	{ }

public:

	// FAvfMediaTrack overrides

	virtual bool IsReady() const override
	{
		// not implemented yet
		return false;
	}

public:

	// IMediaAudioTrack interface
	
	virtual uint32 GetNumChannels() const override
	{
		return NumChannels;
	}

	virtual uint32 GetSamplesPerSecond() const override
	{
		return SamplesPerSecond;
	}

	virtual IMediaStream& GetStream() override
	{
		return *this;
	}

private:

	/** The number of channels. */
	uint32 NumChannels;

	/** The number of samples per second. */
	uint32 SamplesPerSecond;
};
