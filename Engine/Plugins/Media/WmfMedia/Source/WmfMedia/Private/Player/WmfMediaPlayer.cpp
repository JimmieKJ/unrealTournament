// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "WmfMediaPCH.h"

#if WMFMEDIA_SUPPORTED_PLATFORM

#include "WmfMediaPlayer.h"
#include "WmfMediaResolver.h"
#include "WmfMediaSession.h"
#include "WmfMediaSampler.h"
#include "WmfMediaSettings.h"
#include "WmfMediaUtils.h"
#include "AllowWindowsPlatformTypes.h"


/* FWmfVideoPlayer structors
 *****************************************************************************/

FWmfMediaPlayer::FWmfMediaPlayer()
	: Duration(0)
{
	MediaSession = new FWmfMediaSession();
	Resolver = new FWmfMediaResolver;
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
	MediaSession = new FWmfMediaSession();

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

		return Resolver->ResolveByteStream(Archive.ToSharedRef(), Url, *this);
	}

	return Resolver->ResolveUrl(Url, *this);
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
		MediaEvent.Broadcast(EMediaEvent::MediaOpenFailed);
	});
}


/* FWmfMediaPlayer implementation
 *****************************************************************************/

void FWmfMediaPlayer::AddStreamToTopology(uint32 StreamIndex, IMFTopology* Topology, IMFPresentationDescriptor* PresentationDescriptor, IMFMediaSource* MediaSourceObject)
{
	Info += FString::Printf(TEXT("Stream %i\n"), StreamIndex);

	// get stream descriptor & media type handler
	TComPtr<IMFStreamDescriptor> StreamDescriptor;
	{
		BOOL Selected = FALSE;
		HRESULT Result = PresentationDescriptor->GetStreamDescriptorByIndex(StreamIndex, &Selected, &StreamDescriptor);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Skipping missing stream descriptor for stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
			Info += TEXT("    missing stream descriptor\n");
			
			return;
		}

		if (Selected == TRUE)
		{
			PresentationDescriptor->DeselectStream(StreamIndex);
		}
	}

	TComPtr<IMFMediaTypeHandler> Handler;
	{
		HRESULT Result = StreamDescriptor->GetMediaTypeHandler(&Handler);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Failed to get media type handler for stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
			Info += TEXT("    no handler available\n");
			
			return;
		}
	}

	// skip unsupported handler types
	GUID MajorType;
	{
		HRESULT Result = Handler->GetMajorType(&MajorType);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Failed to determine major type of stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
			Info += TEXT("    failed to determine MajorType\n");
			
			return;
		}

		UE_LOG(LogWmfMedia, Verbose, TEXT("Major type of stream %i is %s"), StreamIndex, *WmfMedia::MajorTypeToString(MajorType));
		Info += FString::Printf(TEXT("    Type: %s\n"), *WmfMedia::MajorTypeToString(MajorType));

		if ((MajorType != MFMediaType_Audio) &&
			(MajorType != MFMediaType_SAMI) &&
			(MajorType != MFMediaType_Video))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Unsupported major type %s for stream %i"), *WmfMedia::MajorTypeToString(MajorType), StreamIndex);
			Info += TEXT("    MajorType is not supported\n");
			
			return;
		}
	}

	// get media type & make it current
	TComPtr<IMFMediaType> MediaType;
	{
		DWORD NumSupportedTypes = 0;
		HRESULT Result = Handler->GetMediaTypeCount(&NumSupportedTypes);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Failed to get number of supported media types in stream %i of type %s"), StreamIndex, *WmfMedia::MajorTypeToString(MajorType));
			Info += TEXT("    failed to get supported media types\n");

			return;
		}

		if (NumSupportedTypes > 0)
		{
			for (DWORD TypeIndex = 0; TypeIndex < NumSupportedTypes; ++TypeIndex)
			{
				if (SUCCEEDED(Handler->GetMediaTypeByIndex(TypeIndex, &MediaType)) &&
					SUCCEEDED(Handler->SetCurrentMediaType(MediaType)))
				{
					break;
				}
			}
		}

		if (MediaType == NULL)
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("No supported media type in stream %i of type %s"), StreamIndex, *WmfMedia::MajorTypeToString(MajorType));
			Info += TEXT("    unsupported media type\n");

			return;
		}
	}
	
	// get sub-type
	GUID SubType;
	{
		if (MajorType == MFMediaType_SAMI)
		{
			FMemory::Memzero(SubType);
		}
		else
		{
			HRESULT Result = MediaType->GetGUID(MF_MT_SUBTYPE, &SubType);

			if (FAILED(Result))
			{
				UE_LOG(LogWmfMedia, Warning, TEXT("Failed to get sub-type of stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
				Info += TEXT("    failed to get sub-type\n");

				return;
			}

			UE_LOG(LogWmfMedia, Verbose, TEXT("Sub-type of stream %i is %s"), StreamIndex, *WmfMedia::SubTypeToString(SubType));
		
			Info += FString::Printf(TEXT("    Codec: %s\n"), *WmfMedia::SubTypeToString(SubType));
			Info += FString::Printf(TEXT("    Protected: %s\n"), (::MFGetAttributeUINT32(StreamDescriptor, MF_SD_PROTECTED, FALSE) != 0) ? *GYes.ToString() : *GNo.ToString());
		}
	}

	// configure desired output type
	TComPtr<IMFMediaType> OutputType;
	{
		HRESULT Result = ::MFCreateMediaType(&OutputType);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Failed to create output type for stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
			Info += TEXT("    failed to create output type\n");

			return;
		}

		Result = OutputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Failed to initialize output type for stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
			Info += TEXT("    failed to initialize output type\n");

			return;
		}

		if (MajorType == MFMediaType_Audio)
		{
			// get source stream information
			Info += FString::Printf(TEXT("    Channels: %i\n"), ::MFGetAttributeUINT32(MediaType, MF_MT_AUDIO_NUM_CHANNELS, 0));
			Info += FString::Printf(TEXT("    Sample Rate: %i Hz\n"), ::MFGetAttributeUINT32(MediaType, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0));
			Info += FString::Printf(TEXT("    Bits Per Sample: %i\n"), ::MFGetAttributeUINT32(MediaType, MF_MT_AUDIO_BITS_PER_SAMPLE, 0));

			// filter unsupported audio formats
			if (FMemory::Memcmp(&SubType.Data2, &MFMPEG4Format_Base.Data2, 12) == 0)
			{
				if (GetDefault<UWmfMediaSettings>()->AllowNonStandardCodecs)
				{
					UE_LOG(LogWmfMedia, Verbose, TEXT("Non-standard MP4 audio type '%s' (%s) for stream %i"), *WmfMedia::FourccToString(SubType.Data1), *WmfMedia::GuidToString(SubType), StreamIndex);
					Info += TEXT("    non-standard sub-type\n");
				}
				else
				{
					const bool DocumentedFormat =
						(SubType.Data1 == WAVE_FORMAT_ADPCM) ||
						(SubType.Data1 == WAVE_FORMAT_ALAW) ||
						(SubType.Data1 == WAVE_FORMAT_MULAW) ||
						(SubType.Data1 == WAVE_FORMAT_IMA_ADPCM) ||
						(SubType.Data1 == MFAudioFormat_AAC.Data1) ||
						(SubType.Data1 == MFAudioFormat_MP3.Data1) ||
						(SubType.Data1 == MFAudioFormat_PCM.Data1);

					const bool UndocumentedFormat =
						(SubType.Data1 == WAVE_FORMAT_WMAUDIO2) ||
						(SubType.Data1 == WAVE_FORMAT_WMAUDIO3) ||
						(SubType.Data1 == WAVE_FORMAT_WMAUDIO_LOSSLESS);

					if (!DocumentedFormat && !UndocumentedFormat)
					{
						UE_LOG(LogWmfMedia, Warning, TEXT("Unsupported MP4 audio type '%s' (%s) for stream %i"), *WmfMedia::FourccToString(SubType.Data1), *WmfMedia::GuidToString(SubType), StreamIndex);
						Info += TEXT("    unsupported sub-type\n");

						return;
					}
				}
			}
			else if (FMemory::Memcmp(&SubType.Data2, &MFAudioFormat_Base.Data2, 12) != 0)
			{
				if (GetDefault<UWmfMediaSettings>()->AllowNonStandardCodecs)
				{
					UE_LOG(LogWmfMedia, Verbose, TEXT("Non-standard audio type '%s' (%s) for stream %i"), *WmfMedia::FourccToString(SubType.Data1), *WmfMedia::GuidToString(SubType), StreamIndex);
					Info += TEXT("    non-standard sub-type\n");
				}
				else
				{
					UE_LOG(LogWmfMedia, Warning, TEXT("Unsupported audio type '%s' (%s) for stream %i"), *WmfMedia::FourccToString(SubType.Data1), *WmfMedia::GuidToString(SubType), StreamIndex);
					Info += TEXT("    unsupported sub-type\n");

					return;
				}
			}

			// configure audio output (re-sampling fails for many media types, so we don't attempt it)
			if (FAILED(OutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
				FAILED(OutputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) ||
				FAILED(OutputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16u)))
			{
				UE_LOG(LogWmfMedia, Warning, TEXT("Failed to initialize audio output type for stream %i"), StreamIndex);
				Info += TEXT("    failed to initialize output type\n");

				return;
			}

			if (::MFGetAttributeUINT32(MediaType, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0) != 44100)
			{
				UE_LOG(LogWmfMedia, Warning, TEXT("Possible loss of audio quality in stream %i due to sample rate != 44100 Hz"), StreamIndex);
			}
		}
		else if (MajorType == MFMediaType_SAMI)
		{
			// configure caption output
			Result = OutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_SAMI);

			if (FAILED(Result))
			{
				UE_LOG(LogWmfMedia, Warning, TEXT("Failed to initialize caption output type for stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
				Info += TEXT("    failed to set output type\n");

				return;
			}
		}
		else if (MajorType == MFMediaType_Video)
		{
			// get source stream information
			FIntPoint Dimensions;

			if (SUCCEEDED(::MFGetAttributeSize(MediaType, MF_MT_FRAME_SIZE, (UINT32*)&Dimensions.X, (UINT32*)&Dimensions.Y)))
			{
				Info += FString::Printf(TEXT("    Dimensions: %i x %i\n"), Dimensions.X, Dimensions.Y);
			}
			else
			{
				Info += FString::Printf(TEXT("    Dimensions: n/a\n"));
			}

			UINT32 Numerator = 0;
			UINT32 Denominator = 1;

			if (SUCCEEDED(::MFGetAttributeRatio(MediaType, MF_MT_FRAME_RATE, &Numerator, &Denominator)))
			{
				const float FrameRate = static_cast<float>(Numerator) / Denominator;
				Info += FString::Printf(TEXT("    Frame Rate: %g fps\n"), FrameRate);
			}
			else
			{
				Info += FString::Printf(TEXT("    Frame Rate: n/a\n"));
			}

			// filter unsupported video types
			if (FMemory::Memcmp(&SubType.Data2, &MFVideoFormat_Base.Data2, 12) != 0)
			{
				if (GetDefault<UWmfMediaSettings>()->AllowNonStandardCodecs)
				{
					UE_LOG(LogWmfMedia, Verbose, TEXT("Non-standard video type '%s' (%s) for stream %i"), *WmfMedia::FourccToString(SubType.Data1), *WmfMedia::GuidToString(SubType), StreamIndex);
					Info += TEXT("    non-standard sub-type\n");
				}
				else
				{
					UE_LOG(LogWmfMedia, Warning, TEXT("Unsupported video type '%s' (%s) for stream %i"), *WmfMedia::FourccToString(SubType.Data1), *WmfMedia::GuidToString(SubType), StreamIndex);
					Info += TEXT("    unsupported sub-type\n");
				}

				return;
			}

			// configure video output
			Result = OutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);

			if (FAILED(Result))
			{
				UE_LOG(LogWmfMedia, Warning, TEXT("Failed to set video output type for stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
				Info += TEXT("    failed to set output type\n");

				return;
			}

			GUID OutputSubType = ((SubType == MFVideoFormat_H264) || (SubType == MFVideoFormat_H264_ES))
				? MFVideoFormat_YUY2
				: MFVideoFormat_RGB32;

			Result = OutputType->SetGUID(MF_MT_SUBTYPE, OutputSubType);

			if (FAILED(Result))
			{
				UE_LOG(LogWmfMedia, Warning, TEXT("Failed to set video output sub-type for stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
				Info += TEXT("    failed to set output sub-type\n");

				return;
			}
		}
	}

	TComPtr<FWmfMediaSampler> Sampler = new FWmfMediaSampler();

	// set up output node
	TComPtr<IMFActivate> MediaSamplerActivator;
	{
		HRESULT Result = ::MFCreateSampleGrabberSinkActivate(OutputType, Sampler, &MediaSamplerActivator);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Failed to create sampler grabber sink for stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
			Info += TEXT("    failed to create sample grabber\n");

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
			UE_LOG(LogWmfMedia, Warning, TEXT("Failed to configure output node for stream %i"), StreamIndex);
			Info += TEXT("    failed to configure output node\n");

			return;
		}
	}

	// set up source node
	TComPtr<IMFTopologyNode> SourceNode;
	{
		if (FAILED(::MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &SourceNode)) ||
			FAILED(SourceNode->SetUnknown(MF_TOPONODE_SOURCE, MediaSourceObject)) ||
			FAILED(SourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, PresentationDescriptor)) ||
			FAILED(SourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, StreamDescriptor)) ||
			FAILED(Topology->AddNode(SourceNode)))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Failed to configure source node for stream %i"), StreamIndex);
			Info += TEXT("    failed to configure source node\n");

			return;
		}
	}

	// connect nodes
	HRESULT Result = SourceNode->ConnectOutput(0, OutputNode, 0);

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Warning, TEXT("Failed to connect topology nodes for stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
		Info += TEXT("    failed to connect topology nodes\n");

		return;
	}

	Tracks.AddTrack(MajorType, MediaType, OutputType, Sampler, StreamDescriptor, StreamIndex);
}


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
	
	// create playback topology
	TComPtr<IMFTopology> Topology;
	{
		HRESULT Result = ::MFCreateTopology(&Topology);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Error, TEXT("Failed to create playback topology: %s"), *WmfMedia::ResultToString(Result));
			return false;
		}
	}

	// initialize tracks
	DWORD StreamCount = 0;
	{
		HRESULT Result = PresentationDescriptor->GetStreamDescriptorCount(&StreamCount);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Error, TEXT("Failed to get stream count (%s)"), *WmfMedia::ResultToString(Result));
			return false;
		}

		UE_LOG(LogWmfMedia, Verbose, TEXT("Found %i streams"), StreamCount);
	}

	Tracks.Initialize(*PresentationDescriptor);

	for (uint32 StreamIndex = 0; StreamIndex < StreamCount; ++StreamIndex)
	{
		AddStreamToTopology(StreamIndex, Topology, PresentationDescriptor, MediaSourceObject);
		Info += TEXT("\n");
	}

	MediaEvent.Broadcast(EMediaEvent::TracksChanged);

	// get media duration
	UINT64 PresentationDuration = 0;
	PresentationDescriptor->GetUINT64(MF_PD_DURATION, &PresentationDuration);
	Duration = FTimespan(PresentationDuration);

	// create session
	MediaUrl = SourceUrl;
	MediaSource = MediaSourceObject;
	MediaSession = new FWmfMediaSession(Duration, Topology);

	if (MediaSession->GetState() == EMediaState::Error)
	{
		return false;
	}

	MediaSession->OnSessionEvent().AddRaw(this, &FWmfMediaPlayer::HandleSessionEvent);

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
	case MEEndOfPresentation:
		Event = EMediaEvent::PlaybackEndReached;
		break;

	case MESessionClosed:
	case MEError:
		Tracks.SetPaused(true);
		Tracks.Flush();
		break;

	case MESessionStarted:
		if (!FMath::IsNearlyZero(MediaSession->GetRate()))
		{
			Event = EMediaEvent::PlaybackResumed;
			Tracks.SetPaused(false);
		}
		break;

	case MESessionEnded:
		Event = EMediaEvent::PlaybackSuspended;
		Tracks.SetPaused(true);
		Tracks.Flush();
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
