// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaVideoTrack.h"


/**
 * Video Track implementation using the AV Foundation Framework.
 */
class FAvfMediaVideoTrack
	: public FAvfMediaTrack
	, public IMediaVideoTrack
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InVideoTrack The av track information required to build this instance.
	 */
	FAvfMediaVideoTrack(AVAssetTrack* InVideoTrack);

	/** Destructor. */
    ~FAvfMediaVideoTrack();

public:

	// FAvfMediaTrack overrides
    virtual bool IsReady() const override;

public:

	// IMediaVideoTrack interface

	virtual uint32 GetBitRate() const override
	{
		return BitRate;
	}

	virtual FIntPoint GetDimensions() const override
	{
		return FIntPoint(Width, Height);
	}

	virtual float GetFrameRate() const override
	{
		return FrameRate;
	}

	virtual IMediaStream& GetStream() override
	{
		return *this;
	}

#if WITH_ENGINE
	virtual void BindTexture(class FRHITexture* Texture) override;
	virtual void UnbindTexture(class FRHITexture* Texture) override;
#endif

public:

	// IMediaStream interface

    virtual void AddSink(const IMediaSinkRef& Sink) override
    {
        Sinks.Add(Sink);
    }

    virtual void RemoveSink(const IMediaSinkRef& Sink) override
    {
        Sinks.Remove(Sink);
    }

public:

    /**
     * Read the track information for the frame at the given time. This should be provided by the audio manager to ensure the tracks are in sync.
     *
     * @param AVPlayerTime The time the AVPlayer is currently synced to.
     * @param bInIsInitialFrameRead Flag whether this is the first frame read of the asset.
     */
    bool ReadFrameAtTime(const CMTime& AVPlayerTime, bool bInIsInitialFrameRead = false);

    /**
     * Set the Av Asset reader to a specified time.
     *
     * @param SeekTime The time the Track Reader should be set to.
     */
    bool SeekToTime(const CMTime& SeekTime);
    
    /** 
     * Has the video track completed it's playthrough
     */
    bool ReachedEnd() const;

private:

    /** Reset the AVAssetReader to a different point, which will enable us to regrab frames for seeking and restarting. */
    void ResetAssetReader();

private:

	/** Reference to the av asset, itself */
	AVAssetTrack* VideoTrack;

    /** Access to the Avfoundations track information. */
    AVAssetReader* AVReader;

    /** The AVF track information routed from the avassetreader. */
    AVAssetReaderTrackOutput* AVVideoOutput;

    /** The current state of affairs for the reader in relation to core media. */
    enum ESyncStatus
    {
        Default,    // Starting state
        Ahead,      // Frame is ahead of playback cursor.
        Behind,     // Frame is behind playback cursor.
        Ready,      // Frame is within tolerance of playback cursor.
    } SyncStatus;

    /** Flag which dictates whether we have track information has been successfully loaded. */
    bool bVideoTracksLoaded;

    /** The video's average bit rate. */
    uint32 BitRate;

    /** The video's frame rate. */
    float FrameRate;

	/** The video's height in pixels. */
	uint32 Height;

	/** The video's width in pixels. */
	uint32 Width;

    /** The collection of output streams we are writing to. */
    TArray<IMediaSinkWeakPtr> Sinks;

    /** Access to the video samples */
    CMSampleBufferRef LatestSamples;
};
