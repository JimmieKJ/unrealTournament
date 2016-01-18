// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AllowWindowsPlatformTypes.h"


class FWmfMediaCaptionTrack
	: public FWmfMediaTrack
	, public IMediaCaptionTrack
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
	FWmfMediaCaptionTrack( IMFMediaType* InMediaType, IMFPresentationDescriptor* InPresentationDescriptor, FWmfMediaSampler* InSampler, IMFStreamDescriptor* InStreamDescriptor, DWORD InStreamIndex )
		: FWmfMediaTrack(InPresentationDescriptor, InSampler, InStreamDescriptor, InStreamIndex)
	{ }

public:

	// IMediaCaptionTrack interface

	virtual IMediaStream& GetStream() override
	{
		return *this;
	}
};


#include "HideWindowsPlatformTypes.h"
