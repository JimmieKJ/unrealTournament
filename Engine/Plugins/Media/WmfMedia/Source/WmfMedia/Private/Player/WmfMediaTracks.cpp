// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "WmfMediaTracks.h"
#include "Misc/ScopeLock.h"
#include "UObject/Class.h"
#include "WmfMediaSampler.h"
#include "WmfMediaUtils.h"

#if WMFMEDIA_SUPPORTED_PLATFORM

#include "AllowWindowsPlatformTypes.h"

#define LOCTEXT_NAMESPACE "FWmfMediaTracks"


/* FWmfMediaTracks structors
 *****************************************************************************/

FWmfMediaTracks::FWmfMediaTracks()
	: SelectedAudioTrack(INDEX_NONE)
	, SelectedCaptionTrack(INDEX_NONE)
	, SelectedVideoTrack(INDEX_NONE)
	, AudioSink(nullptr)
	, OverlaySink(nullptr)
	, VideoSink(nullptr)
{ }


FWmfMediaTracks::~FWmfMediaTracks()
{
	Reset();
}


/* FWmfMediaTracks interface
 *****************************************************************************/

void FWmfMediaTracks::AppendStats(FString &OutStats) const
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
		for (const FAudioTrack& AudioTrack : AudioTracks)
		{
			OutStats += FString::Printf(TEXT("    %s\n"), *AudioTrack.DisplayName.ToString());
			OutStats += FString::Printf(TEXT("        Channels: %i\n"), AudioTrack.NumChannels);
			OutStats += FString::Printf(TEXT("        Sample Rate: %i\n\n"), AudioTrack.SampleRate);
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
		for (const FVideoTrack& VideoTrack : VideoTracks)
		{
			OutStats += FString::Printf(TEXT("    %s\n"), *VideoTrack.DisplayName.ToString());
			OutStats += FString::Printf(TEXT("        BitRate: %i\n"), VideoTrack.BitRate);
			OutStats += FString::Printf(TEXT("        Frame Rate: %i\n\n"), VideoTrack.FrameRate);
		}
	}
}


TComPtr<IMFTopology> FWmfMediaTracks::CreateTopology() const
{
	TComPtr<IMFTopology> Topology;
	{
		HRESULT Result = ::MFCreateTopology(&Topology);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Error, TEXT("Failed to create playback topology: %s"), *WmfMedia::ResultToString(Result));
			return NULL;
		}
	}

	if (AudioTracks.IsValidIndex(SelectedAudioTrack))
	{
		AddStreamToTopology(AudioTracks[SelectedAudioTrack], *Topology);
	}

	if (CaptionTracks.IsValidIndex(SelectedCaptionTrack))
	{
		AddStreamToTopology(CaptionTracks[SelectedCaptionTrack], *Topology);
	}

	if (VideoTracks.IsValidIndex(SelectedVideoTrack))
	{
		AddStreamToTopology(VideoTracks[SelectedVideoTrack], *Topology);
	}

	return Topology;
}


void FWmfMediaTracks::FlushSinks()
{
	FScopeLock Lock(&CriticalSection);

	if (AudioSink != nullptr)
	{
		AudioSink->FlushAudioSink();
	}
}


bool FWmfMediaTracks::Initialize(IMFMediaSource& InMediaSource, IMFPresentationDescriptor& InPresentationDescriptor, FString& OutInfo)
{
	Reset();

	FScopeLock Lock(&CriticalSection);

	MediaSource = &InMediaSource;
	PresentationDescriptor = &InPresentationDescriptor;

	// get number of streams
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

	// initialize streams
	for (uint32 StreamIndex = 0; StreamIndex < StreamCount; ++StreamIndex)
	{
		AddStreamToTracks(StreamIndex, InMediaSource, InPresentationDescriptor, OutInfo);	
		OutInfo += TEXT("\n");
	}

	return true;
}


void FWmfMediaTracks::Reset()
{
	FScopeLock Lock(&CriticalSection);

	SelectedAudioTrack = INDEX_NONE;
	SelectedCaptionTrack = INDEX_NONE;
	SelectedVideoTrack = INDEX_NONE;

	if (AudioSink != nullptr)
	{
		AudioSink->ShutdownAudioSink();
	}

	if (OverlaySink != nullptr)
	{
		OverlaySink->ShutdownOverlaySink();
	}

	if (VideoSink != nullptr)
	{
		VideoSink->ShutdownTextureSink();
	}

	AudioTracks.Empty();
	CaptionTracks.Empty();
	VideoTracks.Empty();

	MediaSource = nullptr;
	PresentationDescriptor = nullptr;
}


void FWmfMediaTracks::SetPaused(bool Paused)
{
	FScopeLock Lock(&CriticalSection);

	if (AudioSink != nullptr)
	{
		if (Paused)
		{
			AudioSink->PauseAudioSink();
		}
		else
		{
			AudioSink->ResumeAudioSink();
		}
	}
}


/* IMediaOutput interface
 *****************************************************************************/

void FWmfMediaTracks::SetAudioSink(IMediaAudioSink* Sink)
{
	if (Sink != AudioSink)
	{
		FScopeLock Lock(&CriticalSection);

		if (AudioSink != nullptr)
		{
			AudioSink->ShutdownAudioSink();
		}

		AudioSink = Sink;	
		InitializeAudioSink();
	}
}


void FWmfMediaTracks::SetOverlaySink(IMediaOverlaySink* Sink)
{
	if (Sink != OverlaySink)
	{
		FScopeLock Lock(&CriticalSection);

		if (OverlaySink != nullptr)
		{
			OverlaySink->ShutdownOverlaySink();
		}

		OverlaySink = Sink;
		InitializeOverlaySink();
	}
}


void FWmfMediaTracks::SetVideoSink(IMediaTextureSink* Sink)
{
	if (Sink != VideoSink)
	{
		FScopeLock Lock(&CriticalSection);

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

uint32 FWmfMediaTracks::GetAudioTrackChannels(int32 TrackIndex) const
{
	return AudioTracks.IsValidIndex(TrackIndex) ? AudioTracks[TrackIndex].NumChannels : 0;
}


uint32 FWmfMediaTracks::GetAudioTrackSampleRate(int32 TrackIndex) const
{
	return AudioTracks.IsValidIndex(TrackIndex) ? AudioTracks[TrackIndex].SampleRate : 0;
}


int32 FWmfMediaTracks::GetNumTracks(EMediaTrackType TrackType) const
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


int32 FWmfMediaTracks::GetSelectedTrack(EMediaTrackType TrackType) const
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


FText FWmfMediaTracks::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
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


FString FWmfMediaTracks::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
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
		break;

	default:
		break;
	}

	return FString();
}


FString FWmfMediaTracks::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
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


uint32 FWmfMediaTracks::GetVideoTrackBitRate(int32 TrackIndex) const
{
	return VideoTracks.IsValidIndex(TrackIndex) ? VideoTracks[TrackIndex].BitRate : 0;
}


FIntPoint FWmfMediaTracks::GetVideoTrackDimensions(int32 TrackIndex) const
{
	return VideoTracks.IsValidIndex(TrackIndex) ? VideoTracks[TrackIndex].OutputDim : FIntPoint::ZeroValue;
}


float FWmfMediaTracks::GetVideoTrackFrameRate(int32 TrackIndex) const
{
	return VideoTracks.IsValidIndex(TrackIndex) ? VideoTracks[TrackIndex].FrameRate : 0.0f;
}


bool FWmfMediaTracks::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	if (PresentationDescriptor == nullptr)
	{
		return false; // not initialized
	}

	FScopeLock Lock(&CriticalSection);

	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		if (TrackIndex != SelectedAudioTrack)
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Selecting audio track %i instead of %i (%i tracks)."), TrackIndex, SelectedAudioTrack, AudioTracks.Num());

			if (SelectedAudioTrack != INDEX_NONE)
			{
				PresentationDescriptor->DeselectStream(AudioTracks[SelectedAudioTrack].StreamIndex);
				SelectedAudioTrack = INDEX_NONE;
			}

			if ((TrackIndex == INDEX_NONE) || (AudioTracks.IsValidIndex(TrackIndex) && SUCCEEDED(PresentationDescriptor->SelectStream(AudioTracks[TrackIndex].StreamIndex))))
			{
				SelectedAudioTrack = TrackIndex;
			}

			if (SelectedAudioTrack == TrackIndex)
			{
				InitializeAudioSink();
			}
		}
		break;

	case EMediaTrackType::Caption:
		if (TrackIndex != SelectedCaptionTrack)
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Selecting caption track %i instead of %i (%i tracks)."), TrackIndex, SelectedCaptionTrack, CaptionTracks.Num());

			if (SelectedCaptionTrack != INDEX_NONE)
			{
				PresentationDescriptor->DeselectStream(CaptionTracks[SelectedCaptionTrack].StreamIndex);
				SelectedCaptionTrack = INDEX_NONE;
			}

			if ((TrackIndex == INDEX_NONE) || (CaptionTracks.IsValidIndex(TrackIndex) && SUCCEEDED(PresentationDescriptor->SelectStream(CaptionTracks[TrackIndex].StreamIndex))))
			{
				SelectedCaptionTrack = TrackIndex;
			}

			if (SelectedCaptionTrack == TrackIndex)
			{
				InitializeOverlaySink();
			}
		}
		break;

	case EMediaTrackType::Video:
		if (TrackIndex != SelectedVideoTrack)
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Selecting video track %i instead of %i (%i tracks)."), TrackIndex, SelectedVideoTrack, VideoTracks.Num());

			if (SelectedVideoTrack != INDEX_NONE)
			{
				PresentationDescriptor->DeselectStream(VideoTracks[SelectedVideoTrack].StreamIndex);
				SelectedVideoTrack = INDEX_NONE;
			}

			if ((TrackIndex == INDEX_NONE) || (VideoTracks.IsValidIndex(TrackIndex) && SUCCEEDED(PresentationDescriptor->SelectStream(VideoTracks[TrackIndex].StreamIndex))))
			{
				SelectedVideoTrack = TrackIndex;
			}

			if (SelectedVideoTrack == TrackIndex)
			{
				InitializeVideoSink();
			}
		}
		break;

	default:
		return false;
	}

	SelectionChangedEvent.Broadcast();

	return false;
}


/* FWmfMediaTracks implementation
 *****************************************************************************/

bool FWmfMediaTracks::AddStreamToTopology(const FStreamInfo& Stream, IMFTopology& Topology) const
{
	GUID StreamType;
	{
		HRESULT Result = Stream.OutputType->GetGUID(MF_MT_MAJOR_TYPE, &StreamType);
		check(SUCCEEDED(Result));
	}

	TComPtr<IMFActivate> OutputActivator;
	{
		if ((StreamType == MFMediaType_Audio) && GetDefault<UWmfMediaSettings>()->NativeAudioOut)
		{
			HRESULT Result = MFCreateAudioRendererActivate(&OutputActivator);

			if (FAILED(Result))
			{
				UE_LOG(LogWmfMedia, Warning, TEXT("Failed to create audio renderer for stream %i (%s)"), Stream.StreamIndex, *WmfMedia::ResultToString(Result));
				return false;
			}

#if WITH_ENGINE
			// allow HMD to override audio output device
			if (IHeadMountedDisplayModule::IsAvailable())
			{
				FString AudioOutputDevice = IHeadMountedDisplayModule::Get().GetAudioOutputDevice();

				if (!AudioOutputDevice.IsEmpty())
				{
					Result = OutputActivator->SetString(MF_AUDIO_RENDERER_ATTRIBUTE_ENDPOINT_ID, *AudioOutputDevice);

					if (FAILED(Result))
					{
						UE_LOG(LogWmfMedia, Warning, TEXT("Failed to override HMD audio output device for stream %i (%s)"), Stream.StreamIndex, *WmfMedia::ResultToString(Result));
						return false;
					}
				}
			}
#endif //WITH_ENGINE
		}
		else
		{
			check(Stream.Sampler != NULL);
			HRESULT Result = ::MFCreateSampleGrabberSinkActivate(Stream.OutputType, Stream.Sampler, &OutputActivator);

			if (FAILED(Result))
			{
				UE_LOG(LogWmfMedia, Warning, TEXT("Failed to create sampler grabber sink for stream %i (%s)"), Stream.StreamIndex, *WmfMedia::ResultToString(Result));
				return false;
			}
		}
	}

	// set up output node
	TComPtr<IMFTopologyNode> OutputNode;
	{
		if (FAILED(::MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &OutputNode)) ||
			FAILED(OutputNode->SetObject(OutputActivator)) ||
			FAILED(OutputNode->SetUINT32(MF_TOPONODE_STREAMID, 0)) ||
			FAILED(OutputNode->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE)) ||
			FAILED(Topology.AddNode(OutputNode)))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Failed to configure output node for stream %i"), Stream.StreamIndex);
			return false;
		}
	}

	// set up source node
	TComPtr<IMFTopologyNode> SourceNode;
	{
		if (FAILED(::MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &SourceNode)) ||
			FAILED(SourceNode->SetUnknown(MF_TOPONODE_SOURCE, MediaSource)) ||
			FAILED(SourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, PresentationDescriptor)) ||
			FAILED(SourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, Stream.Descriptor)) ||
			FAILED(Topology.AddNode(SourceNode)))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Failed to configure source node for stream %i"), Stream.StreamIndex);
			return false;
		}
	}

	// connect nodes
	HRESULT Result = SourceNode->ConnectOutput(0, OutputNode, 0);

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Warning, TEXT("Failed to connect topology nodes for stream %i (%s)"), Stream.StreamIndex, *WmfMedia::ResultToString(Result));
		return false;
	}

	return true;
}


void FWmfMediaTracks::AddStreamToTracks(uint32 StreamIndex, IMFMediaSource& InMediaSource, IMFPresentationDescriptor& InPresentationDescriptor, FString& OutInfo)
{
	OutInfo += FString::Printf(TEXT("Stream %i\n"), StreamIndex);

	// get stream descriptor
	TComPtr<IMFStreamDescriptor> StreamDescriptor;
	{
		BOOL Selected = FALSE;
		HRESULT Result = PresentationDescriptor->GetStreamDescriptorByIndex(StreamIndex, &Selected, &StreamDescriptor);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Skipping missing stream descriptor for stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
			OutInfo += TEXT("    missing stream descriptor\n");

			return;
		}

		if (Selected == TRUE)
		{
			PresentationDescriptor->DeselectStream(StreamIndex);
		}
	}

	// get media type handler
	TComPtr<IMFMediaTypeHandler> Handler;
	{
		HRESULT Result = StreamDescriptor->GetMediaTypeHandler(&Handler);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Failed to get media type handler for stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
			OutInfo += TEXT("    no handler available\n");

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
			OutInfo += TEXT("    failed to determine MajorType\n");

			return;
		}

		UE_LOG(LogWmfMedia, Verbose, TEXT("Major type of stream %i is %s"), StreamIndex, *WmfMedia::MajorTypeToString(MajorType));
		OutInfo += FString::Printf(TEXT("    Type: %s\n"), *WmfMedia::MajorTypeToString(MajorType));

		if ((MajorType != MFMediaType_Audio) &&
			(MajorType != MFMediaType_SAMI) &&
			(MajorType != MFMediaType_Video))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Unsupported major type %s for stream %i"), *WmfMedia::MajorTypeToString(MajorType), StreamIndex);
			OutInfo += TEXT("    Unsupported stream type\n");

			return;
		}
	}

	// get media type and make it current
	TComPtr<IMFMediaType> MediaType;
	{
		DWORD NumSupportedTypes = 0;
		HRESULT Result = Handler->GetMediaTypeCount(&NumSupportedTypes);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Failed to get number of supported media types in stream %i of type %s"), StreamIndex, *WmfMedia::MajorTypeToString(MajorType));
			OutInfo += TEXT("    failed to get supported media types\n");

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
			OutInfo += TEXT("    unsupported media type\n");

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
				OutInfo += TEXT("    failed to get sub-type\n");

				return;
			}

			UE_LOG(LogWmfMedia, Verbose, TEXT("Sub-type of stream %i is %s"), StreamIndex, *WmfMedia::SubTypeToString(SubType));

			OutInfo += FString::Printf(TEXT("    Codec: %s\n"), *WmfMedia::SubTypeToString(SubType));
		}
	}

	// @todo gmp: handle protected content
	const bool Protected = ::MFGetAttributeUINT32(StreamDescriptor, MF_SD_PROTECTED, FALSE) != 0;
	{
		if (Protected)
		{
			OutInfo += FString::Printf(TEXT("    Protected content\n"));
		}
	}

	// configure output type
	TComPtr<IMFMediaType> OutputType;
	{
		HRESULT Result = ::MFCreateMediaType(&OutputType);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Failed to create output type for stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
			OutInfo += TEXT("    failed to create output type\n");

			return;
		}

		Result = OutputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Warning, TEXT("Failed to initialize output type for stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
			OutInfo += TEXT("    failed to initialize output type\n");

			return;
		}

		if (MajorType == MFMediaType_Audio)
		{
			// filter unsupported audio formats
			if (FMemory::Memcmp(&SubType.Data2, &MFMPEG4Format_Base.Data2, 12) == 0)
			{
				if (GetDefault<UWmfMediaSettings>()->AllowNonStandardCodecs)
				{
					UE_LOG(LogWmfMedia, Verbose, TEXT("Non-standard MP4 audio type '%s' (%s) for stream %i"), *WmfMedia::FourccToString(SubType.Data1), *WmfMedia::GuidToString(SubType), StreamIndex);
					OutInfo += TEXT("    non-standard sub-type\n");
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
						OutInfo += TEXT("    unsupported sub-type\n");

						return;
					}
				}
			}
			else if (FMemory::Memcmp(&SubType.Data2, &MFAudioFormat_Base.Data2, 12) != 0)
			{
				if (GetDefault<UWmfMediaSettings>()->AllowNonStandardCodecs)
				{
					UE_LOG(LogWmfMedia, Verbose, TEXT("Non-standard audio type '%s' (%s) for stream %i"), *WmfMedia::FourccToString(SubType.Data1), *WmfMedia::GuidToString(SubType), StreamIndex);
					OutInfo += TEXT("    non-standard sub-type\n");
				}
				else
				{
					UE_LOG(LogWmfMedia, Warning, TEXT("Unsupported audio type '%s' (%s) for stream %i"), *WmfMedia::FourccToString(SubType.Data1), *WmfMedia::GuidToString(SubType), StreamIndex);
					OutInfo += TEXT("    unsupported sub-type\n");

					return;
				}
			}

			// configure audio output (re-sampling fails for many media types, so we don't attempt it)
			if (FAILED(OutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
				FAILED(OutputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) ||
				FAILED(OutputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16u)))
			{
				UE_LOG(LogWmfMedia, Warning, TEXT("Failed to initialize audio output type for stream %i"), StreamIndex);
				OutInfo += TEXT("    failed to initialize output type\n");

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
				OutInfo += TEXT("    failed to set output type\n");

				return;
			}
		}
		else if (MajorType == MFMediaType_Video)
		{
			// filter unsupported video types
			if (FMemory::Memcmp(&SubType.Data2, &MFVideoFormat_Base.Data2, 12) != 0)
			{
				if (GetDefault<UWmfMediaSettings>()->AllowNonStandardCodecs)
				{
					UE_LOG(LogWmfMedia, Verbose, TEXT("Non-standard video type '%s' (%s) for stream %i"), *WmfMedia::FourccToString(SubType.Data1), *WmfMedia::GuidToString(SubType), StreamIndex);
					OutInfo += TEXT("    non-standard sub-type\n");
				}
				else
				{
					UE_LOG(LogWmfMedia, Warning, TEXT("Unsupported video type '%s' (%s) for stream %i"), *WmfMedia::FourccToString(SubType.Data1), *WmfMedia::GuidToString(SubType), StreamIndex);
					OutInfo += TEXT("    unsupported sub-type\n");
				}

				return;
			}

			// configure video output
			Result = OutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);

			if (FAILED(Result))
			{
				UE_LOG(LogWmfMedia, Warning, TEXT("Failed to set video output type for stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
				OutInfo += TEXT("    failed to set output type\n");

				return;
			}

			const bool Uncompressed =
				(SubType == MFVideoFormat_RGB555) ||
				(SubType == MFVideoFormat_RGB565) ||
				(SubType == MFVideoFormat_RGB565) ||
				(SubType == MFVideoFormat_RGB24) ||
				(SubType == MFVideoFormat_RGB32) ||
				(SubType == MFVideoFormat_ARGB32);

			Result = OutputType->SetGUID(MF_MT_SUBTYPE, Uncompressed ? MFVideoFormat_RGB32 : MFVideoFormat_YUY2);

			if (FAILED(Result))
			{
				UE_LOG(LogWmfMedia, Warning, TEXT("Failed to set video output sub-type for stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
				OutInfo += TEXT("    failed to set output sub-type\n");

				return;
			}
		}
		else
		{
			check(false); // should never get here as unsupported major types are filtered above
		}
	}

	// create output sampler
	TComPtr<FWmfMediaSampler> Sampler;

	if ((MajorType != MFMediaType_Audio) || !GetDefault<UWmfMediaSettings>()->NativeAudioOut)
	{
		Sampler = new FWmfMediaSampler();
	}

	// create & add track
	FStreamInfo* StreamInfo = nullptr;

	if (MajorType == MFMediaType_Audio)
	{
		const int32 AudioTrackIndex = AudioTracks.AddDefaulted();
		FAudioTrack& AudioTrack = AudioTracks[AudioTrackIndex];
		{
			AudioTrack.BitsPerSample = ::MFGetAttributeUINT32(MediaType, MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
			AudioTrack.NumChannels = ::MFGetAttributeUINT32(MediaType, MF_MT_AUDIO_NUM_CHANNELS, 0);
			AudioTrack.SampleRate = ::MFGetAttributeUINT32(MediaType, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
		}

		if (Sampler != NULL)
		{
			Sampler->OnClock().AddRaw(this, &FWmfMediaTracks::HandleMediaSamplerClock, EMediaTrackType::Audio);
			Sampler->OnSample().AddRaw(this, &FWmfMediaTracks::HandleMediaSamplerSample, EMediaTrackType::Audio);
		}

		OutInfo += FString::Printf(TEXT("    Channels: %i\n"), AudioTrack.NumChannels);
		OutInfo += FString::Printf(TEXT("    Sample Rate: %i Hz\n"), AudioTrack.SampleRate);
		OutInfo += FString::Printf(TEXT("    Bits Per Sample: %i\n"), AudioTrack.BitsPerSample);

		StreamInfo = &AudioTrack;
	}
	else if (MajorType == MFMediaType_SAMI)
	{
		const int32 CaptionTrackIndex = CaptionTracks.AddDefaulted();
		FCaptionTrack& CaptionTrack = CaptionTracks[CaptionTrackIndex];

		if (Sampler != NULL)
		{
			Sampler->OnSample().AddRaw(this, &FWmfMediaTracks::HandleMediaSamplerSample, EMediaTrackType::Caption);
		}

		StreamInfo = &CaptionTrack;
	}
	else if ((MajorType == MFMediaType_Video) || (MajorType == MFMediaType_Image))
	{
		GUID OutputSubType;
		{
			HRESULT Result = OutputType->GetGUID(MF_MT_SUBTYPE, &OutputSubType);

			if (FAILED(Result))
			{
				UE_LOG(LogWmfMedia, Error, TEXT("Failed to get video output sub-type for stream %i (%s)"), StreamIndex, *WmfMedia::ResultToString(Result));
				return;
			}
		}

		const int32 VideoTrackIndex = VideoTracks.AddDefaulted();
		FVideoTrack& VideoTrack = VideoTracks[VideoTrackIndex];
		{
			// bit rate
			VideoTrack.BitRate = ::MFGetAttributeUINT32(MediaType, MF_MT_AVG_BITRATE, 0);
			
			// dimensions
			if (SUCCEEDED(::MFGetAttributeSize(MediaType, MF_MT_FRAME_SIZE, (UINT32*)&VideoTrack.OutputDim.X, (UINT32*)&VideoTrack.OutputDim.Y)))
			{
				OutInfo += FString::Printf(TEXT("    Dimensions: %i x %i\n"), VideoTrack.OutputDim.X, VideoTrack.OutputDim.Y);
			}
			else
			{
				VideoTrack.OutputDim = FIntPoint::ZeroValue;
				OutInfo += FString::Printf(TEXT("    Dimensions: n/a\n"));
			}

			// frame rate
			UINT32 Numerator = 0;
			UINT32 Denominator = 1;

			if (SUCCEEDED(::MFGetAttributeRatio(MediaType, MF_MT_FRAME_RATE, &Numerator, &Denominator)))
			{
				VideoTrack.FrameRate = static_cast<float>(Numerator) / Denominator;
				OutInfo += FString::Printf(TEXT("    Frame Rate: %g fps\n"), VideoTrack.FrameRate);
			}
			else
			{
				VideoTrack.FrameRate = 0.0f;
				OutInfo += FString::Printf(TEXT("    Frame Rate: n/a\n"));
			}

			// sample stride
			long SampleStride = 0;
			
			if (OutputSubType == MFVideoFormat_RGB32)
			{
				::MFGetAttributeUINT32(MediaType, MF_MT_DEFAULT_STRIDE, 0);

				if (SampleStride == 0)
				{
					::MFGetStrideForBitmapInfoHeader(SubType.Data1, VideoTrack.OutputDim.X, &SampleStride);
				}
			}

			if (SampleStride == 0)
			{
				SampleStride = VideoTrack.OutputDim.X * 4;
			}
			else if (SampleStride < 0)
			{
				SampleStride = -SampleStride;
			}

			// sink format
			if (OutputSubType == MFVideoFormat_RGB32)
			{
				VideoTrack.BufferDim = FIntPoint(SampleStride / 4, VideoTrack.OutputDim.Y);
				VideoTrack.SinkFormat = EMediaTextureSinkFormat::CharBMP;
			}
			else
			{
				if (SubType != MFVideoFormat_MJPG)
				{
					SampleStride = Align(SampleStride, 64);
				}

				VideoTrack.BufferDim = FIntPoint(SampleStride / 8, VideoTrack.OutputDim.Y);
				VideoTrack.SinkFormat = EMediaTextureSinkFormat::CharYUY2;
			}
		}

		if (Sampler != NULL)
		{
			Sampler->OnSample().AddRaw(this, &FWmfMediaTracks::HandleMediaSamplerSample, EMediaTrackType::Video);
		}

		StreamInfo = &VideoTrack;
	}

	// get stream info
	if (StreamInfo != nullptr)
	{
		PWSTR OutString = NULL;
		UINT32 OutLength;

		if (SUCCEEDED(StreamDescriptor->GetAllocatedString(MF_SD_LANGUAGE, &OutString, &OutLength)))
		{
			StreamInfo->Language = OutString;
			::CoTaskMemFree(OutString);
		}

		if (SUCCEEDED(StreamDescriptor->GetAllocatedString(MF_SD_STREAM_NAME, &OutString, &OutLength)))
		{
			StreamInfo->Name = OutString;
			::CoTaskMemFree(OutString);
		}

		StreamInfo->DisplayName = (StreamInfo->Name.IsEmpty())
			? FText::Format(LOCTEXT("UnnamedStreamFormat", "Unnamed Stream {0}"), FText::AsNumber((uint32)StreamIndex))
			: FText::FromString(StreamInfo->Name);

		StreamInfo->Descriptor = StreamDescriptor;
		StreamInfo->OutputType = OutputType;
		StreamInfo->Protected = Protected;
		StreamInfo->Sampler = Sampler;
		StreamInfo->StreamIndex = StreamIndex;
	}
}


void FWmfMediaTracks::InitializeAudioSink()
{
	if ((AudioSink == nullptr) || (SelectedAudioTrack == INDEX_NONE))
	{
		return;
	}

	const FAudioTrack& AudioTrack = AudioTracks[SelectedAudioTrack];
	AudioSink->InitializeAudioSink(AudioTrack.NumChannels, AudioTrack.SampleRate);
}


void FWmfMediaTracks::InitializeOverlaySink()
{
	if ((OverlaySink == nullptr) || (SelectedCaptionTrack == INDEX_NONE))
	{
		return;
	}

	OverlaySink->InitializeOverlaySink();
}


void FWmfMediaTracks::InitializeVideoSink()
{
	if ((VideoSink == nullptr) || (SelectedVideoTrack == INDEX_NONE))
	{
		return;
	}

	const FVideoTrack& VideoTrack = VideoTracks[SelectedVideoTrack];
	VideoSink->InitializeTextureSink(VideoTrack.OutputDim, VideoTrack.BufferDim, VideoTrack.SinkFormat, EMediaTextureSinkMode::Buffered);
}


/* FWmfMediaTracks event handlers
 *****************************************************************************/

void FWmfMediaTracks::HandleMediaSamplerClock(EWmfMediaSamplerClockEvent Event, EMediaTrackType TrackType)
{
	// IMFSampleGrabberSinkCallback callbacks seem to be broken (always returns Stopped)
	// We handle sink synchronization via SetPaused() as a workaround
}


void FWmfMediaTracks::HandleMediaSamplerSample(const uint8* Buffer, uint32 Size, FTimespan Duration, FTimespan Time, EMediaTrackType TrackType)
{
	FScopeLock Lock(&CriticalSection);

	if (Buffer == nullptr)
	{
		return;
	}

	// forward the sample to the sink
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		if ((AudioSink != nullptr) && AudioTracks.IsValidIndex(SelectedAudioTrack))
		{
			AudioSink->PlayAudioSink(Buffer, Size, Time);
		}
		break;

	case EMediaTrackType::Caption:
		if ((OverlaySink != nullptr) && CaptionTracks.IsValidIndex(SelectedCaptionTrack))
		{
			OverlaySink->AddOverlaySinkText(FText::FromString((TCHAR*)Buffer), EMediaOverlayType::Caption, Time, Duration, TOptional<FVector2D>());
		}
		break;

	case EMediaTrackType::Video:
		if ((VideoSink != nullptr) && VideoTracks.IsValidIndex(SelectedVideoTrack))
		{
			VideoSink->UpdateTextureSinkBuffer(Buffer);
			VideoSink->DisplayTextureSinkBuffer(Time);
		}
		break;
	}
}


#include "HideWindowsPlatformTypes.h"

#undef LOCTEXT_NAMESPACE

#endif //WMFMEDIA_SUPPORTED_PLATFORM
