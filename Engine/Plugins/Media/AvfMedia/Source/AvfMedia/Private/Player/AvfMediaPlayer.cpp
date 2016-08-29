// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AvfMediaPCH.h"
#include "AvfMediaPlayer.h"

/**
 * Cocoa class that can help us with reading player item information.
 */
@interface FAVPlayerDelegate : NSObject
{
};

/** We should only initiate a helper with a media player */
-(FAVPlayerDelegate*) initWithMediaPlayer:(FAvfMediaPlayer*)InPlayer;

/** Destructor */
-(void)dealloc;

/** Notification called when player item reaches the end of playback. */
-(void)playerItemPlaybackEndReached:(NSNotification*)Notification;

/** Reference to the media player which will be responsible for this media session */
@property FAvfMediaPlayer* MediaPlayer;

/** Flag indicating whether the media player item has reached the end of playback */
@property bool bHasPlayerReachedEnd;

@end

#if !PLATFORM_MAC
static FString ConvertToIOSPath(const FString& Filename, bool bForWrite)
{
	FString Result = Filename;
	if (Result.Contains(TEXT("/OnDemandResources/")))
	{
		return Result;
	}
	
	Result.ReplaceInline(TEXT("../"), TEXT(""));
	Result.ReplaceInline(TEXT(".."), TEXT(""));
	Result.ReplaceInline(FPlatformProcess::BaseDir(), TEXT(""));
	
	if(bForWrite)
	{
		static FString WritePathBase = FString([NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0]) + TEXT("/");
		return WritePathBase + Result;
	}
	else
	{
		// if filehostip exists in the command line, cook on the fly read path should be used
		FString Value;
		// Cache this value as the command line doesn't change...
		static bool bHasHostIP = FParse::Value(FCommandLine::Get(), TEXT("filehostip"), Value) || FParse::Value(FCommandLine::Get(), TEXT("streaminghostip"), Value);
		static bool bIsIterative = FParse::Value(FCommandLine::Get(), TEXT("iterative"), Value);
		if (bHasHostIP)
		{
			static FString ReadPathBase = FString([NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0]) + TEXT("/");
			return ReadPathBase + Result;
		}
		else if (bIsIterative)
		{
			static FString ReadPathBase = FString([NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0]) + TEXT("/");
			return ReadPathBase + Result.ToLower();
		}
		else
		{
			static FString ReadPathBase = FString([[NSBundle mainBundle] bundlePath]) + TEXT("/cookeddata/");
			return ReadPathBase + Result.ToLower();
		}
	}
	
	return Result;
}
#endif

/* FAVPlayerDelegate implementation
 *****************************************************************************/

@implementation FAVPlayerDelegate
@synthesize MediaPlayer;


-(FAVPlayerDelegate*) initWithMediaPlayer:(FAvfMediaPlayer*)InPlayer
{
	id Self = [super init];
	if (Self)
	{
		MediaPlayer = InPlayer;
	}	
	return Self;
}


/** Listener for changes in our media classes properties. */
- (void) observeValueForKeyPath:(NSString*)keyPath
		ofObject:	(id)object
		change:		(NSDictionary*)change
		context:	(void*)context
{
	if ([keyPath isEqualToString:@"status"])
	{
		if (object == (id)context)
		{
			AVPlayerItemStatus Status = ((AVPlayerItem*)object).status;
			MediaPlayer->HandleStatusNotification(Status);
		}
	}
}


- (void)dealloc
{
	[super dealloc];
}

-(void)playerItemPlaybackEndReached:(NSNotification*)Notification
{
	MediaPlayer->HandleDidReachEnd();
}

@end


/* FAvfMediaPlayer structors
 *****************************************************************************/

FAvfMediaPlayer::FAvfMediaPlayer()
{
    CurrentTime = FTimespan::Zero();
    Duration = FTimespan::Zero();
	MediaUrl = FString();
	ShouldLoop = false;
    
	MediaHelper = nil;
    MediaPlayer = nil;
	PlayerItem = nil;
	
	CurrentRate = 0.0f;
	
	State = EMediaState::Closed;
	
	bPrerolled = false;
}


void FAvfMediaPlayer::HandleStatusNotification(AVPlayerItemStatus Status)
{
	switch(Status)
	{
		case AVPlayerItemStatusReadyToPlay:
		{
			if (Duration == 0.0f || State == EMediaState::Closed)
			{
				Tracks.Initialize(PlayerItem);
			 
				AVF_GAME_THREAD_CALL(^AVF_GAME_THREAD_BLOCK{
					MediaEvent.Broadcast(EMediaEvent::TracksChanged);
					AVF_GAME_THREAD_RETURN;
				});
				
				Duration = FTimespan::FromSeconds(CMTimeGetSeconds(PlayerItem.asset.duration));
				State = (State == EMediaState::Closed) ? EMediaState::Preparing : State;
				
				AVF_GAME_THREAD_CALL(^AVF_GAME_THREAD_BLOCK{
					if (!bPrerolled)
					{
						// Preroll for playback.
						[MediaPlayer prerollAtRate:1.0f completionHandler:^(BOOL bFinished)
						{
							bPrerolled = true;
							if (bFinished)
							{
								AVF_GAME_THREAD_CALL(^AVF_GAME_THREAD_BLOCK{
									MediaEvent.Broadcast(EMediaEvent::MediaOpened);
									if (CurrentTime != FTimespan::Zero())
									{
										Seek(CurrentTime);
									}
									if(CurrentRate != 0.0f)
									{
										SetRate(CurrentRate);
									}
									AVF_GAME_THREAD_RETURN;
								});
							}
							else
							{
								State = EMediaState::Error;
								AVF_GAME_THREAD_CALL(^AVF_GAME_THREAD_BLOCK{
									MediaEvent.Broadcast(EMediaEvent::MediaOpenFailed);
									AVF_GAME_THREAD_RETURN;
								});
							}
						}];
					}
					AVF_GAME_THREAD_RETURN;
				});
			}
			break;
		}
		case AVPlayerItemStatusFailed:
		{
			if (Duration == 0.0f || State == EMediaState::Closed)
			{
				State = EMediaState::Error;
				AVF_GAME_THREAD_CALL(^AVF_GAME_THREAD_BLOCK{
					MediaEvent.Broadcast(EMediaEvent::MediaOpenFailed);
					AVF_GAME_THREAD_RETURN;
				});
			}
			else
			{
				State = EMediaState::Error;
				AVF_GAME_THREAD_CALL(^AVF_GAME_THREAD_BLOCK{
					MediaEvent.Broadcast(EMediaEvent::PlaybackSuspended);
					AVF_GAME_THREAD_RETURN;
				});
			}
			break;
		}
		case AVPlayerItemStatusUnknown:
		default:
			break;
	}
}


void FAvfMediaPlayer::HandleDidReachEnd()
{
	if (ShouldLoop)
	{
		AVF_GAME_THREAD_CALL(^AVF_GAME_THREAD_BLOCK{
			MediaEvent.Broadcast(EMediaEvent::PlaybackEndReached);
			Seek(FTimespan::FromSeconds(0.0f));
			SetRate(CurrentRate);
			AVF_GAME_THREAD_RETURN;
		});
	}
	else
	{
		State = EMediaState::Paused;
		CurrentRate = 0.0f;
		AVF_GAME_THREAD_CALL(^AVF_GAME_THREAD_BLOCK{
			Seek(FTimespan::FromSeconds(0.0f));
			MediaEvent.Broadcast(EMediaEvent::PlaybackEndReached);
			MediaEvent.Broadcast(EMediaEvent::PlaybackSuspended);
			AVF_GAME_THREAD_RETURN;
		});
	}
}


/* FTickerObjectBase interface
 *****************************************************************************/

bool FAvfMediaPlayer::Tick(float DeltaTime)
{
	if (GetState() > EMediaState::Error && Duration > 0.0f)
	{
		Tracks.Tick(DeltaTime);
	}

	return true;
}


/* IMediaControls interface
 *****************************************************************************/

FTimespan FAvfMediaPlayer::GetDuration() const
{
	return Duration;
}


float FAvfMediaPlayer::GetRate() const
{
	return CurrentRate;
}


EMediaState FAvfMediaPlayer::GetState() const
{
	return State;
}


TRange<float> FAvfMediaPlayer::GetSupportedRates(EMediaPlaybackDirections Direction, bool Unthinned) const
{
	if (Direction == EMediaPlaybackDirections::Forward)
	{
		float Max = PlayerItem.canPlayFastForward ? 8.0f : 1.0f;
		float Min = 0.0f;
		return TRange<float>(Min, Max);
	}
	else
	{
		if (PlayerItem.canPlayReverse)
		{
			float Max = PlayerItem.canPlayFastReverse ? 8.0f : 1.0f;
			float Min = 0.0f;
			return TRange<float>(Min, Max);
		}
		else
		{
			return TRange<float>::Empty();
		}
	}
}


FTimespan FAvfMediaPlayer::GetTime() const
{
	FTimespan Time = CurrentTime;
	if (MediaPlayer)
	{
		CMTime Current = [MediaPlayer currentTime];
		FTimespan DiplayTime = FTimespan::FromSeconds(CMTimeGetSeconds(Current));
		Time = FMath::Min(DiplayTime, Duration);
	}
	
	return Time;
}


bool FAvfMediaPlayer::IsLooping() const
{
	return ShouldLoop;
}


bool FAvfMediaPlayer::Seek(const FTimespan& Time)
{
	CurrentTime = Time;
	
	if (bPrerolled)
	{
		Tracks.Seek(Time);
		
		double TotalSeconds = Time.GetTotalSeconds();
		CMTime CurrentTimeInSeconds = CMTimeMakeWithSeconds(TotalSeconds, 1000);
		
		static CMTime Tolerance = CMTimeMakeWithSeconds(0.01, 1000);
		[MediaPlayer seekToTime:CurrentTimeInSeconds toleranceBefore:Tolerance toleranceAfter:Tolerance];
	}

	return true;
}


bool FAvfMediaPlayer::SetLooping(bool Looping)
{
	ShouldLoop = Looping;
	
	if (ShouldLoop)
	{
		MediaPlayer.actionAtItemEnd = AVPlayerActionAtItemEndNone;
	}
	else
	{
		MediaPlayer.actionAtItemEnd = AVPlayerActionAtItemEndPause;
	}

	return true;
}


bool FAvfMediaPlayer::SetRate(float Rate)
{
	CurrentRate = Rate;
	
	if (bPrerolled)
	{
		[MediaPlayer setRate : CurrentRate];

		Tracks.SetRate(Rate);
		
		if (FMath::IsNearlyZero(Rate))
		{
			State = EMediaState::Paused;
			MediaEvent.Broadcast(EMediaEvent::PlaybackSuspended);
		}
		else
		{
			State = EMediaState::Playing;
			MediaEvent.Broadcast(EMediaEvent::PlaybackResumed);
		}
	}

	return true;
}


bool FAvfMediaPlayer::SupportsRate(float Rate, bool Unthinned) const
{
	return GetSupportedRates(Rate >= 0.0f ? EMediaPlaybackDirections::Forward : EMediaPlaybackDirections::Reverse, Unthinned).Contains(FMath::Abs(Rate));
}


bool FAvfMediaPlayer::SupportsScrubbing() const
{
	return true;
}


bool FAvfMediaPlayer::SupportsSeeking() const
{
	return true;
}


/* IMediaPlayer interface
 *****************************************************************************/

void FAvfMediaPlayer::Close()
{
	if (GetState() > EMediaState::Closed)
	{
		CurrentTime = 0;
		MediaUrl = FString();
	
		if (PlayerItem != nil)
		{
			if(MediaHelper != nil)
			{
				[[NSNotificationCenter defaultCenter] removeObserver:MediaHelper name:AVPlayerItemDidPlayToEndTimeNotification object:PlayerItem];
			}
			[PlayerItem removeObserver:MediaHelper forKeyPath:@"status"];
			[PlayerItem release];
			PlayerItem = nil;
		}
	
		if (MediaHelper != nil)
		{
			[MediaHelper release];
			MediaHelper = nil;
		}
	
		Tracks.Reset();
		MediaEvent.Broadcast(EMediaEvent::TracksChanged);
	
		Duration = CurrentTime = FTimespan::Zero();
	
		MediaEvent.Broadcast(EMediaEvent::MediaClosed);
		
		bPrerolled = false;
	}
}


IMediaControls& FAvfMediaPlayer::GetControls()
{
	return *this;
}


FString FAvfMediaPlayer::GetInfo() const
{
	return TEXT("AvfMedia media information not implemented yet");
}


IMediaOutput& FAvfMediaPlayer::GetOutput()
{
	return Tracks;
}


FString FAvfMediaPlayer::GetStats() const
{
	FString Result;

	Tracks.AppendStats(Result);

	return Result;
}


IMediaTracks& FAvfMediaPlayer::GetTracks()
{
	return Tracks;
}


FString FAvfMediaPlayer::GetUrl() const
{
	return MediaUrl;
}


bool FAvfMediaPlayer::Open(const FString& Url, const IMediaOptions& Options)
{
	Close();

	// open media file
	NSURL* nsMediaUrl = [NSURL URLWithString: Url.GetNSString()];

	if (nsMediaUrl == nil)
	{
		UE_LOG(LogAvfMedia, Error, TEXT("Failed to open Media file:"), *Url);

		return false;
	}

	// On non-Mac Apple OSes the path is:
	//	a) case-sensitive
	//	b) relative to the cookeddata directory, not the notional GameContentDirectory which is 'virtual' and resolved by the FIOSPlatformFile calls
#if !PLATFORM_MAC
	if ([[nsMediaUrl scheme] isEqualToString:@"file"])
	{
		FString FullPath = FString([nsMediaUrl path]);
		FullPath = ConvertToIOSPath(FullPath, false);
		nsMediaUrl = [NSURL fileURLWithPath: FullPath.GetNSString() isDirectory:NO];
	}
#endif
	
	// create player instance
	MediaUrl = FPaths::GetCleanFilename(Url);
	MediaPlayer = [[AVPlayer alloc] init];

	if (!MediaPlayer)
	{
		UE_LOG(LogAvfMedia, Error, TEXT("Failed to create instance of an AVPlayer"));

		return false;
	}
	
	MediaPlayer.actionAtItemEnd = AVPlayerActionAtItemEndPause;

	// create player item
	MediaHelper = [[FAVPlayerDelegate alloc] initWithMediaPlayer:this];
	check(MediaHelper != nil);

	PlayerItem = [AVPlayerItem playerItemWithURL : nsMediaUrl];

	if (PlayerItem == nil)
	{
		UE_LOG(LogAvfMedia, Error, TEXT("Failed to open player item with Url:"), *Url);

		return false;
	}

	// load tracks
	[PlayerItem retain];

	[[PlayerItem asset] loadValuesAsynchronouslyForKeys:@[@"tracks"] completionHandler:^
	{
		NSError* Error = nil;

		if ([[PlayerItem asset] statusOfValueForKey:@"tracks" error : &Error] == AVKeyValueStatusLoaded)
		{
			[[NSNotificationCenter defaultCenter] addObserver:MediaHelper selector:@selector(playerItemPlaybackEndReached:) name:AVPlayerItemDidPlayToEndTimeNotification object:PlayerItem];
			
			// File movies will be ready now
			if (PlayerItem.status == AVPlayerItemStatusReadyToPlay)
			{
				HandleStatusNotification(PlayerItem.status);
			}
			
			// Streamed movies might not be and we want to know if it ever fails.
			[PlayerItem addObserver:MediaHelper forKeyPath:@"status" options:0 context:PlayerItem];
		}
		else if (Error != nullptr)
		{
			State = EMediaState::Error;
			
			NSDictionary *userInfo = [Error userInfo];
			NSString *errstr = [[userInfo objectForKey : NSUnderlyingErrorKey] localizedDescription];

			UE_LOG(LogAvfMedia, Warning, TEXT("Failed to load video tracks. [%s]"), *FString(errstr));
	 
			AVF_GAME_THREAD_CALL(^AVF_GAME_THREAD_BLOCK{
				MediaEvent.Broadcast(EMediaEvent::MediaOpenFailed);
				AVF_GAME_THREAD_RETURN;
			});
		}
	}];

	[MediaPlayer replaceCurrentItemWithPlayerItem : PlayerItem];
	[[MediaPlayer currentItem] seekToTime:kCMTimeZero];

	MediaPlayer.rate = 0.0;
	CurrentTime = FTimespan::Zero();
		
	return true;
}


bool FAvfMediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions& Options)
{
	return false; // not supported
}
