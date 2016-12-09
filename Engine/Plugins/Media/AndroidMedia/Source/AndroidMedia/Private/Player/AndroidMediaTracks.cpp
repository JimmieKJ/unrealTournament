// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AndroidMediaTracks.h"
#include "AndroidJavaMediaPlayer.h"
#include "Misc/ScopeLock.h"
#include "../AndroidMediaLog.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "IMediaAudioSink.h"
#include "IMediaOverlaySink.h"
#include "IMediaTextureSink.h"

#define LOCTEXT_NAMESPACE "FAndroidMediaTracks"


/* FAndroidMediaTracks structors
 *****************************************************************************/

FAndroidMediaTracks::FAndroidMediaTracks()
	: AudioSink(nullptr)
	, OverlaySink(nullptr)
	, VideoSink(nullptr)
	, SelectedAudioTrack(INDEX_NONE)
	, SelectedCaptionTrack(INDEX_NONE)
	, SelectedVideoTrack(INDEX_NONE)
	, LastFramePosition(INDEX_NONE)
	, PlaybackLooped(false)
{ }


FAndroidMediaTracks::~FAndroidMediaTracks()
{
	FlushRenderingCommands();
	Reset();
}


/* FAndroidMediaTracks interface
 *****************************************************************************/

void FAndroidMediaTracks::Initialize(TSharedRef<FJavaAndroidMediaPlayer, ESPMode::ThreadSafe> InJavaMediaPlayer, FString& OutInfo)
{
	Reset();

	FScopeLock Lock(&CriticalSection);

	JavaMediaPlayer = InJavaMediaPlayer;
	LastFramePosition = INDEX_NONE;
	PlaybackLooped = false;

	InJavaMediaPlayer->GetAudioTracks(AudioTracks);
	InJavaMediaPlayer->GetCaptionTracks(CaptionTracks);
	InJavaMediaPlayer->GetVideoTracks(VideoTracks);

	for (FJavaAndroidMediaPlayer::FVideoTrack Track : VideoTracks)
	{
		OutInfo += FString::Printf(TEXT("Stream %i\n"), Track.Index);
		OutInfo += TEXT("    Type: Video\n");
//		OutInfo += FString::Printf(TEXT("    Duration: %s\n"), *FTimespan::FromMilliseconds(Track->StreamInfo.duration).ToString());
		OutInfo += FString::Printf(TEXT("    MimeType: %s\n"), *Track.MimeType);
		OutInfo += FString::Printf(TEXT("    Language: %s\n"), *Track.Language);
		OutInfo += FString::Printf(TEXT("    FrameRate: %0.1f\n"), Track.FrameRate);
		OutInfo += FString::Printf(TEXT("    Dimensions: %i x %i\n"), Track.Dimensions.X, Track.Dimensions.Y);
		OutInfo += TEXT("\n");
	}

	for (FJavaAndroidMediaPlayer::FAudioTrack Track : AudioTracks)
	{
		OutInfo += FString::Printf(TEXT("Stream %i\n"), Track.Index);
		OutInfo += TEXT("    Type: Audio\n");
//		OutInfo += FString::Printf(TEXT("    Duration: %s\n"), *FTimespan::FromMilliseconds(Track->StreamInfo.duration).ToString());
		OutInfo += FString::Printf(TEXT("    MimeType: %s\n"), *Track.MimeType);
		OutInfo += FString::Printf(TEXT("    Language: %s\n"), *Track.Language);
		OutInfo += FString::Printf(TEXT("    Channels: %i\n"), Track.Channels);
		OutInfo += FString::Printf(TEXT("    Sample Rate: %i Hz\n"), Track.SampleRate);
		OutInfo += TEXT("\n");
	}

	for (FJavaAndroidMediaPlayer::FCaptionTrack Track : CaptionTracks)
	{
		OutInfo += FString::Printf(TEXT("Stream %i\n"), Track.Index);
		OutInfo += TEXT("    Type: Caption\n");
		OutInfo += FString::Printf(TEXT("    MimeType: %s\n"), *Track.MimeType);
		OutInfo += FString::Printf(TEXT("    Language: %s\n"), *Track.Language);
		OutInfo += TEXT("\n");
	}
}


void FAndroidMediaTracks::Reset()
{
	FScopeLock Lock(&CriticalSection);

	SelectedAudioTrack = INDEX_NONE;
	SelectedCaptionTrack = INDEX_NONE;
	SelectedVideoTrack = INDEX_NONE;

	AudioTracks.Empty();
	CaptionTracks.Empty();
	VideoTracks.Empty();

	JavaMediaPlayer.Reset();
}


void FAndroidMediaTracks::Tick()
{
	UpdateAudioSink();
	UpdateOverlaySink();

	if (VideoSink != nullptr)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(AndroidMediaTracksUpdateVideoSink,
			FAndroidMediaTracks*, This, this,
			{
				This->UpdateVideoSink();
			});
	}
}


bool FAndroidMediaTracks::DidPlaybackLoop()
{
	bool Result = PlaybackLooped;
	if (Result)
	{
		PlaybackLooped = false;
	}
	return Result;
}


/* IMediaOutput interface
 *****************************************************************************/

void FAndroidMediaTracks::SetAudioSink(IMediaAudioSink* Sink)
{
	FScopeLock Lock(&CriticalSection);

	if (Sink != AudioSink)
	{
		if (AudioSink != nullptr)
		{
			AudioSink->ShutdownAudioSink();
		}

		AudioSink = Sink;
		InitializeAudioSink();
	}
}


void FAndroidMediaTracks::SetOverlaySink(IMediaOverlaySink* Sink)
{
	// not implemented yet
}


void FAndroidMediaTracks::SetVideoSink(IMediaTextureSink* Sink)
{
	FScopeLock Lock(&CriticalSection);

	if (Sink != VideoSink)
	{
		if (VideoSink != nullptr)
		{
			VideoSink->ShutdownTextureSink();
		}

		VideoSink = Sink;
		InitializeVideoSink();
	}
}


/* IMediaTracks interface
 *****************************************************************************/

uint32 FAndroidMediaTracks::GetAudioTrackChannels(int32 TrackIndex) const
{
	return AudioTracks.IsValidIndex(TrackIndex) ? AudioTracks[TrackIndex].Channels : 0;
}


uint32 FAndroidMediaTracks::GetAudioTrackSampleRate(int32 TrackIndex) const
{
	return AudioTracks.IsValidIndex(TrackIndex) ? AudioTracks[TrackIndex].SampleRate : 0;
}


int32 FAndroidMediaTracks::GetNumTracks(EMediaTrackType TrackType) const
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


int32 FAndroidMediaTracks::GetSelectedTrack(EMediaTrackType TrackType) const
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


FText FAndroidMediaTracks::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		if (AudioTracks.IsValidIndex(TrackIndex))
		{
			return FText::FromString(AudioTracks[TrackIndex].DisplayName);
		}
		break;

	case EMediaTrackType::Caption:
		if (CaptionTracks.IsValidIndex(TrackIndex))
		{
			return FText::FromString(CaptionTracks[TrackIndex].DisplayName);
		}
		break;

	case EMediaTrackType::Video:
		if (VideoTracks.IsValidIndex(TrackIndex))
		{
			return FText::FromString(VideoTracks[TrackIndex].DisplayName);
		}

	default:
		break;
	}

	return FText::GetEmpty();
}


FString FAndroidMediaTracks::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		if (AudioTracks.IsValidIndex(TrackIndex))
		{
			return AudioTracks[TrackIndex].Language;
		}
		break;

	case EMediaTrackType::Caption:
		if (CaptionTracks.IsValidIndex(TrackIndex))
		{
			return CaptionTracks[TrackIndex].Language;
		}
		break;

	case EMediaTrackType::Video:
		if (VideoTracks.IsValidIndex(TrackIndex))
		{
			return VideoTracks[TrackIndex].Language;
		}

	default:
		break;
	}

	return FString();
}


FString FAndroidMediaTracks::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
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

	default:
		break;
	}

	return FString();
}


uint32 FAndroidMediaTracks::GetVideoTrackBitRate(int32 TrackIndex) const
{
	return VideoTracks.IsValidIndex(TrackIndex) ? VideoTracks[TrackIndex].BitRate : 0;
}


FIntPoint FAndroidMediaTracks::GetVideoTrackDimensions(int32 TrackIndex) const
{
	return VideoTracks.IsValidIndex(TrackIndex)
		? VideoTracks[TrackIndex].Dimensions
		: FIntPoint(0, 0);
}


float FAndroidMediaTracks::GetVideoTrackFrameRate(int32 TrackIndex) const
{
	return VideoTracks.IsValidIndex(TrackIndex) ? VideoTracks[TrackIndex].FrameRate : 0;
}


bool FAndroidMediaTracks::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	FScopeLock Lock(&CriticalSection);

	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		if (AudioTracks.IsValidIndex(TrackIndex))
		{
			if (!JavaMediaPlayer->SelectTrack(AudioTracks[TrackIndex].Index))
			{
				return false;
			}

			SelectedAudioTrack = TrackIndex;
			InitializeAudioSink();

			return true;
		}
		break;

	case EMediaTrackType::Caption:
		if (CaptionTracks.IsValidIndex(TrackIndex))
		{
			if (!JavaMediaPlayer->SelectTrack(CaptionTracks[TrackIndex].Index))
			{
				return false;
			}

			SelectedCaptionTrack = TrackIndex;
		}
		break;

	case EMediaTrackType::Video:
		if (TrackIndex == INDEX_NONE)
		{
			JavaMediaPlayer->SetVideoEnabled(false);
			SelectedVideoTrack = TrackIndex;

			return true;
		}
		
		if (VideoTracks.IsValidIndex(TrackIndex))
		{
			if (!JavaMediaPlayer->SelectTrack(VideoTracks[TrackIndex].Index))
			{
				return false;
			}

			JavaMediaPlayer->SetVideoEnabled(true);
			SelectedVideoTrack = TrackIndex;
			InitializeVideoSink();

			return true;
		}
		break;
	}

	return false;
}


/* FAndroidMediaTracks implementation
 *****************************************************************************/

void FAndroidMediaTracks::InitializeAudioSink()
{
	if ((AudioSink == nullptr) || (SelectedAudioTrack == INDEX_NONE))
	{
		return;
	}

	const auto& AudioTrack = AudioTracks[SelectedAudioTrack];
	AudioSink->InitializeAudioSink(AudioTrack.Channels, AudioTrack.SampleRate);
}


void FAndroidMediaTracks::InitializeOverlaySink()
{
	if ((OverlaySink == nullptr) || (SelectedCaptionTrack == INDEX_NONE))
	{
		return;
	}

	const auto& CaptionTrack = CaptionTracks[SelectedCaptionTrack];
	OverlaySink->InitializeOverlaySink();
}


void FAndroidMediaTracks::InitializeVideoSink()
{
	if ((VideoSink == nullptr) || (SelectedVideoTrack == INDEX_NONE))
	{
		return;
	}

	const auto& VideoTrack = VideoTracks[SelectedVideoTrack];
	VideoSink->InitializeTextureSink(VideoTrack.Dimensions, VideoTrack.Dimensions, EMediaTextureSinkFormat::CharBGRA, EMediaTextureSinkMode::Unbuffered);
}


void FAndroidMediaTracks::UpdateAudioSink()
{
	// android currently doesn't have an API for extracting audio
}


void FAndroidMediaTracks::UpdateOverlaySink()
{
	// not implemented yet
}


void FAndroidMediaTracks::UpdateVideoSink()
{
	FScopeLock Lock(&CriticalSection);

	if (!JavaMediaPlayer.IsValid())
	{
		return;
	}

	// only update if playback position changed
	int32 CurrentFramePosition = JavaMediaPlayer->GetCurrentPosition();

	if (LastFramePosition == CurrentFramePosition)
	{
		return;
	}

	// deal with resolution changes (usually from streams)
	if (JavaMediaPlayer->DidResolutionChange())
	{
		FIntPoint Dimensions = FIntPoint(JavaMediaPlayer->GetVideoWidth(), JavaMediaPlayer->GetVideoHeight());
		if (SelectedVideoTrack != INDEX_NONE)
		{
			// The video track dimensions need updating
			VideoTracks[SelectedVideoTrack].Dimensions = Dimensions;
		}
		VideoSink->InitializeTextureSink(Dimensions, Dimensions, EMediaTextureSinkFormat::CharBGRA, EMediaTextureSinkMode::Unbuffered);
	}

	// update sink
#if WITH_ENGINE
	FRHITexture* Texture = VideoSink->GetTextureSinkTexture();

	if (Texture != nullptr)
	{
		int32 Resource = *reinterpret_cast<int32*>(Texture->GetNativeResource());

		if (JavaMediaPlayer->GetVideoLastFrame(Resource))
		{
			if (CurrentFramePosition < LastFramePosition)
			{
				PlaybackLooped = true;
			}
			LastFramePosition = CurrentFramePosition;
		}
	}
#else
	void* Buffer = VideoSink->AcquireTextureSinkBuffer();
	int64 SampleCount = 0;

	if ((Buffer != nullptr) && JavaMediaPlayer->GetVideoLastFrameData(Buffer, SampleCount))
	{
		VideoSink->ReleaseTextureSinkBuffer();
		VideoSink->DisplayTextureSinkBuffer();

		if (CurrentFramePosition < LastFramePosition)
		{
			PlaybackLooped = true;
		}
		LastFramePosition = CurrentFramePosition;
	}
#endif
}
