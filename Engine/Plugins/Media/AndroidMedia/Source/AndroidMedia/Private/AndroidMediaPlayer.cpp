// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AndroidMediaPCH.h"
#include "AndroidMediaPlayer.h"
#include "AndroidApplication.h"
#include "AndroidJava.h"
#include "AndroidFile.h"
#include "Paths.h"
#include "RenderingThread.h"
#include "RenderResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogAndroidMediaPlayer, Log, All);

#define LOCTEXT_NAMESPACE "FAndroidMediaModule"

// Enables directly copying into bound textures instead of using slower glReadPixels
#define ENABLE_BINDTEXTURE	1

class FAndroidMediaPlayer::MediaTrack
	: public IMediaStream
{
public:

	MediaTrack(FAndroidMediaPlayer & Player, int Index)
		: MediaPlayer(Player)
		, TrackIndex(Index)
		, TrackIsEnabled(true)
	{
	}

	virtual void AddSink(const IMediaSinkRef& Sink)
	{
		Sinks.AddUnique(IMediaSinkWeakPtr(Sink));
	}

	virtual bool Disable()
	{
		TrackIsEnabled = false;
		return MediaPlayer.MediaState != EMediaState::Error;
	}

	virtual bool Enable()
	{
		TrackIsEnabled = true;
		return MediaPlayer.MediaState != EMediaState::Error;
	}

	virtual FString GetLanguage() const
	{
		return TEXT("und");
	}

	virtual FString GetName() const
	{
		return FPaths::GetBaseFilename(MediaPlayer.MediaUrl);
	}

	virtual bool IsEnabled() const
	{
		return TrackIsEnabled;
	}

	virtual bool IsMutuallyExclusive(const IMediaStreamRef& Other) const
	{
		return false;
	}

	virtual bool IsProtected() const
	{
		return false;
	}

	virtual void RemoveSink(const IMediaSinkRef& Sink)
	{
		Sinks.RemoveSingle(IMediaSinkWeakPtr(Sink));
	}

	virtual void Tick(float DeltaTime)
	{
	}

	virtual void ProcessMediaSample(void* SampleBuffer, uint32 SampleSize, FTimespan SampleDuration, FTimespan SampleTime)
	{
		/**
		if (SampleSize > 0 && SampleBuffer != nullptr)
		{
			int8 * buffer = (int8*)SampleBuffer;
			int32 test_sample
				= (buffer[SampleSize / 4] << 16)
				+ (buffer[SampleSize / 2] << 8)
				+ buffer[SampleSize * 3 / 4];
			static int32 previous_sample = 0;
			if (previous_sample != test_sample)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FJavaAndroidMediaMediaPlayer::ProcessMediaSample: sample test = %d"), test_sample);
				previous_sample = test_sample;
			}
		}
		**/
		for (IMediaSinkWeakPtr& SinkPtr : Sinks)
		{
			IMediaSinkPtr Sink = SinkPtr.Pin();

			if (Sink.IsValid())
			{
				Sink->ProcessMediaSample(SampleBuffer, SampleSize, FTimespan(SampleDuration), FTimespan(SampleTime));
			}
		}
	}

protected:

	FAndroidMediaPlayer & MediaPlayer;
	int TrackIndex;
	bool TrackIsEnabled;

	// The collection of registered media sinks.
	TArray<IMediaSinkWeakPtr> Sinks;
};


class FAndroidMediaPlayer::VideoTrack
	: public FAndroidMediaPlayer::MediaTrack
	, public IMediaVideoTrack
{
public:

	VideoTrack(FAndroidMediaPlayer & Player, int Index)
		: MediaTrack(Player, Index)
		, LastFramePosition(-1)
	{
	}

	virtual bool Disable()
	{
		if (MediaTrack::Disable())
		{
			MediaPlayer.JavaMediaPlayer->SetVideoEnabled(false);
			return true;
		}
		return false;
	}

	virtual bool Enable()
	{
		if (MediaTrack::Enable())
		{
			MediaPlayer.JavaMediaPlayer->SetVideoEnabled(true);
			return true;
		}
		return false;
	}

	virtual FText GetDisplayName() const
	{
		return FText::Format(LOCTEXT("VideoTrackNameFormat", "Video Track {0}"), FText::AsNumber(TrackIndex));
	}

	virtual uint32 GetBitRate() const
	{
		return 0;
	}

	virtual FIntPoint GetDimensions() const
	{
		if (MediaPlayer.MediaState != EMediaState::Error)
		{
			return FIntPoint(
				MediaPlayer.JavaMediaPlayer->GetVideoWidth(),
				MediaPlayer.JavaMediaPlayer->GetVideoHeight());
		}
		else
		{
			return FIntPoint::NoneValue;
		}
	}

	virtual float GetFrameRate() const
	{
		return 30.0f;
	}

	virtual IMediaStream& GetStream() override
	{
		return *this;
	}

#if ENABLE_BINDTEXTURE
	virtual void BindTexture(FTextureRHIParamRef BoundResource) override
	{
		BoundTextures.Add(BoundResource);
	}

	virtual void UnbindTexture(FTextureRHIParamRef BoundResource) override
	{
		BoundTextures.Remove(BoundResource);
	}

	virtual void Tick(float DeltaTime)
	{
		if (MediaPlayer.MediaState != EMediaState::Error)
		{
			int32 CurrentFramePosition
				= MediaPlayer.JavaMediaPlayer->GetCurrentPosition();
			if (LastFramePosition != CurrentFramePosition)
			{
				if (UpdateBoundTextures())
				{
					LastFramePosition = CurrentFramePosition;
				}
			}
		}
	}

#else

#if WITH_ENGINE
	virtual void BindTexture(class FRHITexture* Texture) override
	{
	}

	virtual void UnbindTexture(class FRHITexture* Texture) override
	{
	}
#endif

	virtual void Tick(float DeltaTime)
	{
		if (MediaPlayer.MediaState != EMediaState::Error)
		{
			int32 CurrentFramePosition
				= MediaPlayer.JavaMediaPlayer->GetCurrentPosition();
			if (LastFramePosition != CurrentFramePosition)
			{
				LastFramePosition = CurrentFramePosition;
				void* SampleData = 0;
				int64 SampleCount = 0;
				if (MediaPlayer.JavaMediaPlayer
						->GetVideoLastFrameData(SampleData, SampleCount))
				{
					ProcessMediaSample(
						SampleData, SampleCount,
						FTimespan::MaxValue(),
						FTimespan::FromMilliseconds(LastFramePosition));
				}
			}
		}
	}

#endif  // ENABLE_BINDTEXTURE

private:

#if ENABLE_BINDTEXTURE
	bool UpdateBoundTextures()
	{
		bool FrameAvailable = false;
		if (IsInRenderingThread())
		{
			for (auto Iter = BoundTextures.CreateIterator(); Iter; ++Iter)
			{
				FTextureRHIParamRef BoundTextureTarget = *Iter;
				int32 DestTexture = *reinterpret_cast<int32*>(BoundTextureTarget->GetNativeResource());
				if (MediaPlayer.JavaMediaPlayer->GetVideoLastFrame(DestTexture))
				{
					FrameAvailable = true;
				}
			}
		}

		return FrameAvailable;
	}
#endif  // ENABLE_BINDTEXTURE

private:

	int32 LastFramePosition;
#if ENABLE_BINDTEXTURE
	TSet<FTextureRHIParamRef> BoundTextures;
#endif  // ENABLE_BINDTEXTURE
};


class FAndroidMediaPlayer::AudioTrack
	: public FAndroidMediaPlayer::MediaTrack
	, public IMediaAudioTrack
{
public:

	AudioTrack(FAndroidMediaPlayer & Player, int Index)
		: MediaTrack(Player, Index)
	{
	}

	virtual bool Disable()
	{
		if (MediaTrack::Disable())
		{
			MediaPlayer.JavaMediaPlayer->SetAudioEnabled(false);
			return true;
		}
		return false;
	}

	virtual bool Enable()
	{
		if (MediaTrack::Enable())
		{
			MediaPlayer.JavaMediaPlayer->SetAudioEnabled(true);
			return true;
		}
		return false;
	}

	virtual FText GetDisplayName() const
	{
		return FText::Format(LOCTEXT("AudioTrackNameFormat", "Audio Track {0}"), FText::AsNumber(TrackIndex));
	}

	virtual uint32 GetNumChannels() const
	{
		return 1;
	}

	virtual uint32 GetSamplesPerSecond() const
	{
		return 0;
	}

	virtual IMediaStream& GetStream() override
	{
		return *this;
	}
};


FAndroidMediaPlayer::FAndroidMediaPlayer()
	: JavaMediaPlayer(nullptr)
	, MediaState(EMediaState::Error)
{
#if ENABLE_BINDTEXTURE
	JavaMediaPlayer = MakeShareable(new FJavaAndroidMediaPlayer(false));
#else
	JavaMediaPlayer = MakeShareable(new FJavaAndroidMediaPlayer());
#endif
	if (JavaMediaPlayer.IsValid())
	{
		MediaState = EMediaState::Idle;
	}
}


FAndroidMediaPlayer::~FAndroidMediaPlayer()
{
	Close();
	if (MediaState == EMediaState::Idle)
	{
		JavaMediaPlayer->Release();
		MediaState = EMediaState::End;
	}
}


FTimespan FAndroidMediaPlayer::GetDuration() const
{
	int32 Milliseconds = 0;
	if (MediaState == EMediaState::Prepared || MediaState == EMediaState::Started ||
		MediaState == EMediaState::Paused || MediaState == EMediaState::Stopped ||
		MediaState == EMediaState::PlaybackCompleted)
	{
		Milliseconds = JavaMediaPlayer->GetDuration();
	}
	return FTimespan::FromMilliseconds(Milliseconds);
}


TRange<float> FAndroidMediaPlayer::GetSupportedRates(
	EMediaPlaybackDirections Direction, bool Unthinned) const
{
	return TRange<float>(1.0f);
}


FString FAndroidMediaPlayer::GetUrl() const
{
	return MediaUrl;
}


bool FAndroidMediaPlayer::SupportsRate(float Rate, bool Unthinned) const
{
	return Rate == 1.0f;
}


bool FAndroidMediaPlayer::SupportsScrubbing() const
{
	return true;
}


bool FAndroidMediaPlayer::SupportsSeeking() const
{
	return true;
}


void FAndroidMediaPlayer::Close()
{
	if (MediaState == EMediaState::Prepared || MediaState == EMediaState::Started ||
		MediaState == EMediaState::Paused || MediaState == EMediaState::Stopped ||
		MediaState == EMediaState::PlaybackCompleted)
	{
		if (JavaMediaPlayer.IsValid())
		{
			JavaMediaPlayer->Stop();
			JavaMediaPlayer->Reset();
		}

		MediaUrl = FString();
		MediaState = EMediaState::Idle;
		
		AudioTracks.Reset();
		CaptionTracks.Reset();
		VideoTracks.Reset();

		MediaEvent.Broadcast(EMediaEvent::TracksChanged);
		MediaEvent.Broadcast(EMediaEvent::MediaClosed);
	}
}


const TArray<IMediaAudioTrackRef>& FAndroidMediaPlayer::GetAudioTracks() const
{
	return AudioTracks;
}


const TArray<IMediaCaptionTrackRef>& FAndroidMediaPlayer::GetCaptionTracks() const
{
	return CaptionTracks;
}


const IMediaInfo& FAndroidMediaPlayer::GetMediaInfo() const
{
	return *this;
}


float FAndroidMediaPlayer::GetRate() const
{
	return 1.0f;
}


FTimespan FAndroidMediaPlayer::GetTime() const
{
	if (MediaState != EMediaState::Error)
	{
		return FTimespan::FromMilliseconds(JavaMediaPlayer->GetCurrentPosition());
	}
	else
	{
		return FTimespan::Zero();
	}
}


const TArray<IMediaVideoTrackRef>& FAndroidMediaPlayer::GetVideoTracks() const
{
	return VideoTracks;
}


bool FAndroidMediaPlayer::IsLooping() const
{
	if (MediaState == EMediaState::Prepared || MediaState == EMediaState::Started ||
		MediaState == EMediaState::Paused || MediaState == EMediaState::Stopped ||
		MediaState == EMediaState::PlaybackCompleted)
	{
		return JavaMediaPlayer->IsLooping();
	}
	else
	{
		return false;
	}
}


bool FAndroidMediaPlayer::IsPaused() const
{
	return MediaState == EMediaState::Paused;
}


bool FAndroidMediaPlayer::IsPlaying() const
{
	if (MediaState != EMediaState::Error)
	{
		return JavaMediaPlayer->IsPlaying();
	}
	else
	{
		return false;
	}
}


bool FAndroidMediaPlayer::IsReady() const
{
	return
		MediaState == EMediaState::Prepared ||
		MediaState == EMediaState::Started ||
		MediaState == EMediaState::Paused ||
		MediaState == EMediaState::PlaybackCompleted;
}


bool FAndroidMediaPlayer::Open(const FString& Url)
{
	if (Url.IsEmpty())
	{
		return false;
	}

	if (MediaState != EMediaState::Idle)
	{
		return false;
	}

	if (Url.StartsWith(TEXT("http:")) || Url.StartsWith(TEXT("https:")) ||
		Url.StartsWith(TEXT("rtsp:")) || Url.StartsWith(TEXT("file:")))
	{
		// Direct open media at a "remote" URL.
		JavaMediaPlayer->SetDataSource(Url);
		MediaState = EMediaState::Initialized;
	}
	else
	{
		// Use the platform file layer to open the media file. We
		// need to access Android specific information to allow
		// for playing media that is embedded in the APK, OBBs,
		// and/or PAKs.

		// Construct a canonical path for the movie.
		FString MoviePath = Url;
		FPaths::NormalizeFilename(MoviePath);

		// Deal with hardcoded path from editor
		if (MoviePath.Contains(":") || MoviePath.StartsWith("/"))
		{
			int32 Index = MoviePath.Find(TEXT("/Content/Movies"));
			if (Index > 0)
			{
				Index--;
				while (Index > 0 && MoviePath[Index] != '/')
				{
					Index--;
				}
				MoviePath = "../../.." + MoviePath.Mid(Index);
			}
		}

		// Don't bother trying to play it if we can't find it.
		if (!IAndroidPlatformFile::GetPlatformPhysical().FileExists(*MoviePath))
		{
			return false;
		}

		// Get information about the movie.
		int64 FileOffset = IAndroidPlatformFile::GetPlatformPhysical().FileStartOffset(*MoviePath);
		int64 FileSize = IAndroidPlatformFile::GetPlatformPhysical().FileSize(*MoviePath);
		FString FileRootPath = IAndroidPlatformFile::GetPlatformPhysical().FileRootPath(*MoviePath);

		// Play the movie as a file or asset.
		if (IAndroidPlatformFile::GetPlatformPhysical().IsAsset(*MoviePath))
		{
			if (JavaMediaPlayer->SetDataSource(
				IAndroidPlatformFile::GetPlatformPhysical().GetAssetManager(),
				FileRootPath, FileOffset, FileSize))
			{
				MediaState = EMediaState::Initialized;
			}
		}
		else
		{
			if (JavaMediaPlayer->SetDataSource(FileRootPath, FileOffset, FileSize))
			{
				MediaState = EMediaState::Initialized;
			}
		}
	}

	if (MediaState == EMediaState::Initialized)
	{
		MediaUrl = Url;
		JavaMediaPlayer->Prepare();
		MediaState = EMediaState::Prepared;
	}

	if (MediaState == EMediaState::Prepared)
	{
		// Use the extension as a rough guess as to what tracks
		// to use.
		FString Extension = FPaths::GetExtension(MediaUrl);
		if (Extension.Equals(TEXT("3gpp"), ESearchCase::IgnoreCase) ||
			Extension.Equals(TEXT("mp4"), ESearchCase::IgnoreCase))
		{
			// For video we add video track and disable audio
			JavaMediaPlayer->SetAudioEnabled(false);
//			AudioTracks.Add(MakeShareable(new AudioTrack(*this, AudioTracks.Num())));
			VideoTracks.Add(MakeShareable(new VideoTrack(*this, VideoTracks.Num())));
		}
		else if (Extension.Equals(TEXT("aac"), ESearchCase::IgnoreCase))
		{
			AudioTracks.Add(MakeShareable(new AudioTrack(*this, AudioTracks.Num())));
		}

		MediaEvent.Broadcast(EMediaEvent::TracksChanged);
	}

	if (MediaState == EMediaState::Prepared)
	{
		MediaEvent.Broadcast(EMediaEvent::MediaOpenFailed);
	}

	return MediaState == EMediaState::Prepared;
}


bool FAndroidMediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive,
	const FString& OriginalUrl)
{
	return false;
}


bool FAndroidMediaPlayer::Seek(const FTimespan& Time)
{
	if (MediaState == EMediaState::Prepared || MediaState == EMediaState::Started ||
		MediaState == EMediaState::Paused || MediaState == EMediaState::PlaybackCompleted)
	{
		JavaMediaPlayer->SeekTo(Time.GetMilliseconds());
		return true;
	}

	return false;
}


bool FAndroidMediaPlayer::SetLooping(bool Looping)
{
	if (MediaState != EMediaState::Error)
	{
		JavaMediaPlayer->SetLooping(Looping);
		return true;
	}

	return false;
}


bool FAndroidMediaPlayer::SetRate(float Rate)
{
	switch (MediaState)
	{
	case EMediaState::Prepared:
		if (Rate > 0.0f)
		{
			JavaMediaPlayer->Start();
			MediaState = EMediaState::Started;
			MediaEvent.Broadcast(EMediaEvent::PlaybackResumed);
			return true;
		}
		else
		{
			JavaMediaPlayer->Pause();
			MediaState = EMediaState::Paused;
			MediaEvent.Broadcast(EMediaEvent::PlaybackSuspended);
			return true;
		}
		break;

	case EMediaState::Started:
		if (FMath::IsNearlyZero(Rate))
		{
			JavaMediaPlayer->Pause();
			MediaState = EMediaState::Paused;
			MediaEvent.Broadcast(EMediaEvent::PlaybackSuspended);
			return true;
		}
		break;

	case EMediaState::Paused:
		if (Rate > 0.0f)
		{
			JavaMediaPlayer->Start();
			MediaState = EMediaState::Started;
			MediaEvent.Broadcast(EMediaEvent::PlaybackResumed);
			return true;
		}
		break;

	case EMediaState::PlaybackCompleted:
		if (Rate > 0.0f)
		{
			JavaMediaPlayer->Start();
			MediaState = EMediaState::Started;
			MediaEvent.Broadcast(EMediaEvent::PlaybackResumed);
			return true;
		}
		break;
	}

	return false;
}


typedef TWeakPtr<IMediaVideoTrack, ESPMode::ThreadSafe> IMediaVideoTrackWeakPtr;


void FAndroidMediaPlayer::Tick(float DeltaTime)
{
	for (IMediaAudioTrackRef AudioTrack : AudioTracks)
	{
		static_cast<MediaTrack&>(AudioTrack->GetStream()).Tick(DeltaTime);
	}

	for (IMediaCaptionTrackRef CaptionTrack : CaptionTracks)
	{
		static_cast<MediaTrack&>(CaptionTrack->GetStream()).Tick(DeltaTime);
	}

	for (IMediaVideoTrackRef VideoTrack : VideoTracks)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			RenderTickMediaTrack, IMediaVideoTrackWeakPtr, Track, VideoTrack, float, DeltaT, DeltaTime,
			{
				IMediaVideoTrackPtr t = Track.Pin();
				if (t.IsValid())
				{
					static_cast<MediaTrack&>(t->GetStream()).Tick(DeltaT);
				}
			});
	}
}


TStatId FAndroidMediaPlayer::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FAndroidMediaPlayer, STATGROUP_Tickables);
}


bool FAndroidMediaPlayer::IsTickable() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
