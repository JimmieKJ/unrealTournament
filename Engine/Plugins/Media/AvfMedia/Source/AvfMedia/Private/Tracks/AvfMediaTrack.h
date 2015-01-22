// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Abstract base class for media tracks using the AV Foundation Framework.
 */
class FAvfMediaTrack
	: public IMediaTrack
{
public:

	/** Default constructor. */
    FAvfMediaTrack() { }

	/** Destructor. */
    ~FAvfMediaTrack() { }

public:

	// IMediaTrack interface

	virtual void AddSink( const IMediaSinkRef& Sink ) = 0;
	virtual bool Disable() override;
	virtual bool Enable() override;
	virtual FText GetDisplayName() const override;
	virtual uint32 GetIndex() const override;
	virtual FString GetLanguage() const override;
	virtual FString GetName() const override;
	virtual bool IsEnabled() const override;
	virtual bool IsMutuallyExclusive( const IMediaTrackRef& Other ) const override;
	virtual bool IsProtected() const override;
	virtual void RemoveSink( const IMediaSinkRef& Sink ) = 0;
    
private:

	/** The stream's language tag. */
	FString Language;

	/** The stream's name. */
	FString Name;

	/** Whether the stream contains protected content. */
	bool Protected;
    
	/** The stream's index number in the presentation. */
	int32 StreamIndex;
};
