// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSAudioBuffer.cpp: Unreal IOSAudio buffer interface object.
 =============================================================================*/

/*------------------------------------------------------------------------------------
	Includes
 ------------------------------------------------------------------------------------*/

#include "IOSAudioDevice.h"
#include "AudioEffect.h"
#include "Engine.h"
#include "IAudioFormat.h"

/*------------------------------------------------------------------------------------
	FIOSAudioSoundBuffer
 ------------------------------------------------------------------------------------*/

FIOSAudioSoundBuffer::FIOSAudioSoundBuffer(FIOSAudioDevice* InAudioDevice, ESoundFormat InSoundFormat):
	FSoundBuffer(InAudioDevice),
	SoundFormat(InSoundFormat),
	SampleData(NULL),
	SampleRate(0),
	CompressedBlockSize(0),
	UncompressedBlockSize(0),
	BufferSize(0)
{
	;
}

FIOSAudioSoundBuffer::~FIOSAudioSoundBuffer(void)
{
	if (bAllocationInPermanentPool)
	{
		UE_LOG(LogIOSAudio, Fatal, TEXT("Can't free resource '%s' as it was allocated in permanent pool."), *ResourceName);
	}

	FMemory::Free(SampleData);
	SampleData = NULL;
}

int32 FIOSAudioSoundBuffer::GetSize(void)
{
	int32 TotalSize = 0;
	
	switch (SoundFormat)
	{
		case SoundFormat_LPCM:
		case SoundFormat_ADPCM:
			TotalSize = BufferSize;
			break;
	}
	
	return TotalSize;
}

FIOSAudioSoundBuffer* FIOSAudioSoundBuffer::CreateNativeBuffer(FIOSAudioDevice* IOSAudioDevice, USoundWave* InWave)
{
	FWaveModInfo WaveInfo;

	InWave->InitAudioResource(IOSAudioDevice->GetRuntimeFormat(InWave));
	if (!InWave->ResourceData || InWave->ResourceSize <= 0 || !WaveInfo.ReadWaveInfo(InWave->ResourceData, InWave->ResourceSize))
	{
		InWave->RemoveAudioResource();
		return NULL;
	}

	uint32 UncompressedBlockSize = 0;
	uint32 CompressedBlockSize = 0;
	const uint32 PreambleSize = 7;
	const uint32 BlockSize = *WaveInfo.pBlockAlign;

	switch (*WaveInfo.pFormatTag)
	{
	case SoundFormat_ADPCM:
		// (BlockSize - PreambleSize) * 2 (samples per byte) + 2 (preamble samples)
		UncompressedBlockSize = (2 + (BlockSize - PreambleSize) * 2) * sizeof(int16);
		CompressedBlockSize = BlockSize;

		if ((WaveInfo.SampleDataSize % CompressedBlockSize) != 0)
		{
			InWave->RemoveAudioResource();
			return NULL;
		}
		break;

	case SoundFormat_LPCM:
		break;
	}

	// Create new buffer
	FIOSAudioSoundBuffer* Buffer = new FIOSAudioSoundBuffer(IOSAudioDevice, static_cast<ESoundFormat>(*WaveInfo.pFormatTag));

	Buffer->NumChannels = InWave->NumChannels;
	Buffer->SampleRate = InWave->SampleRate;
	Buffer->UncompressedBlockSize = UncompressedBlockSize;
	Buffer->CompressedBlockSize = CompressedBlockSize;
	Buffer->BufferSize = WaveInfo.SampleDataSize;

	Buffer->SampleData = static_cast<int16*>(FMemory::Malloc(Buffer->BufferSize));
	FMemory::Memcpy(Buffer->SampleData, WaveInfo.SampleDataStart, Buffer->BufferSize);
	
	FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
	check(AudioDeviceManager != nullptr);

	AudioDeviceManager->TrackResource(InWave, Buffer);
	InWave->RemoveAudioResource();

	return Buffer;
}

FIOSAudioSoundBuffer* FIOSAudioSoundBuffer::Init(FIOSAudioDevice* IOSAudioDevice, USoundWave* InWave)
{
	// Can't create a buffer without any source data
	if (InWave == NULL || InWave->NumChannels == 0)
	{
		return NULL;
	}

	FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();

	FIOSAudioSoundBuffer *Buffer = NULL;

	switch (static_cast<EDecompressionType>(InWave->DecompressionType))
	{
		case DTYPE_Setup:
			// Has circumvented pre-cache mechanism - pre-cache now
			IOSAudioDevice->Precache(InWave, true, false);
			
			// Recall this function with new decompression type
			return Init(IOSAudioDevice, InWave);
			
		case DTYPE_Native:
			if (InWave->ResourceID)
			{
				Buffer = static_cast<FIOSAudioSoundBuffer*>(AudioDeviceManager->WaveBufferMap.FindRef(InWave->ResourceID));
			}

			if (!Buffer)
			{
				Buffer = CreateNativeBuffer(IOSAudioDevice, InWave);
			}
			break;
			
		case DTYPE_Invalid:
		case DTYPE_Preview:
		case DTYPE_Procedural:
		case DTYPE_RealTime:
		default:
			// Invalid will be set if the wave cannot be played
			UE_LOG( LogIOSAudio, Warning, TEXT("Init Buffer on unsupported sound type name = %s type = %d"), *InWave->GetName(), int32(InWave->DecompressionType));
			break;
	}

	return Buffer;
}
