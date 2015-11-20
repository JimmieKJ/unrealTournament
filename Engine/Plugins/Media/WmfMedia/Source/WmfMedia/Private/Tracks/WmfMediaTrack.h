// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AllowWindowsPlatformTypes.h"


/**
 * Abstract base class for media tracks using the Windows Media Foundation framework.
 *
 * This class is used to retrieve frame data for a particular media stream.
 * The data for each sample is written to an internal buffer after each time
 * a new sample is being requested with the @see RequestNewSample method.
 */
class FWmfMediaTrack
	: public IMediaStream
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InPresentationDescriptor The descriptor of the presentation that this stream belongs to.
	 * @param InSampler The sample grabber callback object to use.
	 * @param InStreamDescriptor The stream's descriptor object.
	 * @param InStreamIndex The stream's index number in the presentation.
	 */
	FWmfMediaTrack(
		IMFPresentationDescriptor* InPresentationDescriptor,
		FWmfMediaSampler* InSampler,
		IMFStreamDescriptor* InStreamDescriptor,
		DWORD InStreamIndex);

public:

	// IMediaStream interface

	virtual void AddSink( const IMediaSinkRef& Sink ) override;
	virtual bool Disable() override;
	virtual bool Enable() override;
	virtual FText GetDisplayName() const override;
	virtual FString GetLanguage() const override;
	virtual FString GetName() const override;
	virtual bool IsEnabled() const override;
	virtual bool IsMutuallyExclusive( const IMediaStreamRef& Other ) const override;
	virtual bool IsProtected() const override;
	virtual void RemoveSink( const IMediaSinkRef& Sink ) override;

private:

	/** The stream's language tag. */
	FString Language;

	/** The stream's name. */
	FString Name;

	/** Pointer to the presentation descriptor. */
	TComPtr<IMFPresentationDescriptor> PresentationDescriptor;

	/** Whether the stream contains protected content. */
	bool Protected;

	/** The sample grabber callback object. */
	TComPtr<FWmfMediaSampler> Sampler;

	/** Pointer to the stream's descriptor. */
	TComPtr<IMFStreamDescriptor> StreamDescriptor;

	/** The stream's index number in the presentation. */
	DWORD StreamIndex;
};


#include "HideWindowsPlatformTypes.h"
