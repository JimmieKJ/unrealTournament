// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "WmfMediaPCH.h"

#if WMFMEDIA_SUPPORTED_PLATFORM

#include "WmfMediaSampler.h"
#include "WmfMediaTracks.h"
#include "AllowWindowsPlatformTypes.h"


#define LOCTEXT_NAMESPACE "FWmfMediaTracks"


/* FWmfMediaTracks structors
 *****************************************************************************/

FWmfMediaTracks::FWmfMediaTracks()
	: SelectedAudioTrack(INDEX_NONE)
	, SelectedCaptionTrack(INDEX_NONE)
	, SelectedImageTrack(INDEX_NONE)
	, SelectedVideoTrack(INDEX_NONE)
	, AudioSink(nullptr)
	, CaptionSink(nullptr)
	, ImageSink(nullptr)
	, VideoSink(nullptr)
{ }


FWmfMediaTracks::~FWmfMediaTracks()
{
	Reset();
}


/* FWmfMediaTracks interface
 *****************************************************************************/

void FWmfMediaTracks::AddTrack(const GUID& MajorType, IMFMediaType* MediaType, IMFMediaType* OutputType, FWmfMediaSampler* Sampler, IMFStreamDescriptor* StreamDescriptor, DWORD StreamIndex)
{
	check(PresentationDescriptor != nullptr);

	FScopeLock Lock(&CriticalSection);

	// get stream info
	FString Language;
	{
		PWSTR LanguageString = NULL;
		UINT32 OutLength;

		if (SUCCEEDED(StreamDescriptor->GetAllocatedString(MF_SD_LANGUAGE, &LanguageString, &OutLength)))
		{
			Language = LanguageString;
			CoTaskMemFree(LanguageString);
		}
	}

	FString Name;
	{
		PWSTR NameString = NULL;
		UINT32 OutLength;

		if (SUCCEEDED(StreamDescriptor->GetAllocatedString(MF_SD_STREAM_NAME, &NameString, &OutLength)))
		{
			Name = NameString;
			CoTaskMemFree(NameString);
		}
	}

	FText DisplayName = (Name.IsEmpty())
		? FText::Format(LOCTEXT("UnnamedStreamFormat", "Unnamed Stream {0}"), FText::AsNumber((uint32)StreamIndex))
		: FText::FromString(Name);
		
	bool Protected = ::MFGetAttributeUINT32(StreamDescriptor, MF_SD_PROTECTED, FALSE) != 0;
		
	// add track
	if (MajorType == MFMediaType_Audio)
	{
		const int32 AudioTrackIndex = AudioTracks.AddDefaulted();
		FAudioTrack& AudioTrack = AudioTracks[AudioTrackIndex];
		{
			AudioTrack.DisplayName = DisplayName;
			AudioTrack.Language = Language;
			AudioTrack.Name = Name;
			AudioTrack.NumChannels = ::MFGetAttributeUINT32(MediaType, MF_MT_AUDIO_NUM_CHANNELS, 0);
			AudioTrack.Protected = Protected;
			AudioTrack.SampleRate = ::MFGetAttributeUINT32(MediaType, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
			AudioTrack.Sampler = Sampler;
			AudioTrack.StreamDescriptor = StreamDescriptor;
			AudioTrack.StreamIndex = StreamIndex;
		}

		Sampler->OnClock().AddRaw(this, &FWmfMediaTracks::HandleMediaSamplerClock, EMediaTrackType::Audio);
		Sampler->OnSample().AddRaw(this, &FWmfMediaTracks::HandleMediaSamplerSample, EMediaTrackType::Audio);
	}
	else if (MajorType == MFMediaType_Image)
	{
		const int32 ImageTrackIndex = ImageTracks.AddDefaulted();
		FImageTrack& ImageTrack = ImageTracks[ImageTrackIndex];
		{
			ImageTrack.DisplayName = DisplayName;
			ImageTrack.Language = Language;
			ImageTrack.Name = Name;
			ImageTrack.Protected = Protected;
			ImageTrack.Sampler = Sampler;
			ImageTrack.StreamDescriptor = StreamDescriptor;
			ImageTrack.StreamIndex = StreamIndex;
			
			::MFGetAttributeSize(MediaType, MF_MT_FRAME_SIZE, (UINT32*)&ImageTrack.Dimensions.X, (UINT32*)&ImageTrack.Dimensions.Y);
		}
			
		Sampler->OnSample().AddRaw(this, &FWmfMediaTracks::HandleMediaSamplerSample, EMediaTrackType::Image);
	}
	else if (MajorType == MFMediaType_SAMI)
	{
		const int32 CaptionTrackIndex = CaptionTracks.AddDefaulted();
		FCaptionTrack& CaptionTrack = CaptionTracks[CaptionTrackIndex];
		{
			CaptionTrack.DisplayName = DisplayName;
			CaptionTrack.Language = Language;
			CaptionTrack.Name = Name;
			CaptionTrack.Protected = Protected;
			CaptionTrack.Sampler = Sampler;
			CaptionTrack.StreamDescriptor = StreamDescriptor;
			CaptionTrack.StreamIndex = StreamIndex;
		}

		Sampler->OnSample().AddRaw(this, &FWmfMediaTracks::HandleMediaSamplerSample, EMediaTrackType::Caption);
	}
	else if (MajorType == MFMediaType_Video)
	{
		const int32 VideoTrackIndex = VideoTracks.AddDefaulted();
		FVideoTrack& VideoTrack = VideoTracks[VideoTrackIndex];
		{
			VideoTrack.BitRate = ::MFGetAttributeUINT32(MediaType, MF_MT_AVG_BITRATE, 0);
			VideoTrack.DisplayName = DisplayName;
			VideoTrack.Language = Language;
			VideoTrack.Name = Name;
			VideoTrack.Protected = Protected;
			VideoTrack.Sampler = Sampler;
			VideoTrack.StreamDescriptor = StreamDescriptor;
			VideoTrack.StreamIndex = StreamIndex;

			::MFGetAttributeSize(MediaType, MF_MT_FRAME_SIZE, (UINT32*)&VideoTrack.Dimensions.X, (UINT32*)&VideoTrack.Dimensions.Y);

			// frame rate
			UINT32 Numerator = 0;
			UINT32 Denominator = 1;

			VideoTrack.FrameRate = SUCCEEDED(::MFGetAttributeRatio(MediaType, MF_MT_FRAME_RATE, &Numerator, &Denominator))
				? static_cast<float>(Numerator) / Denominator
				: 0.0f;

			// sink format
			GUID SubType;

			if (SUCCEEDED(OutputType->GetGUID(MF_MT_SUBTYPE, &SubType)) && ((SubType == MFVideoFormat_YUY2)))
			{
				VideoTrack.Pitch = VideoTrack.Dimensions.X * 2;
				VideoTrack.SinkFormat = EMediaTextureSinkFormat::CharYUY2;
			}
			else
			{
				VideoTrack.Pitch = VideoTrack.Dimensions.X * 4;
				VideoTrack.SinkFormat = EMediaTextureSinkFormat::CharBGRA;
			}
		}

		Sampler->OnSample().AddRaw(this, &FWmfMediaTracks::HandleMediaSamplerSample, EMediaTrackType::Video);
	}
}


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


void FWmfMediaTracks::Flush()
{
	FScopeLock Lock(&CriticalSection);

	if (AudioSink != nullptr)
	{
		AudioSink->FlushAudioSink();
	}
}


void FWmfMediaTracks::Initialize(IMFPresentationDescriptor& InPresentationDescriptor)
{
	PresentationDescriptor = &InPresentationDescriptor;
}


void FWmfMediaTracks::Reset()
{
	FScopeLock Lock(&CriticalSection);

	SelectedAudioTrack = INDEX_NONE;
	SelectedCaptionTrack = INDEX_NONE;
	SelectedImageTrack = INDEX_NONE;
	SelectedVideoTrack = INDEX_NONE;

	if (AudioSink != nullptr)
	{
		AudioSink->ShutdownAudioSink();
	}

	if (CaptionSink != nullptr)
	{
		CaptionSink->ShutdownStringSink();
	}

	if (ImageSink != nullptr)
	{
		ImageSink->ShutdownTextureSink();
	}

	if (VideoSink != nullptr)
	{
		VideoSink->ShutdownTextureSink();
	}

	AudioTracks.Empty();
	CaptionTracks.Empty();
	ImageTracks.Empty();
	VideoTracks.Empty();

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


void FWmfMediaTracks::SetCaptionSink(IMediaStringSink* Sink)
{
	if (Sink != CaptionSink)
	{
		FScopeLock Lock(&CriticalSection);

		if (CaptionSink != nullptr)
		{
			CaptionSink->ShutdownStringSink();
		}

		CaptionSink = Sink;
		InitializeCaptionSink();
	}
}


void FWmfMediaTracks::SetImageSink(IMediaTextureSink* Sink)
{
	if (Sink != ImageSink)
	{
		FScopeLock Lock(&CriticalSection);

		if (ImageSink != nullptr)
		{
			ImageSink->ShutdownTextureSink();
		}

		ImageSink = Sink;
		InitializeImageSink();
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
	return VideoTracks.IsValidIndex(TrackIndex) ? VideoTracks[TrackIndex].Dimensions : FIntPoint::ZeroValue;
}


float FWmfMediaTracks::GetVideoTrackFrameRate(int32 TrackIndex) const
{
	return VideoTracks.IsValidIndex(TrackIndex) ? VideoTracks[TrackIndex].FrameRate : 0.0f;
}


bool FWmfMediaTracks::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
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
					PresentationDescriptor->DeselectStream(AudioTracks[SelectedAudioTrack].StreamIndex);
					SelectedAudioTrack = INDEX_NONE;
				}

				if ((TrackIndex != INDEX_NONE) && SUCCEEDED(PresentationDescriptor->SelectStream(AudioTracks[TrackIndex].StreamIndex)))
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
					PresentationDescriptor->DeselectStream(CaptionTracks[SelectedCaptionTrack].StreamIndex);
					SelectedCaptionTrack = INDEX_NONE;
				}

				if ((TrackIndex != INDEX_NONE) && SUCCEEDED(PresentationDescriptor->SelectStream(CaptionTracks[TrackIndex].StreamIndex)))
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

	case EMediaTrackType::Image:
		if ((TrackIndex == INDEX_NONE) || ImageTracks.IsValidIndex(TrackIndex))
		{
			if (TrackIndex != SelectedImageTrack)
			{
				if (SelectedImageTrack != INDEX_NONE)
				{
					PresentationDescriptor->DeselectStream(ImageTracks[SelectedImageTrack].StreamIndex);
					SelectedImageTrack = INDEX_NONE;
				}

				if ((TrackIndex != INDEX_NONE) && SUCCEEDED(PresentationDescriptor->SelectStream(ImageTracks[TrackIndex].StreamIndex)))
				{
					SelectedImageTrack = TrackIndex;
				}

				if (SelectedImageTrack == TrackIndex)
				{
					InitializeImageSink();
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
					PresentationDescriptor->DeselectStream(VideoTracks[SelectedVideoTrack].StreamIndex);
					SelectedVideoTrack = INDEX_NONE;
				}

				if ((TrackIndex != INDEX_NONE) && SUCCEEDED(PresentationDescriptor->SelectStream(VideoTracks[TrackIndex].StreamIndex)))
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


/* FWmfMediaTracks implementation
 *****************************************************************************/

void FWmfMediaTracks::InitializeAudioSink()
{
	if ((AudioSink == nullptr) || (SelectedAudioTrack == INDEX_NONE))
	{
		return;
	}

	const FAudioTrack& AudioTrack = AudioTracks[SelectedAudioTrack];
	AudioSink->InitializeAudioSink(AudioTrack.NumChannels, AudioTrack.SampleRate);
}


void FWmfMediaTracks::InitializeCaptionSink()
{
	if ((CaptionSink == nullptr) || (SelectedCaptionTrack == INDEX_NONE))
	{
		return;
	}

	CaptionSink->InitializeStringSink();
}


void FWmfMediaTracks::InitializeImageSink()
{
	if ((ImageSink == nullptr) || (SelectedImageTrack == INDEX_NONE))
	{
		return;
	}

	const FImageTrack& ImageTrack = ImageTracks[SelectedImageTrack];
	ImageSink->InitializeTextureSink(ImageTrack.Dimensions, EMediaTextureSinkFormat::CharBGRA, EMediaTextureSinkMode::Buffered);
}


void FWmfMediaTracks::InitializeVideoSink()
{
	if ((VideoSink == nullptr) || (SelectedVideoTrack == INDEX_NONE))
	{
		return;
	}

	const FVideoTrack& VideoTrack = VideoTracks[SelectedVideoTrack];
	VideoSink->InitializeTextureSink(VideoTrack.Dimensions, VideoTrack.SinkFormat, EMediaTextureSinkMode::Buffered);
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
		if ((CaptionSink != nullptr) && CaptionTracks.IsValidIndex(SelectedCaptionTrack))
		{
			CaptionSink->DisplayStringSinkString((TCHAR*)Buffer, Time);
		}
		break;

	case EMediaTrackType::Image:
		if ((ImageSink != nullptr) && ImageTracks.IsValidIndex(SelectedImageTrack))
		{
			ImageSink->UpdateTextureSinkBuffer(Buffer, ImageTracks[SelectedImageTrack].Dimensions.X * 4);
			ImageSink->DisplayTextureSinkBuffer(Time);
		}
		break;

	case EMediaTrackType::Video:
		if ((VideoSink != nullptr) && VideoTracks.IsValidIndex(SelectedVideoTrack))
		{
			VideoSink->UpdateTextureSinkBuffer(Buffer, VideoTracks[SelectedVideoTrack].Pitch);
			VideoSink->DisplayTextureSinkBuffer(Time);
		}
		break;
	}
}


#include "HideWindowsPlatformTypes.h"

#endif //WMFMEDIA_SUPPORTED_PLATFORM
