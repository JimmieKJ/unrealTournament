// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Available sample formats for media audio sinks.
 */
enum class EMediaAudioSinkFormat
{
	/** Uncompressed IEEE Floating Point samples. */
	Float,

	/** Uncompressed pulse code modulated samples.*/
	Pcm,
};


/**
 * Interface for media sinks that receive audio sample data.
 *
 * Implementations of this interface must be thread-safe as these
 * methods will be called from some random media decoder thread.
 */
class IMediaAudioSink
{
public:

	/**
	 * Discard the sink's buffer and stop audio playback.
	 *
	 * @see PauseAudioSink, PlayAudioSink, ResumeAudioSink
	 */
	virtual void FlushAudioSink() = 0;

	/**
	 * Get the sink's current number of channels.
	 *
	 * @return Number of audio channels.
	 * @see GetAudioSinkSampleRate, InitializeAudioSink
	 */
	virtual int32 GetAudioSinkChannels() const = 0;

	/**
	 * Get the sink's current sample rate.
	 *
	 * @return Samples per second.
	 * @see GetAudioSinkChannels, InitializeAudioSink
	 */
	virtual int32 GetAudioSinkSampleRate() const = 0;

	/**
	 * Initialize the sink.
	 *
	 * @param Channels Number of audio channels.
	 * @param SampleRate Number of samples per second.
	 * @return true on success, false otherwise.
	 * @see ShutdownAudioSink
	 */
	virtual bool InitializeAudioSink(uint32 Channels, uint32 SampleRate) = 0;

	/**
	 * Pause audio playback.
	 *
	 * @see FlushAudioSink, PlayAudioSink, ResumeAudioSink
	 */
	virtual void PauseAudioSink() = 0;

	/**
	 * Play the given samples.
	 *
	 * @param SampleBuffer The sample buffer.
	 * @param BufferSize The number of bytes in the buffer.
	 * @param Time The time of the sample (relative to the beginning of the media).
	 * @see FlushAudioSink, PauseAudioSink, ResumeAudioSink
	 */
	virtual void PlayAudioSink(const uint8* SampleBuffer, uint32 BufferSize, FTimespan Time) = 0;

	/**
	 * Resume audio playback.
	 *
	 * @see FlushAudioSink, PauseAudioSink, PlayAudioSink
	 */
	virtual void ResumeAudioSink() = 0;

	/**
	 * Shut down the sink.
	 *
	 * @see InitializeAudioSink
	 */
	virtual void ShutdownAudioSink() = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaAudioSink() { }
};
