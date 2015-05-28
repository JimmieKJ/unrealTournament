// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Video Track implementation using the AV Foundation Framework.
 */
class FAvfMediaVideoTrack
	: public FAvfMediaTrack
	, public IMediaTrackVideoDetails
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

	// IMediaTrackVideoDetails interface

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

public:

	// IMediaTrack interface

	virtual const IMediaTrackAudioDetails& GetAudioDetails() const override
	{
		check(false); // not an audio track
		return (IMediaTrackAudioDetails&)*this;
	}

	virtual const IMediaTrackCaptionDetails& GetCaptionDetails() const override
	{
		check(false); // not a caption track
		return (IMediaTrackCaptionDetails&)*this;
	}

	virtual const IMediaTrackVideoDetails& GetVideoDetails() const override
	{
		return *this;
	}

    virtual EMediaTrackTypes GetType() const override
    {
        return EMediaTrackTypes::Video;
    }

    virtual void AddSink( const IMediaSinkRef& Sink ) override
    {
        Sinks.Add( Sink );
    }

    virtual void RemoveSink( const IMediaSinkRef& Sink ) override
    {
        Sinks.Remove( Sink );
    }

public:

    /**
     * Read the track information for the frame at the given time. This should be provided by the audio manager to ensure the tracks are in sync.
     *
     * @param AVPlayerTime The time the AVPlayer is currently synced to.
     * @param bInIsInitialFrameRead Flag whether this is the first frame read of the asset.
     */
    void ReadFrameAtTime( const CMTime& AVPlayerTime, bool bInIsInitialFrameRead = false );

    /**
     * Set the Av Asset reader to a specified time.
     *
     * @param SeekTime The time the Track Reader should be set to.
     */
    bool SeekToTime( const CMTime& SeekTime );

    /**
     * Is this track ready to begin reading?
	 *
	 * @return true if ready, false otherwise.
     */
    bool IsReady() const;

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
