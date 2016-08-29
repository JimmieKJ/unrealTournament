// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Sound/SoundWaveProcedural.h"

USoundWaveProcedural::USoundWaveProcedural(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bProcedural = true;
	bReset = false;
	NumBufferUnderrunSamples = 512;
	NumSamplesToGeneratePerCallback = 1024;
	checkf(NumSamplesToGeneratePerCallback >= NumBufferUnderrunSamples, TEXT("Should generate more samples than this per callback."));
}

void USoundWaveProcedural::QueueAudio(const uint8* AudioData, const int32 BufferSize)
{
	if (BufferSize == 0 || !ensure((BufferSize % sizeof(int16)) == 0))
	{
		return;
	}

	TArray<uint8> NewAudioBuffer;
	NewAudioBuffer.AddUninitialized(BufferSize);
	FMemory::Memcpy(NewAudioBuffer.GetData(), AudioData, BufferSize);
	QueuedAudio.Enqueue(NewAudioBuffer);

	AvailableByteCount.Add(BufferSize);
}

void USoundWaveProcedural::PumpQueuedAudio()
{
	// Pump the enqueued audio
	TArray<uint8> NewQueuedBuffer;
	while (QueuedAudio.Dequeue(NewQueuedBuffer))
	{
		AudioBuffer.Append(NewQueuedBuffer);
	}
}

int32 USoundWaveProcedural::GeneratePCMData(uint8* PCMData, const int32 SamplesNeeded)
{
	// Check if we've been told to reset our audio buffer
	if (bReset)
	{
		bReset = false;
		AudioBuffer.Reset();
	}

	int32 SamplesAvailable = AudioBuffer.Num() / sizeof(int16);
	int32 SamplesToGenerate = FMath::Min(NumSamplesToGeneratePerCallback, SamplesNeeded);

	check(SamplesToGenerate >= NumBufferUnderrunSamples);

	if (SamplesAvailable < SamplesToGenerate && OnSoundWaveProceduralUnderflow.IsBound())
	{
		OnSoundWaveProceduralUnderflow.Execute(this, SamplesToGenerate);
	}

	PumpQueuedAudio();

	SamplesAvailable = AudioBuffer.Num() / sizeof(int16);

	// Wait until we have enough samples that are requested before starting.
	if (SamplesAvailable >= SamplesToGenerate)
	{
		const int32 SamplesToCopy = FMath::Min<int32>(SamplesToGenerate, SamplesAvailable);
		const int32 BytesToCopy = SamplesToCopy * sizeof(int16);

		FMemory::Memcpy((void*)PCMData, &AudioBuffer[0], BytesToCopy);
		AudioBuffer.RemoveAt(0, BytesToCopy);

		// Decrease the available by count
		AvailableByteCount.Subtract(BytesToCopy);

		return BytesToCopy;
	}

	// There wasn't enough data ready, write out zeros
	const int32 BytesCopied = NumBufferUnderrunSamples * sizeof(int16);
	FMemory::Memzero(PCMData, BytesCopied);
	return BytesCopied;
}

void USoundWaveProcedural::ResetAudio()
{
	// Empty out any enqueued audio buffers
	QueuedAudio.Empty();

	// Flag that we need to reset our audio buffer (on the audio thread)
	bReset = true;
}

int32 USoundWaveProcedural::GetAvailableAudioByteCount()
{
	return AvailableByteCount.GetValue();
}

SIZE_T USoundWaveProcedural::GetResourceSize(EResourceSizeMode::Type Mode)
{
	return 0;
}

int32 USoundWaveProcedural::GetResourceSizeForFormat(FName Format)
{
	return 0;
}

void USoundWaveProcedural::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	// SoundWaveProcedural should never be in the asset registry
	check(false);
}

bool USoundWaveProcedural::HasCompressedData(FName Format) const
{
	return false;
}

FByteBulkData* USoundWaveProcedural::GetCompressedData(FName Format)
{
	// SoundWaveProcedural does not have compressed data and should generally not be asked about it
	return nullptr;
}

void USoundWaveProcedural::Serialize(FArchive& Ar)
{
	// Do not call the USoundWave version of serialize
	USoundBase::Serialize(Ar);
}

void USoundWaveProcedural::InitAudioResource(FByteBulkData& CompressedData)
{
	// Should never be pushing compressed data to a SoundWaveProcedural
	check(false);
}

bool USoundWaveProcedural::InitAudioResource(FName Format)
{
	// Nothing to be done to initialize a USoundWaveProcedural
	return true;
}