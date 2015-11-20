// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AllowWindowsPlatformTypes.h"


class FWmfMediaAudioTrack
	: public FWmfMediaTrack
	, public IMediaAudioTrack
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InMediaType The media type information for this track.
	 * @param InPresentationDescriptor The descriptor of the presentation that this stream belongs to.
	 * @param InSampler The sample grabber callback object to use.
	 * @param InStreamIndex The stream's index number in the presentation.
	 * @param InStreamDescriptor The stream's descriptor object.
	 */
	FWmfMediaAudioTrack( IMFMediaType* InMediaType, IMFPresentationDescriptor* InPresentationDescriptor, FWmfMediaSampler* InSampler, IMFStreamDescriptor* InStreamDescriptor, DWORD InStreamIndex )
		: FWmfMediaTrack(InPresentationDescriptor, InSampler, InStreamDescriptor, InStreamIndex)
	{
		NumChannels = ::MFGetAttributeUINT32(InMediaType, MF_MT_AUDIO_NUM_CHANNELS, 0);
		SamplesPerSecond = ::MFGetAttributeUINT32(InMediaType, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
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


#include "HideWindowsPlatformTypes.h"
