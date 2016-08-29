// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AvfMediaPCH.h"
#include "AvfMediaTracks.h"

#if WITH_ENGINE
#include "RenderingThread.h"
#include "RHI.h"
#endif

@interface FAVPlayerItemLegibleOutputPushDelegate : NSObject<AVPlayerItemLegibleOutputPushDelegate>
{
	FAvfMediaTracks* Tracks;
}
- (id)initWithMediaTracks:(FAvfMediaTracks*)Tracks;
- (void)legibleOutput:(AVPlayerItemLegibleOutput *)output didOutputAttributedStrings:(NSArray<NSAttributedString *> *)strings nativeSampleBuffers:(NSArray *)nativeSamples forItemTime:(CMTime)itemTime;
@end

@implementation FAVPlayerItemLegibleOutputPushDelegate

- (id)initWithMediaTracks:(FAvfMediaTracks*)InTracks
{
	id Self = [super init];
	if (Self)
	{
		Tracks = InTracks;
	}
	return Self;
}

- (void)legibleOutput:(AVPlayerItemLegibleOutput *)Output didOutputAttributedStrings:(NSArray<NSAttributedString *> *)Strings nativeSampleBuffers:(NSArray *)NativeSamples forItemTime:(CMTime)ItemTime
{
	Tracks->HandleSubtitleCaptionStrings(Output, Strings, ItemTime);
}

@end

/**
 * Passes a CV*TextureRef or CVPixelBufferRef through to the RHI to wrap in an RHI texture without traversing system memory.
 */
class FAvfTexture2DResourceWrapper : public FResourceBulkDataInterface
{
public:
	FAvfTexture2DResourceWrapper(CFTypeRef InImageBuffer)
	: ImageBuffer(InImageBuffer)
	{
		check(ImageBuffer);
		CFRetain(ImageBuffer);
	}
	
	/**
	 * @return ptr to the resource memory which has been preallocated
	 */
	virtual const void* GetResourceBulkData() const override
	{
		return ImageBuffer;
	}
	
	/**
	 * @return size of resource memory
	 */
	virtual uint32 GetResourceBulkDataSize() const override
	{
		return ImageBuffer ? ~0u : 0;
	}
	
	/**
	 * Free memory after it has been used to initialize RHI resource
	 */
	virtual void Discard() override
	{
		delete this;
	}
	
	virtual ~FAvfTexture2DResourceWrapper()
	{
		CFRelease(ImageBuffer);
		ImageBuffer = nullptr;
	}
	
	CFTypeRef ImageBuffer;
};

/**
 * Allows for direct GPU mem allocation for texture resource from a CVImageBufferRef's system memory backing store.
 */
class FAvfTexture2DResourceMem : public FResourceBulkDataInterface
{
public:
	FAvfTexture2DResourceMem(CVImageBufferRef InImageBuffer)
	: ImageBuffer(InImageBuffer)
	{
		check(ImageBuffer);
		CFRetain(ImageBuffer);
	}
	
	/**
	 * @return ptr to the resource memory which has been preallocated
	 */
	virtual const void* GetResourceBulkData() const override
	{
		CVPixelBufferLockBaseAddress(ImageBuffer, kCVPixelBufferLock_ReadOnly);
		return CVPixelBufferGetBaseAddress(ImageBuffer);
	}
	
	/**
	 * @return size of resource memory
	 */
	virtual uint32 GetResourceBulkDataSize() const override
	{
		int32 Pitch = CVPixelBufferGetBytesPerRow(ImageBuffer);
		int32 Height = CVPixelBufferGetHeight(ImageBuffer);
		uint32 Size = (Pitch * Height);
		return Size;
	}
	
	/**
	 * Free memory after it has been used to initialize RHI resource
	 */
	virtual void Discard() override
	{
		CVPixelBufferUnlockBaseAddress(ImageBuffer, kCVPixelBufferLock_ReadOnly);
		delete this;
	}
	
	virtual ~FAvfTexture2DResourceMem()
	{
		CFRelease(ImageBuffer);
		ImageBuffer = nullptr;
	}
	
	CVImageBufferRef ImageBuffer;
};

/* FAvfMediaTracks structors
 *****************************************************************************/

FAvfMediaTracks::FAvfMediaTracks()
	: AudioSink(nullptr)
	, CaptionSink(nullptr)
	, VideoSink(nullptr)
	, PlayerItem(nullptr)
	, SelectedAudioTrack(INDEX_NONE)
	, SelectedCaptionTrack(INDEX_NONE)
	, SelectedVideoTrack(INDEX_NONE)
	, bAudioPaused(false)
	, SeekTime(-1.0)
	, bZoomed(false)
#if WITH_ENGINE && !PLATFORM_MAC
	, MetalTextureCache(nullptr)
#endif
{
	LastAudioSample = kCMTimeZero;
}


FAvfMediaTracks::~FAvfMediaTracks()
{
	Reset();
}


/* FAvfMediaTracks interface
 *****************************************************************************/

void FAvfMediaTracks::Flush()
{
	FScopeLock Lock(&CriticalSection);

	if (AudioSink != nullptr)
	{
		AudioSink->FlushAudioSink();
	}
}


void FAvfMediaTracks::Initialize(AVPlayerItem* InPlayerItem)
{
	Reset();

	FScopeLock Lock(&CriticalSection);

	PlayerItem = InPlayerItem;

	// initialize tracks
	NSArray* PlayerTracks = PlayerItem.tracks;

	NSError* Error = nil;

	for (AVPlayerItemTrack* PlayerTrack in PlayerItem.tracks)
	{
		// create track
		FTrack* Track = nullptr;
		AVAssetTrack* AssetTrack = PlayerTrack.assetTrack;
		NSString* mediaType = AssetTrack.mediaType;

		if ([mediaType isEqualToString:AVMediaTypeAudio])
		{
			PlayerTrack.enabled = false;
			
			int32 TrackIndex = AudioTracks.AddDefaulted();
			Track = &AudioTracks[TrackIndex];
			
			Track->Name = FString::Printf(TEXT("Audio Track %i"), TrackIndex);
			Track->Converter = nullptr;
			
#if PLATFORM_MAC
			// create asset reader
			AVAssetReader* Reader = [[AVAssetReader alloc] initWithAsset: [AssetTrack asset] error:&Error];
			
			if (Error != nil)
			{
				FString ErrorStr([Error localizedDescription]);
				UE_LOG(LogAvfMedia, Error, TEXT("Failed to create asset reader for track %i: %s"), AssetTrack.trackID, *ErrorStr);
				
				continue;
			}

			NSMutableDictionary* OutputSettings = [NSMutableDictionary dictionary];
			[OutputSettings setObject : [NSNumber numberWithInt : kAudioFormatLinearPCM] forKey : (NSString*)AVFormatIDKey];
			
			AVAssetReaderTrackOutput* AudioReaderOutput = [[AVAssetReaderTrackOutput alloc] initWithTrack:AssetTrack outputSettings:OutputSettings];
			check(AudioReaderOutput);
			
			AudioReaderOutput.alwaysCopiesSampleData = NO;
			AudioReaderOutput.supportsRandomAccess = YES;
			
			// Assign the track to the reader.
			[Reader addOutput:AudioReaderOutput];

			Track->Output = AudioReaderOutput;
			Track->Loaded = [Reader startReading];
			Track->Reader = Reader;
#else
			Track->Output = [PlayerTrack retain];
			Track->Loaded = true;
			Track->Reader = nil;
#endif
		}
		else if (([mediaType isEqualToString:AVMediaTypeClosedCaption]) || ([mediaType isEqualToString:AVMediaTypeSubtitle]))
		{
			FAVPlayerItemLegibleOutputPushDelegate* Delegate = [[FAVPlayerItemLegibleOutputPushDelegate alloc] init];
			AVPlayerItemLegibleOutput* Output = [AVPlayerItemLegibleOutput new];
			check(Output);
			
			// We don't want AVPlayer to render the frame, just decode it for us
			Output.suppressesPlayerRendering = YES;
			
			[Output setDelegate:Delegate queue:dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0)];

			int32 TrackIndex = CaptionTracks.AddDefaulted();
			Track = &CaptionTracks[TrackIndex];

			Track->Name = FString::Printf(TEXT("Caption Track %i"), TrackIndex);
			Track->Output = Output;
			Track->Loaded = true;
			Track->Reader = nil;
			Track->Converter = nullptr;
		}
		else if ([mediaType isEqualToString:AVMediaTypeText])
		{
			// not supported by Media Framework yet
		}
		else if ([mediaType isEqualToString:AVMediaTypeTimecode])
		{
			// not implemented yet - not sure they should be as these are SMTPE editing timecodes for iMovie/Final Cut/etc. not playback timecodes. They only make sense in editable Quicktime Movies (.mov).
		}
		else if ([mediaType isEqualToString:AVMediaTypeVideo])
		{
			NSMutableDictionary* OutputSettings = [NSMutableDictionary dictionary];
			// Mac:
			// On Mac kCVPixelFormatType_422YpCbCr8 is the preferred single-plane YUV format but for H.264 bi-planar formats are the optimal choice
			// The native RGBA format is 32ARGB but we use 32BGRA for consistency with iOS for now.
			//
			// iOS/tvOS:
			// On iOS only bi-planar kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange/kCVPixelFormatType_420YpCbCr8BiPlanarFullRange are supported for YUV so an additional conversion is required.
			// The only RGBA format is 32BGRA
			[OutputSettings setObject : [NSNumber numberWithInt : kCVPixelFormatType_32BGRA] forKey : (NSString*)kCVPixelBufferPixelFormatTypeKey];

#if WITH_ENGINE
			// Setup sharing with RHI's starting with the optional Metal RHI
			if (FPlatformMisc::HasPlatformFeature(TEXT("Metal")))
			{
				[OutputSettings setObject:[NSNumber numberWithBool:YES] forKey:(NSString*)kCVPixelBufferMetalCompatibilityKey];
			}
#if PLATFORM_MAC
			[OutputSettings setObject:[NSNumber numberWithBool:YES] forKey:(NSString*)kCVPixelBufferOpenGLCompatibilityKey];
#else
			[OutputSettings setObject:[NSNumber numberWithBool:YES] forKey:(NSString*)kCVPixelBufferOpenGLESCompatibilityKey];
#endif
#endif

			// Use unaligned rows
			[OutputSettings setObject:[NSNumber numberWithInteger:1] forKey:(NSString*)kCVPixelBufferBytesPerRowAlignmentKey];
		
			// Then create the video output object from which we will grab frames as CVPixelBuffer's
			AVPlayerItemVideoOutput* Output = [[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes:OutputSettings];
			check(Output);
			// We don't want AVPlayer to render the frame, just decode it for us
			Output.suppressesPlayerRendering = YES;

			int32 TrackIndex = VideoTracks.AddDefaulted();
			Track = &VideoTracks[TrackIndex];

			Track->Name = FString::Printf(TEXT("Video Track %i"), TrackIndex);
			Track->Output = Output;
			Track->Loaded = true;
			Track->Reader = nil;
			Track->Converter = nullptr;
		}

		if (Track == nullptr)
		{
			continue;
		}

		Track->AssetTrack = AssetTrack;
		Track->DisplayName = FText::FromString(Track->Name);
	}
}


void FAvfMediaTracks::Reset()
{
	FScopeLock Lock(&CriticalSection);

	SelectedAudioTrack = INDEX_NONE;
	SelectedCaptionTrack = INDEX_NONE;
	SelectedVideoTrack = INDEX_NONE;

	AudioTracks.Empty();
	CaptionTracks.Empty();
	VideoTracks.Empty();

	if (AudioSink != nullptr)
	{
		AudioSink->ShutdownAudioSink();
		AudioSink = nullptr;
	}

	if (CaptionSink != nullptr)
	{
		CaptionSink->ShutdownStringSink();
		CaptionSink = nullptr;
	}

	if (VideoSink != nullptr)
	{
		VideoSink->ShutdownTextureSink();
		VideoSink = nullptr;
	}

	for (FTrack& Track : AudioTracks)
	{
		if (Track.Converter)
		{
			OSStatus CAErr = AudioConverterDispose(Track.Converter);
			check(CAErr == noErr);
		}
		[Track.Output release];
		[Track.Reader release];
	}

	for (FTrack& Track : CaptionTracks)
	{
		AVPlayerItemLegibleOutput* Output = (AVPlayerItemLegibleOutput*)Track.Output;
		[Output.delegate release];
		[Track.Output release];
		[Track.Reader release];
	}

	for (FTrack& Track : VideoTracks)
	{
		[Track.Output release];
		[Track.Reader release];
	}

	AudioTracks.Empty();
	CaptionTracks.Empty();
	VideoTracks.Empty();
	
	LastAudioSample = kCMTimeZero;
	bAudioPaused = false;
	SeekTime = -1.0;
	bZoomed = false;
	
#if WITH_ENGINE && !PLATFORM_MAC
	if (MetalTextureCache)
	{
		CFRelease(MetalTextureCache);
		MetalTextureCache = nullptr;
	}
#endif
}


void FAvfMediaTracks::SetRate(float Rate)
{
	FScopeLock Lock(&CriticalSection);

	if (AudioSink != nullptr)
	{
		// Can only play sensible audio at full rate forward - when seeking, scrubbing or reversing we can't supply
		// the correct samples.
		bool bNearOne = FMath::IsNearlyEqual(Rate, 1.0f);
		
		bool bWasPaused = bAudioPaused;
		
		bAudioPaused = !bNearOne;
		
		if (bAudioPaused)
		{
#if PLATFORM_MAC
			if (!FMath::IsNearlyZero(Rate))
			{
				AudioSink->FlushAudioSink();
			}
#endif
			AudioSink->PauseAudioSink();
		}
		else
		{
#if PLATFORM_MAC
			if (bZoomed)
			{
				CMTime CurrentTime = [PlayerItem currentTime];
				FTimespan Time = FTimespan::FromSeconds(CMTimeGetSeconds(CurrentTime));
				Seek(Time);
			}
#endif
			AudioSink->ResumeAudioSink();
		}
		
		bZoomed = !bNearOne;
	}
}


void FAvfMediaTracks::Seek(const FTimespan& Time)
{
#if PLATFORM_MAC
	FScopeLock Lock(&CriticalSection);
	
	if (AudioSink && SelectedAudioTrack != INDEX_NONE)
	{
		double LastSample = CMTimeGetSeconds(LastAudioSample);
		SeekTime = Time.GetTotalSeconds();
		
		if (SeekTime < LastSample)
		{
			AVAssetReaderTrackOutput* AudioReaderOutput = (AVAssetReaderTrackOutput*)AudioTracks[SelectedAudioTrack].Output;
			check(AudioReaderOutput);
			
			LastAudioSample = CMTimeMakeWithSeconds(0, 1000);
		
			CMSampleBufferRef LatestSamples = nullptr;
			while ((LatestSamples = [AudioReaderOutput copyNextSampleBuffer]))
			{
				if (LatestSamples)
				{
					CFRelease(LatestSamples);
				}
			}
			
			CMTime Start = CMTimeMakeWithSeconds(SeekTime, 1000);
			CMTime Duration = PlayerItem.asset.duration;
			Duration = CMTimeSubtract(Duration, Start);
			
			CMTimeRange TimeRange = CMTimeRangeMake(Start, Duration);
			NSValue* Value = [NSValue valueWithBytes:&TimeRange objCType:@encode(CMTimeRange)];
			NSArray* Array = @[ Value ];
			
			[AudioReaderOutput resetForReadingTimeRanges:Array];
		}
		
		AudioSink->FlushAudioSink();
	}
#endif
}


void FAvfMediaTracks::AppendStats(FString &OutStats) const
{
	FScopeLock Lock(&CriticalSection);

	// audio tracks
	OutStats += TEXT("Audio Tracks\n");
	
	if (AudioTracks.Num() == 0)
	{
		OutStats += TEXT("    none\n");
	}
	else
	{
		for (uint32 i = 0; i < GetNumTracks(EMediaTrackType::Audio); i++)
		{
			OutStats += FString::Printf(TEXT("    %s\n"), *GetTrackDisplayName(EMediaTrackType::Audio, i).ToString());
			OutStats += FString::Printf(TEXT("        Channels: %i\n"), GetAudioTrackChannels(i));
			OutStats += FString::Printf(TEXT("        Sample Rate: %i\n\n"), GetAudioTrackSampleRate(i));
		}
	}

	// video tracks
	OutStats += TEXT("Video Tracks\n");

	if (VideoTracks.Num() == 0)
	{
		OutStats += TEXT("    none\n");
	}
	else
	{
		for (uint32 i = 0; i < GetNumTracks(EMediaTrackType::Video); i++)
		{
			OutStats += FString::Printf(TEXT("    %s\n"), *GetTrackDisplayName(EMediaTrackType::Video, i).ToString());
			OutStats += FString::Printf(TEXT("        BitRate: %i\n"), GetVideoTrackBitRate(i));
			OutStats += FString::Printf(TEXT("        Frame Rate: %f\n\n"), GetVideoTrackFrameRate(i));
		}
	}
}


bool FAvfMediaTracks::Tick(float DeltaTime)
{
	FScopeLock Lock(&CriticalSection);
	
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FAvfMediaTracksTick,
		FAvfMediaTracks*, Tracks, this,
		{
			Tracks->UpdateVideoSink();
		}
	);

#if PLATFORM_MAC
	if (AudioSink && SelectedAudioTrack != INDEX_NONE)
	{
		CMTime CurrentTime = [PlayerItem currentTime];
		
		ESyncStatus Sync = Default;

		AVAssetReaderTrackOutput* AudioReaderOutput = (AVAssetReaderTrackOutput*)AudioTracks[SelectedAudioTrack].Output;
		check(AudioReaderOutput);
		
		while (Sync < Ready)
		{
			float Delta = CMTimeGetSeconds(LastAudioSample) - CMTimeGetSeconds(CurrentTime);
			if (Delta <= 1.0f)
			{
				Sync = Behind;
				
				CMSampleBufferRef LatestSamples = [AudioReaderOutput copyNextSampleBuffer];
				if (LatestSamples)
				{
					CMTime FrameTimeStamp = CMSampleBufferGetPresentationTimeStamp(LatestSamples);
					CMTime Duration = CMSampleBufferGetOutputDuration(LatestSamples);
					
					CMTime FinalTimeStamp = CMTimeAdd(FrameTimeStamp, Duration);
					
					CMTime Seek = CMTimeMakeWithSeconds(SeekTime, 1000);
					if ((SeekTime < 0.0f) || (CMTimeCompare(Seek, FrameTimeStamp) >= 0 && CMTimeCompare(Seek, FinalTimeStamp) < 0))
					{
						SeekTime = -1.0f;
						
						CMItemCount NumSamples = CMSampleBufferGetNumSamples(LatestSamples);
						
						CMFormatDescriptionRef Format = CMSampleBufferGetFormatDescription(LatestSamples);
						check(Format);
						
						const AudioStreamBasicDescription* ASBD = CMAudioFormatDescriptionGetStreamBasicDescription(Format);
						check(ASBD);
						
						CMBlockBufferRef Buffer = CMSampleBufferGetDataBuffer(LatestSamples);
						check(Buffer);
						
						uint32 Length = CMBlockBufferGetDataLength(Buffer);
						if (Length)
						{
							uint8* Data = new uint8[Length];
							
							OSErr Err = CMBlockBufferCopyDataBytes(Buffer, 0, Length, Data);
							check(Err == noErr);
							
							UInt32 OutputLength = (NumSamples * TargetDesc.mBytesPerPacket);
							if (FMemory::Memcmp(ASBD, &TargetDesc, sizeof(AudioStreamBasicDescription)) != 0)
							{
								if (AudioTracks[SelectedAudioTrack].Converter == nullptr)
								{
									OSStatus CAErr = AudioConverterNew(ASBD, &TargetDesc, &AudioTracks[SelectedAudioTrack].Converter);
									check(CAErr == noErr);
								}
								
								uint8* ConvertedData = new uint8[Length];
								
								OSStatus CAErr = AudioConverterConvertBuffer(AudioTracks[SelectedAudioTrack].Converter, (NumSamples * ASBD->mBytesPerPacket), Data, &OutputLength, ConvertedData);
								check(CAErr == noErr);
								
								delete [] Data;
								Data = ConvertedData;
							}
							
							FTimespan DiplayTime = FTimespan::FromSeconds(CMTimeGetSeconds(FrameTimeStamp));
							AudioSink->PlayAudioSink(Data, OutputLength, DiplayTime);
							
							delete [] Data;
							
							LastAudioSample = FinalTimeStamp;
						}
					}
					
					CFRelease(LatestSamples);
				}
				else
				{
					break;
				}
			}
			else
			{
				Sync = Ready;
			}
		}
	}
#endif
	
	return true;
}

void FAvfMediaTracks::HandleSubtitleCaptionStrings(AVPlayerItemLegibleOutput* Output, NSArray<NSAttributedString*>* Strings, CMTime ItemTime)
{
	if (CaptionSink != nil && SelectedCaptionTrack != INDEX_NONE)
	{
		FScopeLock Lock(&CriticalSection);
		
		NSDictionary* DocumentAttributes = @{NSDocumentTypeDocumentAttribute:NSPlainTextDocumentType};
		FTimespan DiplayTime = FTimespan::FromSeconds(CMTimeGetSeconds(ItemTime));
	
		FString OutputString;
		bool bFirst = true;
		for (NSAttributedString* String in Strings)
		{
			if(String)
			{
				// Strip attribtues from the string - we don't care for them.
				NSRange Range = NSMakeRange(0, String.length);
				NSData* Data = [String dataFromRange:Range documentAttributes:DocumentAttributes error:NULL];
				NSString* Result = [[NSString alloc] initWithData:Data encoding:NSUTF8StringEncoding];
				
				// Append the string.
				if (!bFirst)
				{
					OutputString += TEXT("\n");
				}
				bFirst = false;
				OutputString += FString(Result);
			}
		}
		
		CaptionSink->DisplayStringSinkString(OutputString, DiplayTime);
	}
}



/* IMediaOutput interface
 *****************************************************************************/

void FAvfMediaTracks::SetAudioSink(IMediaAudioSink* Sink)
{
	if (Sink != AudioSink)
	{
		FScopeLock Lock(&CriticalSection);

		if (AudioSink != nullptr)
		{
			AudioSink->ShutdownAudioSink();
			AudioSink = nullptr;
		}

		AudioSink = Sink;	
		InitializeAudioSink();
	}
}


void FAvfMediaTracks::SetCaptionSink(IMediaStringSink* Sink)
{
	if (Sink != CaptionSink)
	{
		FScopeLock Lock(&CriticalSection);

		if (CaptionSink != nullptr)
		{
			CaptionSink->ShutdownStringSink();
			CaptionSink = nullptr;
		}

		CaptionSink = Sink;
		InitializeCaptionSink();
	}
}


void FAvfMediaTracks::SetImageSink(IMediaTextureSink* Sink)
{
	// not supported
}


void FAvfMediaTracks::SetVideoSink(IMediaTextureSink* Sink)
{
	if (Sink != VideoSink)
	{
		FScopeLock Lock(&CriticalSection);

		if (VideoSink != nullptr)
		{
			VideoSink->ShutdownTextureSink();
			VideoSink = nullptr;
		}

		VideoSink = Sink;
		InitializeVideoSink();
	}
}


/* IMediaTracks interface
 *****************************************************************************/

uint32 FAvfMediaTracks::GetAudioTrackChannels(int32 TrackIndex) const
{
	uint32 NumAudioTracks = 0;
	if (AudioTracks.IsValidIndex(TrackIndex))
	{
		checkf(AudioTracks[TrackIndex].AssetTrack.formatDescriptions.count == 1, TEXT("Can't handle non-uniform audio streams!"));
		CMFormatDescriptionRef DescRef = (CMFormatDescriptionRef)[AudioTracks[TrackIndex].AssetTrack.formatDescriptions objectAtIndex:0];
		AudioStreamBasicDescription const* Desc = CMAudioFormatDescriptionGetStreamBasicDescription(DescRef);
		if (Desc)
		{
			NumAudioTracks = Desc->mChannelsPerFrame;
		}
	}
	
	return NumAudioTracks;
}


uint32 FAvfMediaTracks::GetAudioTrackSampleRate(int32 TrackIndex) const
{
	uint32 SampleRate = 0;
	if (AudioTracks.IsValidIndex(TrackIndex))
	{
		checkf(AudioTracks[TrackIndex].AssetTrack.formatDescriptions.count == 1, TEXT("Can't handle non-uniform audio streams!"));
		CMFormatDescriptionRef DescRef = (CMFormatDescriptionRef)[AudioTracks[TrackIndex].AssetTrack.formatDescriptions objectAtIndex:0];
		AudioStreamBasicDescription const* Desc = CMAudioFormatDescriptionGetStreamBasicDescription(DescRef);
		if (Desc)
		{
			SampleRate = Desc->mSampleRate;
		}
	}
	
	return SampleRate;
}


int32 FAvfMediaTracks::GetNumTracks(EMediaTrackType TrackType) const
{
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		return AudioTracks.Num();
	case EMediaTrackType::Caption:
		return CaptionTracks.Num();
	case EMediaTrackType::Video:
		return VideoTracks.Num();
	default:
		return 0;
	}
}


int32 FAvfMediaTracks::GetSelectedTrack(EMediaTrackType TrackType) const
{
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		return SelectedAudioTrack;
	case EMediaTrackType::Caption:
		return SelectedCaptionTrack;
	case EMediaTrackType::Video:
		return SelectedVideoTrack;
	default:
		return INDEX_NONE;
	}
}


FText FAvfMediaTracks::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		if (AudioTracks.IsValidIndex(TrackIndex))
		{
			return AudioTracks[TrackIndex].DisplayName;
		}
		break;

	case EMediaTrackType::Caption:
		if (CaptionTracks.IsValidIndex(TrackIndex))
		{
			return CaptionTracks[TrackIndex].DisplayName;
		}
		break;

	case EMediaTrackType::Video:
		if (VideoTracks.IsValidIndex(TrackIndex))
		{
			return VideoTracks[TrackIndex].DisplayName;
		}
		break;

	default:
		break;
	}

	return FText::GetEmpty();
}


FString FAvfMediaTracks::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (TrackType == EMediaTrackType::Audio)
	{
		if (AudioTracks.IsValidIndex(TrackIndex))
		{
			return FString(AudioTracks[TrackIndex].AssetTrack.languageCode);
		}
	}
	else if (TrackType == EMediaTrackType::Caption)
	{
		if (CaptionTracks.IsValidIndex(TrackIndex))
		{
			return FString(CaptionTracks[TrackIndex].AssetTrack.languageCode);
		}
	}
	else if (TrackType == EMediaTrackType::Video)
	{
		if (VideoTracks.IsValidIndex(TrackIndex))
		{
			return FString(VideoTracks[TrackIndex].AssetTrack.languageCode);
		}
	}

	return FString();
}


FString FAvfMediaTracks::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		if (AudioTracks.IsValidIndex(TrackIndex))
		{
			return AudioTracks[TrackIndex].Name;
		}
		break;

	case EMediaTrackType::Caption:
		if (CaptionTracks.IsValidIndex(TrackIndex))
		{
			return CaptionTracks[TrackIndex].Name;
		}
		break;

	case EMediaTrackType::Video:
		if (VideoTracks.IsValidIndex(TrackIndex))
		{
			return VideoTracks[TrackIndex].Name;
		}
		break;

	default:
		break;
	}

	return FString();
}


uint32 FAvfMediaTracks::GetVideoTrackBitRate(int32 TrackIndex) const
{
	return VideoTracks.IsValidIndex(TrackIndex) ? [VideoTracks[TrackIndex].AssetTrack estimatedDataRate] : 0;
}


FIntPoint FAvfMediaTracks::GetVideoTrackDimensions(int32 TrackIndex) const
{
	return VideoTracks.IsValidIndex(TrackIndex)
		? FIntPoint([VideoTracks[TrackIndex].AssetTrack naturalSize].width,
					[VideoTracks[TrackIndex].AssetTrack naturalSize].height)
		: FIntPoint::ZeroValue;
}


float FAvfMediaTracks::GetVideoTrackFrameRate(int32 TrackIndex) const
{
	return VideoTracks.IsValidIndex(TrackIndex) ? 1.0f / [VideoTracks[TrackIndex].AssetTrack nominalFrameRate] : 0.0f;
}


bool FAvfMediaTracks::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	FScopeLock Lock(&CriticalSection);

	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		if ((TrackIndex == INDEX_NONE) || AudioTracks.IsValidIndex(TrackIndex))
		{
			if (TrackIndex != SelectedAudioTrack)
			{
				if (SelectedAudioTrack != INDEX_NONE)
				{
#if PLATFORM_MAC
					AVAssetReaderTrackOutput* AudioReaderOutput = (AVAssetReaderTrackOutput*)AudioTracks[SelectedAudioTrack].Output;
					check(AudioReaderOutput);
					
					CMSampleBufferRef LatestSamples = nullptr;
					while ((LatestSamples = [AudioReaderOutput copyNextSampleBuffer]))
					{
						if (LatestSamples)
						{
							CFRelease(LatestSamples);
						}
					}
					
					CMTimeRange TimeRange = CMTimeRangeMake(kCMTimeZero, PlayerItem.asset.duration);
					NSValue* Value = [NSValue valueWithBytes:&TimeRange objCType:@encode(CMTimeRange)];
					NSArray* Array = @[ Value ];
					
					[AudioReaderOutput resetForReadingTimeRanges:Array];
#else
					AVPlayerItemTrack* PlayerTrack = (AVPlayerItemTrack*)AudioTracks[SelectedAudioTrack].Output;
					check(PlayerTrack);
					PlayerTrack.enabled = false;
#endif
					SelectedAudioTrack = INDEX_NONE;
				}

//				if ((TrackIndex != INDEX_NONE) && SUCCEEDED(PresentationDescriptor->SelectStream(AudioTracks[TrackIndex].StreamIndex)))
				{
					SelectedAudioTrack = TrackIndex;
				}

				if (SelectedAudioTrack == TrackIndex)
				{
					InitializeAudioSink();
				}
			}

			return true;
		}
		break;

	case EMediaTrackType::Caption:
		if ((TrackIndex == INDEX_NONE) || CaptionTracks.IsValidIndex(TrackIndex))
		{
			if (TrackIndex != SelectedCaptionTrack)
			{
				if (SelectedCaptionTrack != INDEX_NONE)
				{
					[PlayerItem removeOutput:(AVPlayerItemOutput*)CaptionTracks[SelectedCaptionTrack].Output];
					SelectedCaptionTrack = INDEX_NONE;
				}

//				if ((TrackIndex != INDEX_NONE) && SUCCEEDED(PresentationDescriptor->SelectStream(CaptionTracks[TrackIndex].StreamIndex)))
				{
					SelectedCaptionTrack = TrackIndex;
				}

				if (SelectedCaptionTrack == TrackIndex)
				{
					InitializeCaptionSink();
				}
			}

			return true;
		}
		break;

	case EMediaTrackType::Video:
		if ((TrackIndex == INDEX_NONE) || VideoTracks.IsValidIndex(TrackIndex))
		{
			if (TrackIndex != SelectedVideoTrack)
			{
				if (SelectedVideoTrack != INDEX_NONE)
				{
					[PlayerItem removeOutput:(AVPlayerItemOutput*)VideoTracks[SelectedVideoTrack].Output];
					SelectedVideoTrack = INDEX_NONE;
				}

//				if ((TrackIndex != INDEX_NONE) && SUCCEEDED(PresentationDescriptor->SelectStream(VideoTracks[TrackIndex].StreamIndex)))
				{
					SelectedVideoTrack = TrackIndex;
				}

				if (SelectedVideoTrack == TrackIndex)
				{
					InitializeVideoSink();
				}
			}

			return true;
		}
		break;

	default:
		break;
	}

	return false;
}


/* FAvfMediaTracks implementation
 *****************************************************************************/

void FAvfMediaTracks::InitializeAudioSink()
{
	if ((AudioSink == nullptr) || (SelectedAudioTrack == INDEX_NONE))
	{
		return;
	}

#if PLATFORM_MAC
	CMFormatDescriptionRef DescRef = (CMFormatDescriptionRef)[AudioTracks[SelectedAudioTrack].AssetTrack.formatDescriptions objectAtIndex:0];
	AudioStreamBasicDescription const* ASBD = CMAudioFormatDescriptionGetStreamBasicDescription(DescRef);
	
	TargetDesc.mSampleRate = ASBD->mSampleRate;
	TargetDesc.mFormatID = kAudioFormatLinearPCM;
	TargetDesc.mFormatFlags = kAudioFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
	TargetDesc.mBytesPerPacket = ASBD->mChannelsPerFrame * sizeof(int16);
	TargetDesc.mFramesPerPacket = 1;
	TargetDesc.mBytesPerFrame = ASBD->mChannelsPerFrame * sizeof(int16);
	TargetDesc.mChannelsPerFrame = ASBD->mChannelsPerFrame;
	TargetDesc.mBitsPerChannel = 16;
	TargetDesc.mReserved = 0;
#else
	AVPlayerItemTrack* PlayerTrack = (AVPlayerItemTrack*)AudioTracks[SelectedAudioTrack].Output;
	check(PlayerTrack);
	PlayerTrack.enabled = true;
#endif

	AudioSink->InitializeAudioSink(
		GetAudioTrackChannels(SelectedAudioTrack),
		GetAudioTrackSampleRate(SelectedAudioTrack)
	);
}


void FAvfMediaTracks::InitializeCaptionSink()
{
	if ((CaptionSink == nullptr) || (SelectedCaptionTrack == INDEX_NONE))
	{
		return;
	}
	
	[PlayerItem addOutput:(AVPlayerItemOutput*)CaptionTracks[SelectedCaptionTrack].Output];

	CaptionSink->InitializeStringSink();
}


void FAvfMediaTracks::InitializeVideoSink()
{
	if ((VideoSink == nullptr) || (SelectedVideoTrack == INDEX_NONE))
	{
		return;
	}

	[PlayerItem addOutput:(AVPlayerItemOutput*)VideoTracks[SelectedVideoTrack].Output];

#if WITH_ENGINE
	VideoSink->InitializeTextureSink(
		GetVideoTrackDimensions(SelectedVideoTrack),
		EMediaTextureSinkFormat::CharBGRA,
		EMediaTextureSinkMode::Unbuffered
	);
#else
	VideoSink->InitializeTextureSink(
		GetVideoTrackDimensions(SelectedVideoTrack),
		EMediaTextureSinkFormat::CharBGRA,
		EMediaTextureSinkMode::Buffered
	);
#endif
}

void FAvfMediaTracks::UpdateVideoSink()
{
	FScopeLock Lock(&CriticalSection);
	
	if ((VideoSink != nullptr) && (SelectedVideoTrack != INDEX_NONE))
	{
		AVPlayerItemVideoOutput* Output = (AVPlayerItemVideoOutput*)VideoTracks[SelectedVideoTrack].Output;
		check(Output);
		
		CMTime OutputItemTime = [Output itemTimeForHostTime:CACurrentMediaTime()];
		if ([Output hasNewPixelBufferForItemTime:OutputItemTime])
		{
			CVPixelBufferRef Frame =  [Output copyPixelBufferForItemTime:OutputItemTime itemTimeForDisplay:nullptr];
			if(Frame)
			{
#if WITH_ENGINE
#if !PLATFORM_MAC // On iOS/tvOS we use the Metal texture cache
				if (IsMetalPlatform(GMaxRHIShaderPlatform))
				{
					if (!MetalTextureCache)
					{
						id<MTLDevice> Device = (id<MTLDevice>)GDynamicRHI->RHIGetNativeDevice();
						check(Device);
					
						CVReturn Return = CVMetalTextureCacheCreate(kCFAllocatorDefault, nullptr, Device, nullptr, &MetalTextureCache);
						check(Return == kCVReturnSuccess);
					}
					check(MetalTextureCache);
					
					int32 Width = CVPixelBufferGetWidth(Frame);
					int32 Height = CVPixelBufferGetHeight(Frame);
					
					CVMetalTextureRef TextureRef = nullptr;
					CVReturn Result = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, MetalTextureCache, Frame, nullptr, MTLPixelFormatBGRA8Unorm, Width, Height, 0, &TextureRef);
					check(Result == kCVReturnSuccess);
					check(TextureRef);
					
					FRHIResourceCreateInfo CreateInfo;
					CreateInfo.BulkData = new FAvfTexture2DResourceWrapper(TextureRef);
					CreateInfo.ResourceArray = nullptr;
					
					uint32 TexCreateFlags = 0;
					TexCreateFlags |= TexCreate_Dynamic | TexCreate_NoTiling;
					
					TRefCountPtr<FRHITexture2D> RenderTarget;
					TRefCountPtr<FRHITexture2D> ShaderResource;
					
					RHICreateTargetableShaderResource2D(Width,
														Height,
														PF_B8G8R8A8,
														1,
														TexCreateFlags,
														TexCreate_RenderTargetable,
														false,
														CreateInfo,
														RenderTarget,
														ShaderResource
														);
														
					VideoSink->UpdateTextureSinkResource(RenderTarget, ShaderResource);
					CFRelease(TextureRef);
				}
				else // Ran out of time to implement efficient OpenGLES texture upload - its running out of memory.
				/*{
					if (!OpenGLTextureCache)
					{
						EAGLContext* Context = [EAGLContext currentContext];
						check(Context);
						
						CVReturn Return = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, nullptr, Context, nullptr, &OpenGLTextureCache);
						check(Return == kCVReturnSuccess);
					}
					check(OpenGLTextureCache);
				
					int32 Width = CVPixelBufferGetWidth(Frame);
					int32 Height = CVPixelBufferGetHeight(Frame);
					
					CVOpenGLESTextureRef TextureRef = nullptr;
					CVReturn Result = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault, OpenGLTextureCache, Frame, nullptr, GL_TEXTURE_2D, GL_RGBA, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, 0, &TextureRef);
					check(Result == kCVReturnSuccess);
					check(TextureRef);
				
					FRHIResourceCreateInfo CreateInfo;
					CreateInfo.BulkData = new FAvfTexture2DResourceWrapper(TextureRef);
					CreateInfo.ResourceArray = nullptr;
					
					uint32 TexCreateFlags = 0;
					TexCreateFlags |= TexCreate_Dynamic | TexCreate_NoTiling;
					
					TRefCountPtr<FRHITexture2D> RenderTarget;
					TRefCountPtr<FRHITexture2D> ShaderResource;
					
					RHICreateTargetableShaderResource2D(Width,
														Height,
														PF_R8G8B8A8,
														1,
														TexCreateFlags,
														TexCreate_RenderTargetable,
														false,
														CreateInfo,
														RenderTarget,
														ShaderResource
														);
														
					VideoSink->UpdateTextureSinkResource(RenderTarget, ShaderResource);
					CFRelease(TextureRef);
				}*/
#endif	// On Mac we have to use IOSurfaceRef for backward compatibility - unless we update MIN_REQUIRED_VERSION to 10.11 we link against an older version of CoreVideo that doesn't support Metal.
				{
					FRHIResourceCreateInfo CreateInfo;
					if (IsMetalPlatform(GMaxRHIShaderPlatform))
					{
						// Metal can upload directly from an IOSurface to a 2D texture, so we can just wrap it.
						CreateInfo.BulkData = new FAvfTexture2DResourceWrapper(Frame);
					}
					else
					{
						// OpenGL on Mac uploads as a TEXTURE_RECTANGLE for which we have no code, so upload via system memory.
						CreateInfo.BulkData = new FAvfTexture2DResourceMem(Frame);
					}
					CreateInfo.ResourceArray = nullptr;
						
					int32 Width = CVPixelBufferGetWidth(Frame);
					int32 Height = CVPixelBufferGetHeight(Frame);
					
					uint32 TexCreateFlags = TexCreate_SRGB;
					TexCreateFlags |= TexCreate_Dynamic | TexCreate_NoTiling;
					
					TRefCountPtr<FRHITexture2D> RenderTarget;
					TRefCountPtr<FRHITexture2D> ShaderResource;
					
					RHICreateTargetableShaderResource2D(Width,
														Height,
														PF_B8G8R8A8,
														1,
														TexCreateFlags,
														TexCreate_RenderTargetable,
														false,
														CreateInfo,
														RenderTarget,
														ShaderResource
														);
														
					VideoSink->UpdateTextureSinkResource(RenderTarget, ShaderResource);
				}
#else
				int32 Pitch = CVPixelBufferGetBytesPerRow(Frame);

				CVPixelBufferLockBaseAddress(Frame, kCVPixelBufferLock_ReadOnly);
				
				uint8* VideoData = (uint8*)CVPixelBufferGetBaseAddress(Frame);
				VideoSink->UpdateTextureSinkBuffer(VideoData, Pitch);
				
				CVPixelBufferUnlockBaseAddress(Frame, kCVPixelBufferLock_ReadOnly);
#endif
				FTimespan DiplayTime = FTimespan::FromSeconds(CMTimeGetSeconds(OutputItemTime));
				VideoSink->DisplayTextureSinkBuffer(DiplayTime);
				
				CVPixelBufferRelease(Frame);
			}
		}
	}
}
