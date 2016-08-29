// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AndroidMediaPCH.h"
#include "AndroidJavaMediaPlayer.h"
#include "AndroidMediaPlayer.h"
#include "AndroidJavaMediaPlayer.h"


#define LOCTEXT_NAMESPACE "FAndroidMediaModule"


/* FAndroidMediaPlayer structors
 *****************************************************************************/

FAndroidMediaPlayer::FAndroidMediaPlayer()
	: JavaMediaPlayer(nullptr)
{
	JavaMediaPlayer = MakeShareable(new FJavaAndroidMediaPlayer(false));
	State = JavaMediaPlayer.IsValid() ? EMediaState::Closed : EMediaState::Error;
}


FAndroidMediaPlayer::~FAndroidMediaPlayer()
{
	Close();

	if (JavaMediaPlayer.IsValid())
	{
		JavaMediaPlayer->Reset();
		JavaMediaPlayer->Release();
	}
}


/* FTickerObjectBase interface
 *****************************************************************************/

bool FAndroidMediaPlayer::Tick(float DeltaTime)
{
	if (State == EMediaState::Playing)
	{
		Tracks.Tick();

		if (!JavaMediaPlayer->IsPlaying())
		{
			//FPlatformMisc::LowLevelOutputDebugString(TEXT("STOPPED!!!!!"));
			State = EMediaState::Stopped;
			MediaEvent.Broadcast(EMediaEvent::PlaybackEndReached);
		}
		else if (Tracks.DidPlaybackLoop())
		{
			//FPlatformMisc::LowLevelOutputDebugString(TEXT("LOOPED!!!!!"));
			MediaEvent.Broadcast(EMediaEvent::PlaybackEndReached);
		}
	}

	return true;
}


/* IMediaControls interface
 *****************************************************************************/

FTimespan FAndroidMediaPlayer::GetDuration() const
{
	if (State == EMediaState::Error)
	{
		return FTimespan::Zero();
	}

	return FTimespan::FromMilliseconds(JavaMediaPlayer->GetDuration());
}


float FAndroidMediaPlayer::GetRate() const
{
	return (State == EMediaState::Playing) ? 1.0f : 0.0f;
}


EMediaState FAndroidMediaPlayer::GetState() const
{
	return State;
}


TRange<float> FAndroidMediaPlayer::GetSupportedRates(EMediaPlaybackDirections Direction, bool Unthinned) const
{
	if (Direction == EMediaPlaybackDirections::Reverse)
	{
		return TRange<float>::Empty();
	}

	return TRange<float>(1.0f);
}


FTimespan FAndroidMediaPlayer::GetTime() const
{
	if (State == EMediaState::Error)
	{
		return FTimespan::Zero();
	}

	return FTimespan::FromMilliseconds(JavaMediaPlayer->GetCurrentPosition());
}


bool FAndroidMediaPlayer::IsLooping() const
{
	return JavaMediaPlayer.IsValid() ? JavaMediaPlayer->IsLooping() : false;
}


bool FAndroidMediaPlayer::Seek(const FTimespan& Time)
{
	if ((State == EMediaState::Closed) || (State == EMediaState::Error))
	{
		return false;
	}

	JavaMediaPlayer->SeekTo(Time.GetMilliseconds());

	return true;
}


bool FAndroidMediaPlayer::SetLooping(bool Looping)
{
	if (!JavaMediaPlayer.IsValid())
	{
		return false;
	}

	JavaMediaPlayer->SetLooping(Looping);

	return true;
}


bool FAndroidMediaPlayer::SetRate(float Rate)
{
	switch (State)
	{
	case EMediaState::Playing:
		if (FMath::IsNearlyZero(Rate))
		{
			JavaMediaPlayer->Pause();
			State = EMediaState::Paused;
			MediaEvent.Broadcast(EMediaEvent::PlaybackSuspended);
			
			return true;
		}
		break;

	case EMediaState::Paused:
	case EMediaState::Stopped:
		if (Rate > 0.0f)
		{
			JavaMediaPlayer->Start();
			State = EMediaState::Playing;
			MediaEvent.Broadcast(EMediaEvent::PlaybackResumed);
			
			return true;
		}
		break;
	}

	return false;
}


bool FAndroidMediaPlayer::SupportsRate(float Rate, bool Unthinned) const
{
	return (Rate == 1.0f);
}


bool FAndroidMediaPlayer::SupportsScrubbing() const
{
	return true;
}


bool FAndroidMediaPlayer::SupportsSeeking() const
{
	return true;
}


/* IMediaPlayer interface
 *****************************************************************************/

void FAndroidMediaPlayer::Close()
{
	if (State == EMediaState::Closed)
	{
		return;
	}

	Tracks.Reset();

	if (JavaMediaPlayer.IsValid())
	{
		JavaMediaPlayer->Stop();
		JavaMediaPlayer->Reset();
	}

	MediaUrl = FString();
	State = EMediaState::Closed;

	// notify listeners
	MediaEvent.Broadcast(EMediaEvent::TracksChanged);
	MediaEvent.Broadcast(EMediaEvent::MediaClosed);
}


IMediaControls& FAndroidMediaPlayer::GetControls()
{
	return *this;
}


FString FAndroidMediaPlayer::GetInfo() const
{
	return TEXT("AndroidMedia media information not implemented yet");
}


IMediaOutput& FAndroidMediaPlayer::GetOutput()
{
	return Tracks;
}


FString FAndroidMediaPlayer::GetStats() const
{
	return TEXT("AndroidMedia stats information not implemented yet");
}


IMediaTracks& FAndroidMediaPlayer::GetTracks()
{
	return Tracks;
}


FString FAndroidMediaPlayer::GetUrl() const
{
	return MediaUrl;
}


bool FAndroidMediaPlayer::Open(const FString& Url, const IMediaOptions& Options)
{
	if (State == EMediaState::Error)
	{
		return false;
	}

	Close();

	if ((Url.IsEmpty()))
	{
		return false;
	}

	MediaUrl = Url;

	// open the media
	if (Url.StartsWith(TEXT("file://")))
	{
		FString FilePath = Url.RightChop(7);
		FPaths::NormalizeFilename(FilePath);

		IAndroidPlatformFile& PlatformFile = IAndroidPlatformFile::GetPlatformPhysical();

		// make sure that file exists
		if (!PlatformFile.FileExists(*FilePath))
		{
			UE_LOG(LogAndroidMedia, Warning, TEXT("File doesn't exist %s."), *FilePath);

			return false;
		}

		// get information about media
		int64 FileOffset = PlatformFile.FileStartOffset(*FilePath);
		int64 FileSize = PlatformFile.FileSize(*FilePath);
		FString FileRootPath = PlatformFile.FileRootPath(*FilePath);

		// play movie as a file or asset
		if (PlatformFile.IsAsset(*FilePath))
		{
			if (!JavaMediaPlayer->SetDataSource(PlatformFile.GetAssetManager(), FileRootPath, FileOffset, FileSize))
			{
				UE_LOG(LogAndroidMedia, Warning, TEXT("Failed to set data source for asset %s"), *FilePath);
				return false;
			}
		}
		else
		{
			if (!JavaMediaPlayer->SetDataSource(FileRootPath, FileOffset, FileSize))
			{
				UE_LOG(LogAndroidMedia, Warning, TEXT("Failed to set data source for file %s"), *FilePath);
				return false;
			}
		}
	}
	else
	{
		// open remote media
		JavaMediaPlayer->SetDataSource(Url);
	}

	// prepare media
	MediaUrl = Url;

	if (!JavaMediaPlayer->Prepare())
	{
		UE_LOG(LogAndroidMedia, Warning, TEXT("Failed to prepare media source %s"), *Url);
		return false;
	}

	State = EMediaState::Stopped;

	Tracks.Initialize(JavaMediaPlayer.ToSharedRef());
	Tracks.SelectTrack(EMediaTrackType::Audio, 0);
	Tracks.SelectTrack(EMediaTrackType::Video, 0);

	// notify listeners
	MediaEvent.Broadcast(EMediaEvent::TracksChanged);
	MediaEvent.Broadcast(EMediaEvent::MediaOpened);

	return true;
}


bool FAndroidMediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions& Options)
{
	// @todo AndroidMedia: implement opening media from FArchive
	return false;
}


#undef LOCTEXT_NAMESPACE
