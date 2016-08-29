// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSAudioSource.cpp: Unreal IOSAudio source interface object.
 =============================================================================*/

/*------------------------------------------------------------------------------------
	Includes
 ------------------------------------------------------------------------------------*/

#include "IOSAudioDevice.h"
#include "Engine.h"

#define SOUND_SOURCE_FREE 0
#define SOUND_SOURCE_LOCKED 1

namespace
{
	inline bool LockSourceChannel(int32* ChannelLock)
	{
		check(ChannelLock != NULL);
		return FPlatformAtomics::InterlockedCompareExchange(ChannelLock, SOUND_SOURCE_LOCKED, SOUND_SOURCE_FREE) == SOUND_SOURCE_FREE;
	}

	inline void UnlockSourceChannel(int32* ChannelLock)
	{
		check(ChannelLock != NULL);
		int32 Result = FPlatformAtomics::InterlockedCompareExchange(ChannelLock, SOUND_SOURCE_FREE, SOUND_SOURCE_LOCKED);

		check(Result == SOUND_SOURCE_LOCKED);
	}

} // end namespace

// Linker will figure this out for us on IOS as it is now in a core source file
namespace ADPCM
{
	void DecodeBlock(const uint8* EncodedADPCMBlock, int32 BlockSize, int16* DecodedPCMData);
}

/*------------------------------------------------------------------------------------
	FIOSAudioSoundSource
 ------------------------------------------------------------------------------------*/

FIOSAudioSoundSource::FIOSAudioSoundSource(FIOSAudioDevice* InAudioDevice, uint32 InBusNumber) :
	FSoundSource(InAudioDevice),
	IOSAudioDevice(InAudioDevice),
	Buffer(NULL),
	StreamBufferSize(0),
	SampleRate(0),
	BusNumber(InBusNumber)
{
	check(IOSAudioDevice);
	FMemory::Memzero(StreamBufferStates, sizeof(StreamBufferStates));

	// Start in a disabled state
	DetachFromAUGraph();
	SampleRate = static_cast<int32>(IOSAudioDevice->MixerFormat.mSampleRate);

	AURenderCallbackStruct Input;	
	Input.inputProc = &IOSAudioRenderCallback;
	Input.inputProcRefCon = this;

	OSStatus Status = noErr;
	for (int32 Channel = 0; Channel < CHANNELS_PER_BUS; Channel++)
	{
		StreamBufferStates[Channel].StreamBuffer = NULL;
		StreamBufferStates[Channel].SampleReadCursor = 0;
		StreamBufferStates[Channel].StreamReadCursor = 0;
		StreamBufferStates[Channel].bFinished = false;
		StreamBufferStates[Channel].ChannelLock = SOUND_SOURCE_FREE;

		Status = AudioUnitSetProperty(IOSAudioDevice->GetMixerUnit(),
		                              kAudioUnitProperty_StreamFormat,
		                              kAudioUnitScope_Input,
		                              GetAudioUnitElement(Channel),
		                              &IOSAudioDevice->MixerFormat,
		                              sizeof(AudioStreamBasicDescription));
		UE_CLOG(Status != noErr, LogIOSAudio, Error, TEXT("Failed to set kAudioUnitProperty_StreamFormat for audio mixer unit: BusNumber=%d, Channel=%d"), BusNumber, Channel);

		Status = AudioUnitSetParameter(IOSAudioDevice->GetMixerUnit(),
		                               k3DMixerParam_Distance,
		                               kAudioUnitScope_Input,
		                               GetAudioUnitElement(Channel),
		                               1.0f,
		                               0);
		UE_CLOG(Status != noErr, LogIOSAudio, Error, TEXT("Failed to set k3DMixerParam_Distance for audio mixer unit: BusNumber=%d, Channel=%d"), BusNumber, Channel);

		Status = AUGraphSetNodeInputCallback(IOSAudioDevice->GetAudioUnitGraph(),
		                                     IOSAudioDevice->GetMixerNode(), 
		                                     GetAudioUnitElement(Channel),
		                                     &Input);
		UE_CLOG(Status != noErr, LogIOSAudio, Error, TEXT("Failed to set input callback for audio mixer node: BusNumber=%d, Channel=%d"), BusNumber, Channel);
	}
}

FIOSAudioSoundSource::~FIOSAudioSoundSource(void)
{
	for (int32 Channel = 0; Channel < CHANNELS_PER_BUS; Channel++)
	{
		FMemory::Free(StreamBufferStates[Channel].StreamBuffer);
		StreamBufferStates[Channel].StreamBuffer = NULL;
	}
}

bool FIOSAudioSoundSource::Init(FWaveInstance* InWaveInstance)
{
	if (InWaveInstance->OutputTarget == EAudioOutputTarget::Controller)
	{
		return false;
	}

	Buffer = FIOSAudioSoundBuffer::Init(IOSAudioDevice, InWaveInstance->WaveData);

	if (Buffer == NULL || Buffer->NumChannels <= 0 || (Buffer->SoundFormat != SoundFormat_LPCM && Buffer->SoundFormat != SoundFormat_ADPCM))
	{
		return false;
	}

	SCOPE_CYCLE_COUNTER(STAT_AudioSourceInitTime);

	uint32 BufferSize = StreamBufferSize;
	bool bBufferSizeChanged = false;
	bool bSampleRateChanged = false;

	WaveInstance = InWaveInstance;
	switch (Buffer->SoundFormat)
	{
	case SoundFormat_LPCM:
		for (int32 Channel = 0; Channel < Buffer->NumChannels; ++Channel)
		{
			// Input for multi-channel audio IS interlaced
			StreamBufferStates[Channel].SampleReadCursor = Channel;
			StreamBufferStates[Channel].StreamReadCursor = 0;
			StreamBufferStates[Channel].bFinished = false;
				
			FMemory::Free(StreamBufferStates[Channel].StreamBuffer);
			StreamBufferStates[Channel].StreamBuffer = NULL;
		}
			
		for (int32 Channel = Buffer->NumChannels; Channel < CHANNELS_PER_BUS; ++Channel)
		{
			StreamBufferStates[Channel].bFinished = true;
			
			FMemory::Free(StreamBufferStates[Channel].StreamBuffer);
			StreamBufferStates[Channel].StreamBuffer = NULL;
		}
		break;

	case SoundFormat_ADPCM:
		if (StreamBufferSize < Buffer->UncompressedBlockSize)
		{
			bBufferSizeChanged = true;
			BufferSize = Buffer->UncompressedBlockSize;
		}

		// Resize/allocate buffers as needed
		for (int32 Channel = 0; Channel < Buffer->NumChannels; ++Channel)
		{
			// Input is separated into discrete sections in the buffer; they are NOT interlaced
			StreamBufferStates[Channel].SampleReadCursor = (Channel * Buffer->BufferSize) / Buffer->NumChannels;
			StreamBufferStates[Channel].StreamReadCursor = 0;
			StreamBufferStates[Channel].bFinished = false;

			if (!StreamBufferStates[Channel].StreamBuffer || bBufferSizeChanged)
			{
				FMemory::Free(StreamBufferStates[Channel].StreamBuffer);
				StreamBufferStates[Channel].StreamBuffer = static_cast<int16*>(FMemory::Malloc(BufferSize));
			}
		}

		// Release unneeded streaming buffers, in case the number of channels has been reduced
		for (int32 Channel = Buffer->NumChannels; Channel < CHANNELS_PER_BUS; ++Channel)
		{
			StreamBufferStates[Channel].bFinished = true;

			FMemory::Free(StreamBufferStates[Channel].StreamBuffer);
			StreamBufferStates[Channel].StreamBuffer = NULL;
		}
			
		StreamBufferSize = BufferSize;
		break;
	}

	AudioStreamBasicDescription StreamFormat;
	AudioUnitParameterValue Pan = 0.0f;

	if (SampleRate != Buffer->SampleRate)
	{
		bSampleRateChanged = true;
		SampleRate = Buffer->SampleRate;

		StreamFormat = IOSAudioDevice->MixerFormat;
		StreamFormat.mSampleRate = static_cast<Float64>(SampleRate);
	}

	OSStatus Status = noErr;
	for (int32 Channel = 0; Channel < Buffer->NumChannels; ++Channel)
	{
		if (bSampleRateChanged)
		{
			Status = AudioUnitSetProperty(IOSAudioDevice->GetMixerUnit(),
			                              kAudioUnitProperty_StreamFormat,
			                              kAudioUnitScope_Input,
			                              GetAudioUnitElement(Channel),
			                              &StreamFormat,
			                              sizeof(AudioStreamBasicDescription));
			UE_CLOG(Status != noErr, LogIOSAudio, Error, TEXT("Failed to set kAudioUnitProperty_StreamFormat for audio mixer unit: BusNumber=%d, Channel=%d"), BusNumber, Channel);
		}

		if(Buffer->NumChannels == 2)
		{
			const AudioUnitParameterValue AzimuthRangeScale = 90.f;
			Pan = (-1.0f + (Channel * 2.0f)) * AzimuthRangeScale;
		}
		else if (!WaveInstance->bUseSpatialization)
		{
			Pan = 0.0f;
		}

		Status = AudioUnitSetParameter(IOSAudioDevice->GetMixerUnit(),
		                               k3DMixerParam_Azimuth,
		                               kAudioUnitScope_Input,
		                               GetAudioUnitElement(Channel),
		                               Pan,
		                               0);
		UE_CLOG(Status != noErr, LogIOSAudio, Error, TEXT("Failed to set k3DMixerParam_Azimuth for audio mixer unit: BusNumber=%d, Channel=%d"), BusNumber, Channel);
	}

	// Start in a disabled state
	DetachFromAUGraph();
	Update();

	return true;
}

void FIOSAudioSoundSource::Update(void)
{
	SCOPE_CYCLE_COUNTER(STAT_AudioUpdateSources);

	if (!WaveInstance || Paused)
	{
		return;
	}

	AudioUnitParameterValue Volume = 0.0f;

	if (!AudioDevice->IsAudioDeviceMuted())
	{
		Volume = WaveInstance->Volume * WaveInstance->VolumeMultiplier;
	}

	if (SetStereoBleed())
	{
		// Emulate the bleed to rear speakers followed by stereo fold down
		Volume *= 1.25f;
	}

	// Apply global multiplier to disable sound when not the foreground app
	Volume *= AudioDevice->GetPlatformAudioHeadroom();
	Volume = FMath::Clamp(Volume, 0.0f, 1.0f);

	// Convert to dB
	const AudioUnitParameterValue Gain = FMath::Clamp<float>(20.0f * log10(Volume), -100, 0.0f);
	const AudioUnitParameterValue Pitch = FMath::Clamp<float>(WaveInstance->Pitch, MIN_PITCH, MAX_PITCH);

	OSStatus Status = noErr;

	// We only adjust panning on playback for mono sounds that want spatialization
	if (Buffer->NumChannels == 1 && WaveInstance->bUseSpatialization)
	{
		// Compute the directional offset
		FVector Offset = WaveInstance->Location - IOSAudioDevice->PlayerLocation;
		const AudioUnitParameterValue AzimuthRangeScale = 90.0f;
		const AudioUnitParameterValue Pan = Offset.CosineAngle2D(IOSAudioDevice->PlayerRight) * AzimuthRangeScale;

		Status = AudioUnitSetParameter(IOSAudioDevice->GetMixerUnit(),
		                               k3DMixerParam_Azimuth,
		                               kAudioUnitScope_Input,
		                               GetAudioUnitElement(0),
		                               Pan,
		                               0);
		UE_CLOG(Status != noErr, LogIOSAudio, Error, TEXT("Failed to set k3DMixerParam_Azimuth for audio mixer unit: BusNumber=%d, Channel=%d"), BusNumber, 0);
	}

	for (int32 Channel = 0; Channel < Buffer->NumChannels; Channel++)
	{
		Status = AudioUnitSetParameter(IOSAudioDevice->GetMixerUnit(),
		                               k3DMixerParam_Gain,
		                               kAudioUnitScope_Input,
		                               GetAudioUnitElement(Channel),
		                               Gain,
		                               0);
		UE_CLOG(Status != noErr, LogIOSAudio, Error, TEXT("Failed to set k3DMixerParam_Gain for audio mixer unit: BusNumber=%d, Channel=%d"), BusNumber, Channel);

		Status = AudioUnitSetParameter(IOSAudioDevice->GetMixerUnit(),
		                               k3DMixerParam_PlaybackRate,
		                               kAudioUnitScope_Input,
		                               GetAudioUnitElement(Channel),
		                               Pitch,
		                               0);
		UE_CLOG(Status != noErr, LogIOSAudio, Error, TEXT("Failed to set k3DMixerParam_PlaybackRate for audio mixer unit: BusNumber=%d, Channel=%d"), BusNumber, Channel);
	}
}

void FIOSAudioSoundSource::Play(void)
{
	if (WaveInstance && AttachToAUGraph())
	{
		Paused = false;
		Playing = true;

		// Updates the source which sets the pitch and volume
		Update();
	}
}

void FIOSAudioSoundSource::Stop(void)
{
	if (WaveInstance)
	{
		Pause();
		
		// Lock each channel to block playback
		for (uint32 Channel = 0; Channel < CHANNELS_PER_BUS; Channel++)
		{
			while (!LockSourceChannel(&StreamBufferStates[Channel].ChannelLock))
			{
				UE_LOG(LogIOSAudio, Log, TEXT("Waiting for source to unlock: Channel=%d"), Channel);
				
				// Allow time for other threads to run
				FPlatformProcess::Sleep(0.0f);
			}
		}

		Paused = false;
		Playing = false;
		Buffer = NULL;

		FSoundSource::Stop();
		
		// Restore each channel to an unlocked state
		for (uint32 Channel = 0; Channel < CHANNELS_PER_BUS; Channel++)
		{
			UnlockSourceChannel(&StreamBufferStates[Channel].ChannelLock);
		}
	}
}

void FIOSAudioSoundSource::Pause(void)
{
	if (WaveInstance)
	{	
		if (Playing)
		{
			DetachFromAUGraph();
		}

		Paused = true;
	}
}

bool FIOSAudioSoundSource::IsFinished(void)
{
	// A paused source is not finished.
	if (Paused)
	{
		return false;
	}
	
	/* TODO::JTM - Jan 07, 2013 02:56PM - Properly handle wave instance notifications */
	if (WaveInstance && Playing)
	{
		if (WaveInstance->LoopingMode == LOOP_Never)
		{
			bool bFinished = true;
			for (int32 Channel = 0; Channel < Buffer->NumChannels && bFinished; Channel++)
			{
				bFinished = StreamBufferStates[Channel].bFinished;
			}

			return bFinished;
		}
		else if (WaveInstance->LoopingMode == LOOP_WithNotification)
		{
			bool bLoopCallback = true;
			for (int32 Channel = 0; Channel < Buffer->NumChannels && bLoopCallback; Channel++)
			{
				bLoopCallback = StreamBufferStates[Channel].bLooped;
			}

			// Notify the wave instance that the looping callback was hit
			if (bLoopCallback)
			{
				WaveInstance->NotifyFinished();

				// Reset the loop states for each active channel
				for (int32 Channel = 0; Channel < Buffer->NumChannels && bLoopCallback; Channel++)
				{
					StreamBufferStates[Channel].bLooped = false;
				}
			}
		}

		return false;
	}

	return true;
}

AudioUnitElement FIOSAudioSoundSource::GetAudioUnitElement(int32 Channel)
{
	check(Channel < CHANNELS_PER_BUS);
	return BusNumber * CHANNELS_PER_BUS + static_cast<uint32>(Channel);
}

bool FIOSAudioSoundSource::AttachToAUGraph()
{
	OSStatus Status = noErr;

	// Set a callback for the specified node's specified input
	for (int32 Channel = 0; Channel < Buffer->NumChannels; Channel++)
	{
		Status = AudioUnitSetParameter(IOSAudioDevice->GetMixerUnit(),
		                               k3DMixerParam_Enable, 
		                               kAudioUnitScope_Input,
		                               GetAudioUnitElement(Channel),
		                               1,
		                               0);
		UE_CLOG(Status != noErr, LogIOSAudio, Error, TEXT("Failed to set k3DMixerParam_Enable for audio mixer unit: BusNumber=%d, Channel=%d"), BusNumber, Channel);
	}

	return Status == noErr;
}

bool FIOSAudioSoundSource::DetachFromAUGraph()
{
	OSStatus Status = noErr;

	// Set a callback for the specified node's specified input
	for (int32 Channel = 0; Channel < CHANNELS_PER_BUS; Channel++)
	{
		Status = AudioUnitSetParameter(IOSAudioDevice->GetMixerUnit(),
		                               k3DMixerParam_Enable,
		                               kAudioUnitScope_Input,
		                               GetAudioUnitElement(Channel),
		                               0,
		                               0);
		UE_CLOG(Status != noErr, LogIOSAudio, Error, TEXT("Failed to set k3DMixerParam_Enable for audio mixer unit: BusNumber=%d, Channel=%d"), BusNumber, Channel);

		Status = AudioUnitSetParameter(IOSAudioDevice->GetMixerUnit(),
		                               k3DMixerParam_Gain,
		                               kAudioUnitScope_Input,
		                               GetAudioUnitElement(Channel),
		                               -120.0,
		                               0);
		UE_CLOG(Status != noErr, LogIOSAudio, Error, TEXT("Failed to set k3DMixerParam_Gain for audio mixer unit: BusNumber=%d, Channel=%d"), BusNumber, Channel);

	}

	return Status == noErr;
}

OSStatus FIOSAudioSoundSource::IOSAudioRenderCallback(void* RefCon, AudioUnitRenderActionFlags* ActionFlags,
                                                      const AudioTimeStamp* TimeStamp, UInt32 BusNumber,
                                                      UInt32 NumFrames, AudioBufferList* IOData)
{
	FIOSAudioSoundSource* Source = static_cast<FIOSAudioSoundSource*>(RefCon);
	UInt32 Channel = BusNumber % CHANNELS_PER_BUS;
	bool bFinished = false;
	
	AudioSampleType* OutData = reinterpret_cast<AudioSampleType*>(IOData->mBuffers[0].mData);
	
	// Make sure this channel should be rendering
	if (!LockSourceChannel(&Source->StreamBufferStates[Channel].ChannelLock))
	{
		FMemory::Memzero(OutData, NumFrames * sizeof(AudioSampleType));
		return -1;
	}
	else if (!Source->Buffer || Channel > Source->Buffer->NumChannels || !Source->IsPlaying() || Source->IsPaused() || Source->StreamBufferStates[Channel].bFinished)
	{
		UnlockSourceChannel(&Source->StreamBufferStates[Channel].ChannelLock);
		FMemory::Memzero(OutData, NumFrames * sizeof(AudioSampleType));
		return -1;
	}
	
	OSStatus Result = noErr;
	UInt32 NumFramesWritten = 0;

	switch (Source->Buffer->SoundFormat)
	{
	case SoundFormat_LPCM:
		Result = Source->IOSAudioRenderCallbackLPCM(OutData, NumFrames, NumFramesWritten, Channel);
		break;

	case SoundFormat_ADPCM:
		Result = Source->IOSAudioRenderCallbackADPCM(OutData, NumFrames, NumFramesWritten, Channel);
		break;

	default:
		UnlockSourceChannel(&Source->StreamBufferStates[Channel].ChannelLock);
		FMemory::Memzero(OutData, NumFrames * sizeof(AudioSampleType));
		return -1;
	}

	// Pad the rest of the output with zeros
	if (NumFrames > NumFramesWritten)
	{
		Source->StreamBufferStates[Channel].bFinished = true;
		FMemory::Memzero(OutData, (NumFrames - NumFramesWritten) * sizeof(AudioSampleType));
	}

	UnlockSourceChannel(&Source->StreamBufferStates[Channel].ChannelLock);
	return Result;
}

OSStatus FIOSAudioSoundSource::IOSAudioRenderCallbackADPCM(AudioSampleType* OutData, UInt32 NumFrames, UInt32& NumFramesWritten, UInt32 Channel)
{
	FAudioBuffer& BufferState = StreamBufferStates[Channel];
	while (NumFrames > 0)
	{
		if (BufferState.StreamReadCursor == 0)
		{
			const uint8* EncodedADPCMBlock = reinterpret_cast<uint8*>(Buffer->SampleData) + BufferState.SampleReadCursor;

			check(BufferState.StreamBuffer != NULL);
			ADPCM::DecodeBlock(EncodedADPCMBlock, Buffer->CompressedBlockSize, BufferState.StreamBuffer);
		}

		uint32 NumSamples = FMath::Min<uint32>(NumFrames, StreamBufferSize / sizeof(AudioSampleType)-BufferState.StreamReadCursor);
		for (uint32 SampleScan = 0; SampleScan < NumSamples; SampleScan++)
		{
			*OutData++ = BufferState.StreamBuffer[BufferState.StreamReadCursor++];
		}

		NumFramesWritten += NumSamples;
		NumFrames -= NumSamples;
		
		check(BufferState.StreamReadCursor * sizeof(AudioSampleType) <= StreamBufferSize);
		if (BufferState.StreamReadCursor * sizeof(AudioSampleType) == StreamBufferSize)
		{
			// Advance to the next compressed block
			BufferState.StreamReadCursor = 0;
			BufferState.SampleReadCursor += Buffer->CompressedBlockSize;

			check(BufferState.SampleReadCursor <= Buffer->BufferSize);
			const uint32 ChannelBufferSize = Buffer->BufferSize / Buffer->NumChannels;
			const uint32 SampleReadEnd = (Channel + 1) * ChannelBufferSize;

			if (BufferState.SampleReadCursor == SampleReadEnd)
			{
				BufferState.SampleReadCursor = Channel * ChannelBufferSize;
				switch (WaveInstance->LoopingMode)
				{
				case LOOP_Never:
					return noErr;

				case LOOP_WithNotification:
				case LOOP_Forever:
					BufferState.bLooped = true;
					break;
				}
			}
		}
	}

	return noErr;
}

OSStatus FIOSAudioSoundSource::IOSAudioRenderCallbackLPCM(AudioSampleType* OutData, UInt32 NumFrames, UInt32& NumFramesWritten, UInt32 Channel)
{
	FAudioBuffer& BufferState = StreamBufferStates[Channel];
	const uint32 ChannelStride = Buffer->NumChannels;
	const uint32 NumSamplesPerChannel = Buffer->BufferSize / (Buffer->NumChannels * sizeof(AudioSampleType));

	while (NumFrames > 0)
	{
		uint32 NumSamples = FMath::Min<uint32>(NumFrames, NumSamplesPerChannel - BufferState.SampleReadCursor);
		for (uint32 SampleScan = 0; SampleScan < NumSamples; SampleScan++)
		{
			*OutData++ = Buffer->SampleData[BufferState.SampleReadCursor++ * ChannelStride];
		}

		NumFramesWritten += NumSamples;
		NumFrames -= NumSamples;
		
		if (NumFrames > 0)
		{
			BufferState.SampleReadCursor = 0;
			
			switch (WaveInstance->LoopingMode)
			{
				case LOOP_Never:
					return noErr;
					
				case LOOP_WithNotification:
				case LOOP_Forever:
					BufferState.bLooped = true;
					break;
			}
		}
	}

	return noErr;
}

