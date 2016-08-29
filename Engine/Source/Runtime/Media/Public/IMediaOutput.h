// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


class IMediaAudioSink;
class IMediaStringSink;
class IMediaTextureSink;


/**
 * Interface for access to a media player's output.
 *
 * @see IMediaControls, IMediaTracks
 */
class IMediaOutput
{
public:

	/**
	 * Set the sink to receive data from the active audio track.
	 *
	 * @param Sink The sink to set, or nullptr to clear.
	 * @see SetCaptionSink, SetImageSink, SetVideoSink
	 */
	virtual void SetAudioSink(IMediaAudioSink* Sink) = 0;

	/**
	 * Set the sink to receive data from the active caption track.
	 *
	 * @param Sink The sink to set, or nullptr to clear.
	 * @see SetAudioSink, SetImageSink, SetVideoSink
	 */
	virtual void SetCaptionSink(IMediaStringSink* Sink) = 0;

	/**
	 * Set the sink to receive data from the active image track.
	 *
	 * @param Sink The sink to set, or nullptr to clear.
	 * @see SetAudioSink, SetCaptionSink, SetVideoSink
	 */
	virtual void SetImageSink(IMediaTextureSink* Sink) = 0;

	/**
	 * Set the sink to receive data from the active video track.
	 *
	 * @param Sink The sink to set, or nullptr to clear.
	 * @see SetAudioSink, SetCaptionSink, SetImageSink
	 */
	virtual void SetVideoSink(IMediaTextureSink* Sink) = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaOutput() { }
};
