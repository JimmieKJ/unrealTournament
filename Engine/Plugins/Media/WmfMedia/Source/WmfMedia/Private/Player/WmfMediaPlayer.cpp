// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "WmfMediaPlayer.h"
#include "WmfMediaResolver.h"
#include "WmfMediaSession.h"
#include "WmfMediaSampler.h"
#include "WmfMediaUtils.h"
#include "Serialization/ArrayReader.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"

#if WMFMEDIA_SUPPORTED_PLATFORM

#include "AllowWindowsPlatformTypes.h"

/* FWmfVideoPlayer structors
 *****************************************************************************/

FWmfMediaPlayer::FWmfMediaPlayer()
	: Duration(0)
{
	MediaSession = new FWmfMediaSession(EMediaState::Closed);
	Resolver = new FWmfMediaResolver;

	Tracks.OnSelectionChanged().AddLambda([this]() {
		if (MediaSession != NULL)
		{
			MediaSession->SetTopology(Tracks.CreateTopology());
		}
	});
}


FWmfMediaPlayer::~FWmfMediaPlayer()
{
	Close();
}


/* FTickerObjectBase interface
 *****************************************************************************/

bool FWmfMediaPlayer::Tick(float DeltaTime)
{
	TFunction<void()> Task;

	while (GameThreadTasks.Dequeue(Task))
	{
		Task();
	}

	return true;
}


/* IMediaPlayer interface
 *****************************************************************************/

void FWmfMediaPlayer::Close()
{
	Resolver->Cancel();

	if ((MediaSession == NULL) || (MediaSession->GetState() == EMediaState::Closed))
	{
		return;
	}

	// unregister session callbacks
	MediaSession->OnSessionEvent().RemoveAll(this);

	if (MediaSession->GetState() == EMediaState::Playing)
	{
		MediaEvent.Broadcast(EMediaEvent::PlaybackSuspended);
	}

	// close and release session
	MediaSession->SetState(EMediaState::Closed);
	MediaSession = new FWmfMediaSession(EMediaState::Closed);

	// release media source
	if (MediaSource != NULL)
	{
		MediaSource->Shutdown();
		MediaSource = NULL;
	}

	// reset player
	Duration = 0;
	Info.Empty();
	MediaUrl = FString();
	Tracks.Reset();

	// notify listeners
	MediaEvent.Broadcast(EMediaEvent::TracksChanged);
	MediaEvent.Broadcast(EMediaEvent::MediaClosed);
}


IMediaControls& FWmfMediaPlayer::GetControls()
{
	return *MediaSession;
}


FString FWmfMediaPlayer::GetInfo() const
{
	return Info;
}


FName FWmfMediaPlayer::GetName() const
{
	static FName PlayerName(TEXT("WmfMedia"));
	return PlayerName;
}


IMediaOutput& FWmfMediaPlayer::GetOutput()
{
	return Tracks;
}


FString FWmfMediaPlayer::GetStats() const
{
	FString Result;

	Tracks.AppendStats(Result);

	return Result;
}


IMediaTracks& FWmfMediaPlayer::GetTracks()
{
	return Tracks;
}


FString FWmfMediaPlayer::GetUrl() const
{
	return MediaUrl;
}


bool FWmfMediaPlayer::Open(const FString& Url, const IMediaOptions& Options)
{
	Close();

	if (Url.IsEmpty())
	{
		return false;
	}

	bool Resolving = false;

	// open local files via platform file system
	if (Url.StartsWith(TEXT("file://")))
	{
		TSharedPtr<FArchive, ESPMode::ThreadSafe> Archive;
		const TCHAR* FilePath = &Url[7];

		if (Options.GetMediaOption("PrecacheFile", false))
		{
			FArrayReader* Reader = new FArrayReader;

			if (FFileHelper::LoadFileToArray(*Reader, FilePath))
			{
				Archive = MakeShareable(Reader);
			}
			else
			{
				delete Reader;
			}
		}
		else
		{
			Archive = MakeShareable(IFileManager::Get().CreateFileReader(FilePath));
		}

		if (!Archive.IsValid())
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Failed to open media file: %s"), FilePath);

			return false;
		}

		Resolving = Resolver->ResolveByteStream(Archive.ToSharedRef(), Url, *this);
	}
	else
	{
		Resolving = Resolver->ResolveUrl(Url, *this);
	}

	if (Resolving)
	{
		MediaSession = new FWmfMediaSession(EMediaState::Preparing);
	}

	return Resolving;
}


bool FWmfMediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions& Options)
{
	Close();

	if ((Archive->TotalSize() == 0) || OriginalUrl.IsEmpty())
	{
		return false;
	}

	return Resolver->ResolveByteStream(Archive, OriginalUrl, *this);
}


/* IWmfMediaResolverCallbacks interface
 *****************************************************************************/

void FWmfMediaPlayer::ProcessResolveComplete(TComPtr<IUnknown> SourceObject, FString ResolvedUrl)
{
	GameThreadTasks.Enqueue([=]() {
		MediaEvent.Broadcast(
			InitializeMediaSession(SourceObject, ResolvedUrl)
				? EMediaEvent::MediaOpened
				: EMediaEvent::MediaOpenFailed
		);
	});
}


void FWmfMediaPlayer::ProcessResolveFailed(FString FailedUrl)
{
	GameThreadTasks.Enqueue([=]() {
		MediaSession = new FWmfMediaSession(EMediaState::Closed);
		MediaEvent.Broadcast(EMediaEvent::MediaOpenFailed);
	});
}


/* FWmfMediaPlayer implementation
 *****************************************************************************/

bool FWmfMediaPlayer::InitializeMediaSession(IUnknown* SourceObject, const FString& SourceUrl)
{
	if (SourceObject == nullptr)
	{
		return false;
	}

	UE_LOG(LogWmfMedia, Verbose, TEXT("Initializing media session for %s"), *SourceUrl);

	// create presentation descriptor
	TComPtr<IMFMediaSource> MediaSourceObject;
	{
		HRESULT Result = SourceObject->QueryInterface(IID_PPV_ARGS(&MediaSourceObject));

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Error, TEXT("Failed to query media source: %s"), *WmfMedia::ResultToString(Result));
			return false;
		}
	}

	TComPtr<IMFPresentationDescriptor> PresentationDescriptor;
	{
		HRESULT Result = MediaSourceObject->CreatePresentationDescriptor(&PresentationDescriptor);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Error, TEXT("Failed to create presentation descriptor: %s"), *WmfMedia::ResultToString(Result));
			return false;
		}
	}

	// initialize tracks
	if (!Tracks.Initialize(*MediaSourceObject, *PresentationDescriptor, Info))
	{
		UE_LOG(LogWmfMedia, Error, TEXT("Failed to initialize tracks"));
		return false;
	}

	// get media duration
	UINT64 PresentationDuration = 0;
	PresentationDescriptor->GetUINT64(MF_PD_DURATION, &PresentationDuration);
	Duration = FTimespan(PresentationDuration);

	// create session
	MediaUrl = SourceUrl;
	MediaSource = MediaSourceObject;
	MediaSession = new FWmfMediaSession(Duration);

	if (MediaSession->GetState() == EMediaState::Error)
	{
		return false;
	}

	MediaSession->OnSessionEvent().AddRaw(this, &FWmfMediaPlayer::HandleSessionEvent);
	MediaEvent.Broadcast(EMediaEvent::TracksChanged);

	return true;
}


/* FWmfMediaPlayer callbacks
 *****************************************************************************/

void FWmfMediaPlayer::HandleSessionEvent(MediaEventType EventType)
{
	TOptional<EMediaEvent> Event;

	// process event
	switch (EventType)
	{
	case MESessionClosed:
	case MEError:
		Tracks.SetPaused(true);
		Tracks.FlushSinks();
		break;

	case MESessionStarted:
		if (!FMath::IsNearlyZero(MediaSession->GetRate()))
		{
			Event = EMediaEvent::PlaybackResumed;
			Tracks.SetPaused(false);
		}
		break;

	case MESessionEnded:
		Event = EMediaEvent::PlaybackEndReached;
		Tracks.SetPaused(true);
		Tracks.FlushSinks();
		break;

	case MESessionStopped:
		Event = EMediaEvent::PlaybackSuspended;
		Tracks.SetPaused(true);
		break;
	}

	// forward event to game thread
	if (Event.IsSet())
	{
		GameThreadTasks.Enqueue([=]() {
			MediaEvent.Broadcast(Event.GetValue());
		});
	}
}


#include "HideWindowsPlatformTypes.h"

#endif //WMFMEDIA_SUPPORTED_PLATFORM
