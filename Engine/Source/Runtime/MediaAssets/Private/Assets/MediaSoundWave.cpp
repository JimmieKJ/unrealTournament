// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaAssetsPCH.h"
#include "MediaSoundWave.h"


const int32 ZeroBufferSize = 1024;


/* UMediaSoundWave structors
 *****************************************************************************/

UMediaSoundWave::UMediaSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Paused(true)
	, SinkNumChannels(0)
	, SinkSampleRate(0)
{
	bLooping = false;
	bProcedural = true;
	Duration = INDEFINITELY_LOOPING_DURATION;

	// @todo media: add support for reinitializing sound wave
	NumChannels = 2;
	SampleRate = 44100;
}


/* USoundWave interface
 *****************************************************************************/

int32 UMediaSoundWave::GeneratePCMData(uint8* Data, const int32 SamplesRequested)
{
	if ((Data == nullptr) || (SamplesRequested <= 0))
	{
		return 0;
	}

	// return queued samples
	if (!Paused && (SinkNumChannels > 0) && (SinkSampleRate > 0) && (QueuedAudio.Num() > 0))
	{
		FScopeLock Lock(&CriticalSection);

		// copy queued audio to buffer
		if ((SinkNumChannels == NumChannels) && (SinkSampleRate == SampleRate))
		{
			const int32 BytesRequested = SamplesRequested * sizeof(int16);
			const int32 BytesToCopy = FMath::Min<int32>(BytesRequested, QueuedAudio.Num());

			FMemory::Memcpy((void*)Data, &QueuedAudio[0], BytesToCopy);
			QueuedAudio.RemoveAt(0, BytesToCopy);

			return BytesToCopy;
		}

		// poor man's re-sampling
		const float ResampleRatio = (float)SinkSampleRate / 44100.0f;
		const int32 SamplesAvailable = (QueuedAudio.Num() / sizeof(int16)) / ResampleRatio - 2 * SinkNumChannels;
		const int32 NumFrames = FMath::Min<int32>(SamplesRequested, SamplesAvailable) / NumChannels;
		
		if (NumFrames > 0)
		{
			int16* InputSamples = (int16*)QueuedAudio.GetData();
			int16* OutputSamples = (int16*)Data;
			
			float InputFrameInterp = 0.0f;
			int32 InputIndex = 0;

			for (int32 OutputFrame = 0; OutputFrame < NumFrames; ++OutputFrame)
			{
				int32 InputFrame = (int32)InputFrameInterp;
				float Alpha = InputFrameInterp - InputFrame;

				InputIndex = InputFrame * SinkNumChannels;

				if (SinkNumChannels == 1)
				{
					// mono to stereo
					const int16 OutputSample = (int16)FMath::Lerp((float)InputSamples[InputIndex], (float)InputSamples[InputIndex + 1], Alpha);

					OutputSamples[0] = OutputSample;
					OutputSamples[1] = OutputSample;
				}
				else if (SinkNumChannels == 2)
				{
					// stereo to stereo
					const int16 OutputSampleL = (int16)FMath::Lerp((float)InputSamples[InputIndex], (float)InputSamples[InputIndex + 2], Alpha);
					const int16 OutputSampleR = (int16)FMath::Lerp((float)InputSamples[InputIndex + 1], (float)InputSamples[InputIndex + 3], Alpha);

					OutputSamples[0] = OutputSampleL;
					OutputSamples[1] = OutputSampleR;
				}
				else if (SinkNumChannels == 6)
				{
					// 5.1 to stereo
					const int16 OutputSampleC = (int16)FMath::Lerp((float)InputSamples[InputIndex], (float)InputSamples[InputIndex + 6], Alpha);
					const int16 OutputSampleL = (int16)FMath::Lerp((float)InputSamples[InputIndex + 1], (float)InputSamples[InputIndex + 7], Alpha);
					const int16 OutputSampleR = (int16)FMath::Lerp((float)InputSamples[InputIndex + 2], (float)InputSamples[InputIndex + 8], Alpha);
					const int16 OutputSampleLS = (int16)FMath::Lerp((float)InputSamples[InputIndex + 3], (float)InputSamples[InputIndex + 9], Alpha);
					const int16 OutputSampleRS = (int16)FMath::Lerp((float)InputSamples[InputIndex + 4], (float)InputSamples[InputIndex + 10], Alpha);
					const int16 OutputSampleLFE = (int16)FMath::Lerp((float)InputSamples[InputIndex + 5], (float)InputSamples[InputIndex + 11], Alpha);

					OutputSamples[0] = (OutputSampleL + OutputSampleLS) / 2 + (OutputSampleLFE + OutputSampleC) / 4;
					OutputSamples[1] = (OutputSampleR + OutputSampleRS) / 2 + (OutputSampleLFE + OutputSampleC) / 4;
				}

				OutputSamples += NumChannels;
				InputIndex += SinkNumChannels;
				InputFrameInterp += ResampleRatio;
			}

			QueuedAudio.RemoveAt(0, InputIndex * sizeof(int16));

			return (uint8*)OutputSamples - (uint8*)Data;
		}
	}

	// return zero samples if paused or buffer underrun
	if (!Paused && (SinkNumChannels > 0) && (SinkSampleRate > 0))
	{
		UE_LOG(LogMediaAssets, Verbose, TEXT("MediaSoundWave buffer underrun."));
	}

	FMemory::Memzero(Data, ZeroBufferSize);

	return ZeroBufferSize;
}


FByteBulkData* UMediaSoundWave::GetCompressedData(FName Format)
{
	return nullptr; // uncompressed audio
}


int32 UMediaSoundWave::GetResourceSizeForFormat(FName Format)
{
	return 0; // procedural sound
}


void UMediaSoundWave::InitAudioResource(FByteBulkData& CompressedData)
{
	check(false); // should never be pushing compressed data to this class
}


bool UMediaSoundWave::InitAudioResource(FName Format)
{
	return true;
}


/* UObject interface
 *****************************************************************************/

void UMediaSoundWave::BeginDestroy()
{
	Super::BeginDestroy();

	BeginDestroyEvent.Broadcast(*this);
}


void UMediaSoundWave::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	// do nothing
}


SIZE_T UMediaSoundWave::GetResourceSize(EResourceSizeMode::Type Mode)
{
	return 0; // procedural sound
}


void UMediaSoundWave::Serialize(FArchive& Ar)
{
	// do not call the USoundWave version of serialize
	USoundBase::Serialize(Ar);
}


/* IMediaAudioSink interface
 *****************************************************************************/

void UMediaSoundWave::FlushAudioSink()
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("MediaSoundWave flushing sink."));

	FScopeLock Lock(&CriticalSection);
	QueuedAudio.Empty();
}


int32 UMediaSoundWave::GetAudioSinkChannels() const
{
	return SinkNumChannels;
}


int32 UMediaSoundWave::GetAudioSinkSampleRate() const
{
	return SinkSampleRate;
}


bool UMediaSoundWave::InitializeAudioSink(uint32 Channels, uint32 InSampleRate)
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("MediaSoundWave initializing sink with %i channels at %i Hz."), Channels, InSampleRate);

	if ((Channels != 1) && (Channels != 2) && (Channels != 6))
	{
		return false; // we currently only support mono, stereo, and 5.1
	}

	if ((Channels == SinkNumChannels) && (InSampleRate == SinkSampleRate))
	{
		return true;
	}

	FScopeLock Lock(&CriticalSection);
	QueuedAudio.Empty();

	SinkNumChannels = Channels;
	SinkSampleRate = InSampleRate;

	return true;
}


void UMediaSoundWave::PauseAudioSink()
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("MediaSoundWave pausing sink."));
	Paused = true;
}


void UMediaSoundWave::PlayAudioSink(const uint8* SampleBuffer, uint32 BufferSize, FTimespan Time)
{
	UE_LOG(LogMediaAssets, VeryVerbose, TEXT("MediaSoundWave playing %i bytes at %s."), BufferSize, *Time.ToString());

	FScopeLock Lock(&CriticalSection);
	QueuedAudio.Append(SampleBuffer, BufferSize);
}


void UMediaSoundWave::ResumeAudioSink()
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("MediaSoundWave resuming sink."));
	Paused = false;
}


void UMediaSoundWave::ShutdownAudioSink()
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("MediaSoundWave shutting down sink."));

	FScopeLock Lock(&CriticalSection);
	QueuedAudio.Empty();

	Paused = true;
	SinkNumChannels = 0;
	SinkSampleRate = 0;
}
