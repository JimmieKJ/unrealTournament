// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AvfMediaPrivatePCH.h"


/* FAvfMediaVideoTrack structors
 *****************************************************************************/

FAvfMediaVideoTrack::FAvfMediaVideoTrack( AVAssetTrack* InVideoTrack )
    : FAvfMediaTrack()
    , SyncStatus(Default)
    , BitRate(0)
{
    LatestSamples = nil;
    
    NSError* nsError = nil;
	VideoTrack = InVideoTrack;
    AVReader = [[AVAssetReader alloc] initWithAsset: [VideoTrack asset] error:&nsError];
    if( nsError != nil )
    {
        FString ErrorStr( [nsError localizedDescription] );
		UE_LOG(LogAvfMedia, Error, TEXT("Failed to create asset reader: %s"), *ErrorStr);
    }
    else
    {
        // Initialize our video output to match the format of the texture we'll be streaming to.
        NSMutableDictionary* OutputSettings = [NSMutableDictionary dictionary];
        [OutputSettings setObject: [NSNumber numberWithInt:kCVPixelFormatType_32BGRA] forKey:(NSString*)kCVPixelBufferPixelFormatTypeKey];
        
        AVVideoOutput = [[AVAssetReaderTrackOutput alloc] initWithTrack:VideoTrack outputSettings:OutputSettings];
        AVVideoOutput.alwaysCopiesSampleData = NO;
    
        FrameRate = 1.0f / [[AVVideoOutput track] nominalFrameRate];
        
        [AVReader addOutput:AVVideoOutput];
        bVideoTracksLoaded = [AVReader startReading];

        // The initial read of a frame will allow us to gather information on the video track, before we start playing the samples
        ReadFrameAtTime( kCMTimeZero, true );
    }
}


FAvfMediaVideoTrack::~FAvfMediaVideoTrack()
{
    if( AVVideoOutput != nil )
    {
        AVVideoOutput = nil;
    }
    
    if( AVReader != nil )
    {
        AVReader = nil;
    }
}


/* FAvfMediaVideoTrack interface
 *****************************************************************************/

bool FAvfMediaVideoTrack::SeekToTime( const CMTime& SeekTime )
{
    bool bSeekComplete = false;
    if( AVReader != nil )
    {
		ResetAssetReader();
		
		NSError* nsError = nil;
		AVReader = [[AVAssetReader alloc] initWithAsset: [VideoTrack asset] error:&nsError];
		if( nsError != nil )
		{
			FString ErrorStr( [nsError localizedDescription] );
			UE_LOG(LogAvfMedia, Error, TEXT("Failed to create asset reader: %s"), *ErrorStr);
		}
		
        CMTimeRange TimeRange = CMTimeRangeMake(SeekTime, [[AVReader asset] duration]);
        AVReader.timeRange = TimeRange;

		NSMutableDictionary* OutputSettings = [NSMutableDictionary dictionary];
		[OutputSettings setObject: [NSNumber numberWithInt:kCVPixelFormatType_32BGRA] forKey:(NSString*)kCVPixelBufferPixelFormatTypeKey];
		
		AVVideoOutput = [[AVAssetReaderTrackOutput alloc] initWithTrack:VideoTrack outputSettings:OutputSettings];
		AVVideoOutput.alwaysCopiesSampleData = NO;
		
		[AVReader addOutput:AVVideoOutput];
		bVideoTracksLoaded = [AVReader startReading];
		
		ReadFrameAtTime( SeekTime, false );
		
		bSeekComplete = AVReader.status == AVAssetReaderStatusReading;
    }
	
    return bSeekComplete;
}


void FAvfMediaVideoTrack::ResetAssetReader()
{
    [AVReader cancelReading];
    [AVReader release];
}


bool FAvfMediaVideoTrack::IsReady() const
{
    return AVReader != nil && AVReader.status == AVAssetReaderStatusReading;
}


void FAvfMediaVideoTrack::ReadFrameAtTime( const CMTime& AVPlayerTime, bool bInIsInitialFrameRead )
{
    check( bVideoTracksLoaded );
    
    if( AVReader.status == AVAssetReaderStatusReading )
    {
        double CurrentAVTime = CMTimeGetSeconds( AVPlayerTime );
    
        // We need to synchronize the video playback with the audio.
        // If the video frame is within tolerance (Ready), update the Texture Data.
        // If the video is Behind, throw it away and get the next one until we catch up with the ref time.
        // If the video is Ahead, update the TextureData but don't retrieve more frames until time catches up.
        while( SyncStatus != Ready )
        {
            double Delta;
            if( SyncStatus != Ahead )
            {
                LatestSamples = [AVVideoOutput copyNextSampleBuffer];
            
                if( LatestSamples == NULL )
                {
                    // Failed to get next sample buffer.
                    break;
                }
            }
        
            // Get the time stamp of the video frame
            CMTime FrameTimeStamp = CMSampleBufferGetPresentationTimeStamp(LatestSamples);
    
            // Compute delta of video frame and current playback times
            Delta = CurrentAVTime - CMTimeGetSeconds(FrameTimeStamp);
        
            if( Delta < 0 )
            {
                Delta *= -1;
                SyncStatus = Ahead;
            }
            else
            {
                SyncStatus = Behind;
            }
        
            if( Delta < FrameRate )
            {
                // Video in sync with audio
                SyncStatus = Ready;
            }
            else if(SyncStatus == Ahead)
            {
                // Video ahead of audio: stay in Ahead state, exit loop
                break;
            }
            else if( LatestSamples != nil )
            {
                // Video behind audio (Behind): stay in loop
                CFRelease(LatestSamples);
                LatestSamples = NULL;
            }
        }
    
        // Process this frame.
        if( SyncStatus == Ready )
        {
            check(LatestSamples != NULL);
    
            // Grab the pixel buffer and lock it for reading.
            CVImageBufferRef PixelBuffer = CMSampleBufferGetImageBuffer(LatestSamples);
            CVPixelBufferLockBaseAddress( PixelBuffer, kCVPixelBufferLock_ReadOnly );
    
            Width  = (uint32)CVPixelBufferGetWidth(PixelBuffer);
            Height = (uint32)CVPixelBufferGetHeight(PixelBuffer);
    
            uint8* VideoDataBuffer = (uint8*)CVPixelBufferGetBaseAddress( PixelBuffer );
            uint32 BufferSize = Width * Height * 4;
            
            // Push the pixel data for the frame to the sinks which are listening for it.
            for( IMediaSinkWeakPtr& SinkPtr : Sinks )
            {
                IMediaSinkPtr NextSink = SinkPtr.Pin();
                if( NextSink.IsValid() )
                {
                    NextSink->ProcessMediaSample( VideoDataBuffer, BufferSize, FTimespan::MaxValue(), FTimespan::FromSeconds(CurrentAVTime));
                }
            }
    
            CVPixelBufferUnlockBaseAddress( PixelBuffer, kCVPixelBufferLock_ReadOnly );
            
            // If we have are reading the initial frame, do not discard the latest samples.
            if( bInIsInitialFrameRead == false )
            {
                CFRelease(LatestSamples);
                LatestSamples= nil;
            }
        }
    
        if( SyncStatus != Ahead )
        {
            // Reset status;
            SyncStatus = bInIsInitialFrameRead ? Ready : Default;
        }
    }
}
