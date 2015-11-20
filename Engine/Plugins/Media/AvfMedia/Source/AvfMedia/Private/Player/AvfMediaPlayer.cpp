// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AvfMediaPrivatePCH.h"

#if PLATFORM_MAC
#include "CocoaThread.h"
#endif


/* FMediaHelper implementation
 *****************************************************************************/
@implementation FMediaHelper
@synthesize bIsPlayerItemReady;
@synthesize MediaPlayer;

-(FMediaHelper*) initWithMediaPlayer:(AVPlayer*)InPlayer
{
	MediaPlayer = InPlayer;
	bIsPlayerItemReady = false;
	
	self = [super init];
	return self;
}


/** Listener for changes in our media classes properties. */
- (void) observeValueForKeyPath:(NSString*)keyPath
					ofObject:	(id)object
					change:		(NSDictionary*)change
					context:	(void*)context
{
	if( [keyPath isEqualToString:@"status"] )
	{
		if( object == [MediaPlayer currentItem] )
		{
			bIsPlayerItemReady = ([MediaPlayer currentItem].status == AVPlayerItemStatusReadyToPlay);
		}
	}
}


- (void)dealloc
{
	[MediaPlayer release];
	[super dealloc];
}

@end


/* FAvfMediaPlayer structors
 *****************************************************************************/

FAvfMediaPlayer::FAvfMediaPlayer()
{
    CurrentTime = FTimespan::Zero();
    Duration = FTimespan::Zero();
	MediaUrl = FString();
    
    PlayerItem = nil;
    MediaPlayer = nil;
	MediaHelper = nil;
	
	CurrentRate = 0.0f;
}


/* IMediaInfo interface
 *****************************************************************************/

FTimespan FAvfMediaPlayer::GetDuration() const
{
	return Duration;
}


TRange<float> FAvfMediaPlayer::GetSupportedRates( EMediaPlaybackDirections Direction, bool Unthinned ) const
{
	float MaxRate = 1.0f;
	float MinRate = 0.0f;
    
	return TRange<float>(MinRate, MaxRate);
}


FString FAvfMediaPlayer::GetUrl() const
{
	return MediaUrl;
}


bool FAvfMediaPlayer::SupportsRate( float Rate, bool Unthinned ) const
{
    return GetSupportedRates(EMediaPlaybackDirections::Forward, true).Contains( Rate );
}


bool FAvfMediaPlayer::SupportsScrubbing() const
{
	return false;
}


bool FAvfMediaPlayer::SupportsSeeking() const
{
	return true;
}


/* IMediaPlayer interface
 *****************************************************************************/

void FAvfMediaPlayer::Close()
{
    CurrentTime = 0;
	MediaUrl = FString();
    
    if( PlayerItem != nil )
    {
		[PlayerItem removeObserver:MediaHelper forKeyPath:@"status"];
		[PlayerItem release];
        PlayerItem = nil;
    }

	if( MediaHelper )
	{
		[MediaHelper release];
		MediaHelper = nil;
	}
		
	AudioTracks.Reset();
	CaptionTracks.Reset();
	VideoTracks.Reset();

	TracksChangedEvent.Broadcast();
    
    Duration = CurrentTime = FTimespan::Zero();
    
#if PLATFORM_MAC
    GameThreadCall(^{
#elif PLATFORM_IOS
    // Report back to the game thread whether this succeeded.
    [FIOSAsyncTask CreateTaskWithBlock : ^ bool(void){
#endif
        // Displatch on the gamethread that we have closed the video.
        ClosedEvent.Broadcast();
         
#if PLATFORM_IOS
        return true;
    }];
#elif PLATFORM_MAC
    });
#endif

}


const TArray<IMediaAudioTrackRef>& FAvfMediaPlayer::GetAudioTracks() const
{
	return AudioTracks;
}


const TArray<IMediaCaptionTrackRef>& FAvfMediaPlayer::GetCaptionTracks() const
{
	return CaptionTracks;
}


const IMediaInfo& FAvfMediaPlayer::GetMediaInfo() const
{
	return *this;
}


float FAvfMediaPlayer::GetRate() const
{
    return CurrentRate;
}


FTimespan FAvfMediaPlayer::GetTime() const
{
    return CurrentTime;
}


const TArray<IMediaVideoTrackRef>& FAvfMediaPlayer::GetVideoTracks() const
{
	return VideoTracks;
}


bool FAvfMediaPlayer::IsLooping() const 
{
    return bLoop;
}


bool FAvfMediaPlayer::IsPaused() const
{
    return FMath::IsNearlyZero( CurrentRate );
}


bool FAvfMediaPlayer::IsPlaying() const
{
	if ((AudioTracks.Num() > 0) && (CaptionTracks.Num() > 0) && (VideoTracks.Num() > 0))
	{
		return false;
	}

    return (MediaPlayer != nil) && !FMath::IsNearlyZero(CurrentRate) && !ReachedEnd();
}


bool FAvfMediaPlayer::IsReady() const
{
    // To be ready, we need the AVPlayer setup
	if ((MediaPlayer == nil) || ([MediaPlayer status] != AVPlayerStatusReadyToPlay))
	{
		return false;
	}

	// A player item setup and ready to stream,
	if ((MediaHelper == nil) || (![MediaHelper bIsPlayerItemReady]))
	{
		return false;
	}
    
    // and all tracks to be setup and ready
    for( const IMediaAudioTrackRef& AudioTrack : AudioTracks )
    {
        FAvfMediaTrack& AVFTrack = (FAvfMediaTrack&)AudioTrack.Get();
		if (!AVFTrack.IsReady())
		{
			return false;
        }
    }

	for( const IMediaCaptionTrackRef& CaptionTrack : CaptionTracks )
    {
        FAvfMediaTrack& AVFTrack = (FAvfMediaTrack&)CaptionTrack.Get();
		if (!AVFTrack.IsReady())
		{
			return false;
        }
    }

	for( const IMediaVideoTrackRef& VideoTrack : VideoTracks )
    {
        FAvfMediaVideoTrack& AVFTrack = (FAvfMediaVideoTrack&)VideoTrack.Get();
		if (!AVFTrack.IsReady())
		{
			return false;
        }
    }

	return true;
}


/* FAvfMediaPlayer implementation
 *****************************************************************************/

bool FAvfMediaPlayer::Open( const FString& Url )
{
    // Cache off the url for use on other threads.
    FString CachedUrl = Url;
    
    Close();
    
    bool bSuccessfullyOpenedMovie = false;

    FString Ext = FPaths::GetExtension(Url);
    FString BFNStr = FPaths::GetBaseFilename(Url);
    
#if PLATFORM_MAC
    NSURL* nsMediaUrl = [NSURL fileURLWithPath:Url.GetNSString()];
#elif PLATFORM_IOS
    NSURL* nsMediaUrl = [[NSBundle mainBundle] URLForResource:BFNStr.GetNSString() withExtension:Ext.GetNSString()];
#endif
    
    if( nsMediaUrl != nil )
    {
        MediaUrl = FPaths::GetCleanFilename(Url);
        
        MediaPlayer = [[AVPlayer alloc] init];
        if( MediaPlayer )
        {
			MediaHelper = [[FMediaHelper alloc] initWithMediaPlayer: MediaPlayer];
			check( MediaHelper != nil );
			
            PlayerItem = [AVPlayerItem playerItemWithURL: nsMediaUrl];
            if( PlayerItem != nil )
            {
				[PlayerItem retain];
				[PlayerItem addObserver:MediaHelper forKeyPath:@"status" options:0 context:nil];
                Duration = FTimespan::FromSeconds( CMTimeGetSeconds( PlayerItem.asset.duration ) );

                [[PlayerItem asset] loadValuesAsynchronouslyForKeys: @[@"tracks"] completionHandler:^
                {
                    NSError* Error = nil;
                    if( [[PlayerItem asset] statusOfValueForKey:@"tracks" error:&Error] == AVKeyValueStatusLoaded )
                    {
                        NSArray* AssetVideoTracks = [[PlayerItem asset] tracksWithMediaType:AVMediaTypeVideo];
                        if( AssetVideoTracks.count > 0 )
                        {
                            AVAssetTrack* VideoTrack = [AssetVideoTracks objectAtIndex: 0];
							FAvfMediaVideoTrack* NewTrack = new FAvfMediaVideoTrack(VideoTrack);
							if (NewTrack->IsReady())
							{
								VideoTracks.Add( MakeShareable( NewTrack ) );
#if PLATFORM_MAC
								GameThreadCall(^{
#elif PLATFORM_IOS
									// Report back to the game thread whether this succeeded.
									[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void){
#endif
										// Displatch on the gamethread that we have opened the video.
										TracksChangedEvent.Broadcast();
										OpenedEvent.Broadcast( CachedUrl );
#if PLATFORM_IOS
										return true;
									}];
#elif PLATFORM_MAC
								});
#endif
							}
							else
							{
								delete NewTrack;
							}
                        }
                    }
                    else
                    {
                        NSDictionary *userInfo = [Error userInfo];
                        NSString *errstr = [[userInfo objectForKey : NSUnderlyingErrorKey] localizedDescription];
                        UE_LOG(LogAvfMedia, Display, TEXT("Failed to load video tracks. [%s]"), *FString(errstr));
                    }
                }];
            
                [MediaPlayer replaceCurrentItemWithPlayerItem: PlayerItem];
            
                [[MediaPlayer currentItem] seekToTime:kCMTimeZero];
                MediaPlayer.rate = 0.0;
				CurrentTime = FTimespan::Zero();
            
                bSuccessfullyOpenedMovie = true;
            }
            else
            {
                UE_LOG(LogAvfMedia, Warning, TEXT("Failed to open player item with Url:"), *Url);
            }
        }
        else
        {
            UE_LOG(LogAvfMedia, Error, TEXT("Failed to create instance of an AVPlayer"));
        }
    }
    else
    {
        UE_LOG(LogAvfMedia, Warning, TEXT("Failed to open Media file:"), *Url);
    }
    
    
    return bSuccessfullyOpenedMovie;
}


bool FAvfMediaPlayer::Open( const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl )
{
    return false;
}


bool FAvfMediaPlayer::Seek( const FTimespan& Time )
{
    // We need to find a suitable way to seek using avplayer and AVAssetReader,
    CurrentTime = Time;
	
	double TotalSeconds = CurrentTime.GetTotalSeconds();
	CMTime CurrentTimeInSeconds = CMTimeMake(TotalSeconds, 1);
	
	[MediaPlayer seekToTime:CurrentTimeInSeconds];
	[[MediaPlayer currentItem] seekToTime:CurrentTimeInSeconds];
	
    for (IMediaVideoTrackRef& VideoTrack : VideoTracks)
    {
        FAvfMediaVideoTrack& AVFTrack = (FAvfMediaVideoTrack&)VideoTrack.Get();
        AVFTrack.SeekToTime(CurrentTimeInSeconds);
    }

    return false;
}


bool FAvfMediaPlayer::SetLooping( bool bInLooping )
{
    bLoop = bInLooping;
	return bLoop;
}


bool FAvfMediaPlayer::SetRate( float Rate )
{
    CurrentRate = Rate;
    [MediaPlayer setRate: CurrentRate];
    return true;
}


bool FAvfMediaPlayer::ReachedEnd() const
{
	bool bHasReachedEnd = false;
	for( const IMediaVideoTrackRef& VideoTrack : VideoTracks )
	{
		FAvfMediaVideoTrack& AVFTrack = (FAvfMediaVideoTrack&)VideoTrack.Get();
		if (AVFTrack.ReachedEnd())
		{
			bHasReachedEnd = true;
		}
	}
	return bHasReachedEnd;
}


bool FAvfMediaPlayer::ShouldTick() const
{
	return IsReady() || ReachedEnd();
}


/* FTickerBase interface
 *****************************************************************************/

bool FAvfMediaPlayer::Tick( float DeltaTime )
{
	if (ShouldTick())
	{
		if (IsPlaying())
		{
			for (IMediaVideoTrackRef& VideoTrack : VideoTracks)
			{
				FAvfMediaVideoTrack& AVFTrack = (FAvfMediaVideoTrack&)VideoTrack.Get();
				if (!AVFTrack.ReadFrameAtTime([[MediaPlayer currentItem] currentTime]))
				{
					if(bLoop && ReachedEnd())
					{
						Seek(FTimespan(0));
						SetRate(CurrentRate);
					}
					else
					{
						if(!ReachedEnd())
						{
							UE_LOG(LogAvfMedia, Display, TEXT("Failed to read video track for "), *MediaUrl);
						}
						return false;
					}
				}
				else
				{
					CurrentTime = FTimespan::FromSeconds( CMTimeGetSeconds([[MediaPlayer currentItem] currentTime]) );
				}
			}
		}
	}
	
    return true;
}


/* FAvfMediaPlayer implementation
 *****************************************************************************/

