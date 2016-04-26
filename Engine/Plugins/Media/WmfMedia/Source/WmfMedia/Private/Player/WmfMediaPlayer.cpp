// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "WmfMediaPrivatePCH.h"
#include "AllowWindowsPlatformTypes.h"
#include "Async/Async.h"


/* FWmfVideoPlayer structors
 *****************************************************************************/

FWmfMediaPlayer::FWmfMediaPlayer()
	: Duration(0)
	, MediaSession(nullptr)
{
	Resolver = new(std::nothrow) FWmfMediaResolver;
}


FWmfMediaPlayer::~FWmfMediaPlayer()
{
	Close();
}


/* IMediaInfo interface
 *****************************************************************************/

FTimespan FWmfMediaPlayer::GetDuration() const
{
	return Duration;
}


TRange<float> FWmfMediaPlayer::GetSupportedRates(EMediaPlaybackDirections Direction, bool Unthinned) const
{
	if (MediaSession != NULL)
	{
		return MediaSession->GetSupportedRates(Direction, Unthinned);
	}

	return TRange<float>(0.0f);
}


FString FWmfMediaPlayer::GetUrl() const
{
	return MediaUrl;
}


bool FWmfMediaPlayer::SupportsRate(float Rate, bool Unthinned) const
{
	return (MediaSession != NULL)
		? MediaSession->IsRateSupported(Rate, Unthinned)
		: false;
}


bool FWmfMediaPlayer::SupportsScrubbing() const
{
	return ((MediaSession != NULL) && MediaSession->SupportsScrubbing());
}


bool FWmfMediaPlayer::SupportsSeeking() const
{
	return ((MediaSession != NULL) &&
			((MediaSession->GetCapabilities() & MFSESSIONCAP_SEEK) != 0) &&
			(Duration > FTimespan::Zero()));
}


/* IMediaPlayer interface
 *****************************************************************************/

void FWmfMediaPlayer::Close()
{
	Resolver->Cancel();

	if (MediaSession == NULL)
	{
		return;
	}

	MediaSession->OnError().RemoveAll(this);
	MediaSession->OnSessionEvent().RemoveAll(this);

	if (IsPlaying())
	{
		MediaEvent.Broadcast(EMediaEvent::PlaybackSuspended);
	}

	MediaSession->SetState(EMediaStates::Closed);
	MediaSession.Reset();

	if (MediaSource != NULL)
	{
		MediaSource->Shutdown();
		MediaSource = NULL;
	}

	AudioTracks.Reset();
	CaptionTracks.Reset();
	VideoTracks.Reset();

	Duration = 0;
	MediaUrl = FString();

	MediaEvent.Broadcast(EMediaEvent::TracksChanged);
	MediaEvent.Broadcast(EMediaEvent::MediaClosed);
}


const TArray<IMediaAudioTrackRef>& FWmfMediaPlayer::GetAudioTracks() const
{
	return AudioTracks;
}


const TArray<IMediaCaptionTrackRef>& FWmfMediaPlayer::GetCaptionTracks() const
{
	return CaptionTracks;
}


const IMediaInfo& FWmfMediaPlayer::GetMediaInfo() const 
{
	return *this;
}


float FWmfMediaPlayer::GetRate() const
{
	return (MediaSession != NULL)
		? MediaSession->GetRate()
		: 0.0f;
}


FTimespan FWmfMediaPlayer::GetTime() const 
{
	return (MediaSession != NULL)
		? MediaSession->GetPosition()
		: FTimespan::Zero();
}


const TArray<IMediaVideoTrackRef>& FWmfMediaPlayer::GetVideoTracks() const
{
	return VideoTracks;
}


bool FWmfMediaPlayer::IsLooping() const 
{
	return ((MediaSession != NULL) && MediaSession->IsLooping());
}


bool FWmfMediaPlayer::IsPaused() const
{
	if (MediaSession == NULL)
	{
		return false;
	}

	if (MediaSession->GetState() == EMediaStates::Playing)
	{
		return (MediaSession->GetRate() == 0.0);
	}

	return (MediaSession->GetState() == EMediaStates::Paused);
}


bool FWmfMediaPlayer::IsPlaying() const
{
	return (MediaSession != NULL) && (MediaSession->GetState() == EMediaStates::Playing) && !FMath::IsNearlyZero(MediaSession->GetRate());
}


bool FWmfMediaPlayer::IsReady() const
{
	return ((MediaSession != NULL) &&
			(MediaSession->GetState() != EMediaStates::Closed) &&
			(MediaSession->GetState() != EMediaStates::Error));
}


bool FWmfMediaPlayer::Open(const FString& Url)
{
	if (Url.IsEmpty())
	{
		return false;
	}

	return Resolver->ResolveUrl(Url, *this);
}


bool FWmfMediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl)
{
	if ((Archive->TotalSize() == 0) || OriginalUrl.IsEmpty())
	{
		return false;
	}

	return Resolver->ResolveByteStream(Archive, OriginalUrl, *this);
}


bool FWmfMediaPlayer::Seek(const FTimespan& Time)
{
	return ((MediaSession != NULL) && MediaSession->SetPosition(Time));
}


bool FWmfMediaPlayer::SetLooping(bool Looping)
{
	if (MediaSession == NULL)
	{
		return false;
	}

	MediaSession->SetLooping(Looping);

	return true;
}


bool FWmfMediaPlayer::SetRate(float Rate)
{
	if (MediaSession == NULL)
	{
		return false;
	}

	return MediaSession->SetRate(Rate);
}


/* IWmfMediaResolverCallbacks interface
 *****************************************************************************/

void FWmfMediaPlayer::ProcessResolveComplete(TComPtr<IUnknown> SourceObject, FString ResolvedUrl)
{
	// forward event to game thread
	AsyncTask(ENamedThreads::GameThread, [=]() {
		MediaEvent.Broadcast(
			InitializeMediaSession(SourceObject, ResolvedUrl)
				? EMediaEvent::MediaOpened
				: EMediaEvent::MediaOpenFailed
		);
	});
}


void FWmfMediaPlayer::ProcessResolveFailed(FString FailedUrl)
{
	// forward event to game thread
	AsyncTask(ENamedThreads::GameThread, [=]() {
		MediaEvent.Broadcast(EMediaEvent::MediaOpenFailed);
	});
}


/* FWmfMediaPlayer implementation
 *****************************************************************************/

void FWmfMediaPlayer::AddStreamToTopology(uint32 StreamIndex, IMFTopology* Topology, IMFPresentationDescriptor* PresentationDescriptor, IMFMediaSource* MediaSourceObject)
{
	// get stream descriptor
	TComPtr<IMFStreamDescriptor> StreamDescriptor;
	BOOL Selected = FALSE;

	if (FAILED(PresentationDescriptor->GetStreamDescriptorByIndex(StreamIndex, &Selected, &StreamDescriptor)))
	{
		UE_LOG(LogWmfMedia, Warning, TEXT("Skipping missing stream descriptor for stream index %i"), StreamIndex);

		return;
	}

	// fetch media type
	TComPtr<IMFMediaTypeHandler> Handler;

	if (FAILED(StreamDescriptor->GetMediaTypeHandler(&Handler)))
	{
		return;
	}

	GUID MajorType;
		
	if (FAILED(Handler->GetMajorType(&MajorType)))
	{
		return;
	}

	// skip unsupported types
	if ((MajorType != MFMediaType_Audio) &&
		(MajorType != MFMediaType_SAMI) &&
		(MajorType != MFMediaType_Video))
	{
		return;
	}

	// prepare input and output types
	TComPtr<IMFMediaType> OutputType;
	{
		if (FAILED(Handler->GetCurrentMediaType(&OutputType)))
		{
			return;
		}
	}

	TComPtr<IMFMediaType> InputType;
	{
		if (FAILED(::MFCreateMediaType(&InputType)))
		{
			return;
		}

		if (FAILED(InputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE)))
		{
			return;
		}
	}

	if (MajorType == MFMediaType_Audio)
	{
		if (FAILED(InputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
			FAILED(InputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)))/* ||
			FAILED(InputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_BLOCK, 16384u)))*/
		{
			return;
		}
	}
	else if (MajorType == MFMediaType_SAMI)
	{
		if (FAILED(InputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_SAMI)))
		{
			return;
		}
	}
	else if (MajorType == MFMediaType_Video)
	{
		if (FAILED(InputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) ||
			FAILED(InputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32)))
		{
			return;
		}
	}

	// create sample grabber callback and add to topology
	TComPtr<FWmfMediaSampler> Sampler = new FWmfMediaSampler();
	{
		TComPtr<IMFActivate> MediaSamplerActivator;

		if (FAILED(::MFCreateSampleGrabberSinkActivate(InputType, Sampler, &MediaSamplerActivator)))
		{
			return;
		}

		TComPtr<IMFTopologyNode> SourceNode;
		{
			if (FAILED(::MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &SourceNode)) ||
				FAILED(SourceNode->SetUnknown(MF_TOPONODE_SOURCE, MediaSourceObject)) ||
				FAILED(SourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, PresentationDescriptor)) ||
				FAILED(SourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, StreamDescriptor)) ||
				FAILED(Topology->AddNode(SourceNode)))
			{
				return;
			}
		}

		TComPtr<IMFTopologyNode> OutputNode;
		{
			if (FAILED(::MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &OutputNode)) ||
				FAILED(OutputNode->SetObject(MediaSamplerActivator)) ||
				FAILED(OutputNode->SetUINT32(MF_TOPONODE_STREAMID, 0)) ||
				FAILED(OutputNode->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE)) ||
				FAILED(Topology->AddNode(OutputNode)))
			{
				return;
			}
		}

		if (FAILED(SourceNode->ConnectOutput(0, OutputNode, 0)))
		{
			return;
		}
	}

	// create and add track
	if (MajorType == MFMediaType_Audio)
	{
		AudioTracks.Add(MakeShareable(new FWmfMediaAudioTrack(OutputType, PresentationDescriptor, Sampler, StreamDescriptor, StreamIndex)));
	}
	else if (MajorType == MFMediaType_SAMI)
	{
		CaptionTracks.Add(MakeShareable(new FWmfMediaCaptionTrack(OutputType, PresentationDescriptor, Sampler, StreamDescriptor, StreamIndex)));
	}
	else if (MajorType == MFMediaType_Video)
	{
		VideoTracks.Add(MakeShareable(new FWmfMediaVideoTrack(OutputType, PresentationDescriptor, Sampler, StreamDescriptor, StreamIndex)));
	}
}


bool FWmfMediaPlayer::InitializeMediaSession(IUnknown* SourceObject, const FString& SourceUrl)
{
	Close();

	if (SourceObject == nullptr)
	{
		return false;
	}

	UE_LOG(LogWmfMedia, Verbose, TEXT("Initializing media session for %s"), *SourceUrl);

	// create presentation descriptor
	TComPtr<IMFMediaSource> MediaSourceObject;

	if (FAILED(SourceObject->QueryInterface(IID_PPV_ARGS(&MediaSourceObject))))
	{
		UE_LOG(LogWmfMedia, Error, TEXT("Failed to query media source"));

		return false;
	}

	TComPtr<IMFPresentationDescriptor> PresentationDescriptor;
	
	if (FAILED(MediaSourceObject->CreatePresentationDescriptor(&PresentationDescriptor)))
	{
		UE_LOG(LogWmfMedia, Error, TEXT("Failed to create presentation descriptor"));

		return false;
	}
	
	// create playback topology
	DWORD StreamCount = 0;

	if (FAILED(PresentationDescriptor->GetStreamDescriptorCount(&StreamCount)))
	{
		UE_LOG(LogWmfMedia, Error, TEXT("Failed to get stream count"));

		return false;
	}

	TComPtr<IMFTopology> Topology;

	if (FAILED(::MFCreateTopology(&Topology)))
	{
		UE_LOG(LogWmfMedia, Error, TEXT("Failed to create playback topology"));

		return false;
	}

	for (uint32 StreamIndex = 0; StreamIndex < StreamCount; ++StreamIndex)
	{
		AddStreamToTopology(StreamIndex, Topology, PresentationDescriptor, MediaSourceObject);
	}

	UE_LOG(LogWmfMedia, Verbose, TEXT("Added a total of %i audio tracks, %i caption tracks, %i video tracks"), AudioTracks.Num(), CaptionTracks.Num(), VideoTracks.Num());
	MediaEvent.Broadcast(EMediaEvent::TracksChanged);

	UINT64 PresentationDuration = 0;
	PresentationDescriptor->GetUINT64(MF_PD_DURATION, &PresentationDuration);
	Duration = FTimespan(PresentationDuration);

	// create session
	MediaUrl = SourceUrl;
	MediaSource = MediaSourceObject;
	MediaSession = new FWmfMediaSession(Duration, Topology);
	{
		MediaSession->OnError().AddRaw(this, &FWmfMediaPlayer::HandleSessionError);
		MediaSession->OnSessionEvent().AddRaw(this, &FWmfMediaPlayer::HandleSessionEvent);
	}

	return (MediaSession->GetState() != EMediaStates::Error);
}


/* FWmfMediaPlayer callbacks
 *****************************************************************************/

void FWmfMediaPlayer::HandleSessionError(HRESULT Error)
{
	UE_LOG(LogWmfMedia, Error, TEXT("An error occured in the media session: 0x%X"), Error);
}


void FWmfMediaPlayer::HandleSessionEvent(MediaEventType EventType)
{
	EMediaEvent Event = EMediaEvent::Unknown;

	switch (EventType)
	{
	case MEEndOfPresentation:
		Event = EMediaEvent::PlaybackEndReached;
		break;

	case MESessionStarted:
		Event = EMediaEvent::PlaybackResumed;
		break;

	case MESessionStopped:
		Event = EMediaEvent::PlaybackSuspended;
		break;
	}

	if (Event != EMediaEvent::Unknown)
	{
		// forward event to game thread
		AsyncTask(ENamedThreads::GameThread, [=]() {
			MediaEvent.Broadcast(Event);
		});
	}
}


#include "HideWindowsPlatformTypes.h"
