// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*------------------------------------------------------------------------------------
	FSLESSoundSource.
------------------------------------------------------------------------------------*/

#include "AndroidAudioDevice.h"
#include "AudioDecompress.h"
#include "Engine.h"


// Callback that is registered if the source needs to loop
void OpenSLBufferQueueCallback( SLAndroidSimpleBufferQueueItf InQueueInterface, void* pContext ) 
{
	FSLESSoundSource* SoundSource = (FSLESSoundSource*)pContext;
	if( SoundSource )
	{
		SoundSource->OnRequeueBufferCallback( InQueueInterface );
	}
}

// Requeues buffer to loop Sound Source
void FSLESSoundSource::OnRequeueBufferCallback( SLAndroidSimpleBufferQueueItf InQueueInterface ) 
{
	if (!bStreamedSound)
	{
		SLresult result = (*SL_PlayerBufferQueue)->Enqueue(SL_PlayerBufferQueue, Buffer->AudioData, Buffer->GetSize() );
		if(result != SL_RESULT_SUCCESS) 
		{ 
			UE_LOG( LogAndroidAudio, Warning, TEXT("FAILED OPENSL BUFFER Enqueue SL_PlayerBufferQueue (Requeing)"));  
		}
		bHasLooped = true;
	}
	else
	{
		// Sound decoding is complete, just waiting to finish playing
		if (bBuffersToFlush)
		{
			// set the player's state to stopped
			SLresult result = (*SL_PlayerPlayInterface)->SetPlayState(SL_PlayerPlayInterface, SL_PLAYSTATE_STOPPED);
			check(SL_RESULT_SUCCESS == result);

			return;
		}

		// Enqueue the previously decoded buffer
		SLresult result = (*SL_PlayerBufferQueue)->Enqueue(SL_PlayerBufferQueue, AudioBuffers[BufferInUse].AudioData, AudioBuffers[BufferInUse].AudioDataSize );
		if(result != SL_RESULT_SUCCESS) 
		{ 
			UE_LOG( LogAndroidAudio, Warning, TEXT("FAILED OPENSL BUFFER Enqueue SL_PlayerBufferQueue (Requeing)"));  
		}

		// Switch to the next buffer and decode for the next time the callback fires
		BufferInUse = !BufferInUse;
		bHasLooped = ReadMorePCMData(BufferInUse);
	}
}

bool FSLESSoundSource::CreatePlayer()
{
	// data info
	SLDataLocator_AndroidSimpleBufferQueue LocationBuffer	= {		SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 1 };
		
	// PCM Info
	SLDataFormat_PCM PCM_Format				= {		SL_DATAFORMAT_PCM, SLuint32(Buffer->NumChannels), SLuint32( Buffer->SampleRate * 1000 ),	
		SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16, 
		Buffer->NumChannels == 2 ? ( SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT ) : SL_SPEAKER_FRONT_CENTER, 
		SL_BYTEORDER_LITTLEENDIAN };
		
	SLDataSource SoundDataSource			= {		&LocationBuffer, &PCM_Format};
		
	// configure audio sink
	SLDataLocator_OutputMix Output_Mix		= {		SL_DATALOCATOR_OUTPUTMIX, ((FSLESAudioDevice *)AudioDevice)->SL_OutputMixObject};
	SLDataSink AudioSink					= {		&Output_Mix, NULL};

	// create audio player
	const SLInterfaceID	ids[] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
	const SLboolean		req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
	SLresult result = (*Device->SL_EngineEngine)->CreateAudioPlayer( Device->SL_EngineEngine, &SL_PlayerObject, 
																	&SoundDataSource, &AudioSink, sizeof(ids) / sizeof(SLInterfaceID), ids, req );
	if(result != SL_RESULT_SUCCESS)
	{
		UE_LOG(LogAndroidAudio, Warning, TEXT("FAILED OPENSL BUFFER CreateAudioPlayer 0x%x"), result);
		return false;
	}
		
	bool bFailedSetup = false;
		
	// realize the player
	result = (*SL_PlayerObject)->Realize(SL_PlayerObject, SL_BOOLEAN_FALSE);
	if (result != SL_RESULT_SUCCESS) { UE_LOG(LogAndroidAudio, Warning, TEXT("FAILED OPENSL BUFFER Realize 0x%x"), result); return false; }
		
	// get the play interface
	result = (*SL_PlayerObject)->GetInterface(SL_PlayerObject, SL_IID_PLAY, &SL_PlayerPlayInterface);
	if (result != SL_RESULT_SUCCESS) { UE_LOG(LogAndroidAudio, Warning, TEXT("FAILED OPENSL BUFFER GetInterface SL_IID_PLAY 0x%x"), result); bFailedSetup |= true; }
	// volume
	result = (*SL_PlayerObject)->GetInterface(SL_PlayerObject, SL_IID_VOLUME, &SL_VolumeInterface);
	if (result != SL_RESULT_SUCCESS) { UE_LOG(LogAndroidAudio, Warning, TEXT("FAILED OPENSL BUFFER GetInterface SL_IID_VOLUME 0x%x"), result); bFailedSetup |= true; }
	// buffer system
	result = (*SL_PlayerObject)->GetInterface(SL_PlayerObject, SL_IID_BUFFERQUEUE, &SL_PlayerBufferQueue);
	if (result != SL_RESULT_SUCCESS) { UE_LOG(LogAndroidAudio, Warning, TEXT("FAILED OPENSL BUFFER GetInterface SL_IID_BUFFERQUEUE 0x%x"), result); bFailedSetup |= true; }
	
	return bFailedSetup == false;
}

void FSLESSoundSource::DestroyPlayer()
{
	if( SL_PlayerObject )
	{
		// close it down...
		(*SL_PlayerObject)->Destroy(SL_PlayerObject);			
		SL_PlayerObject			= NULL;
		SL_PlayerPlayInterface	= NULL;
		SL_PlayerBufferQueue	= NULL;
		SL_VolumeInterface		= NULL;
	}
}

bool FSLESSoundSource::EnqueuePCMBuffer( bool bLoop)
{
	SLresult result;
	// If looping, register a callback to requeue the buffer
	if( bLoop ) 
	{
		result = (*SL_PlayerBufferQueue)->RegisterCallback(SL_PlayerBufferQueue, OpenSLBufferQueueCallback, (void*)this);
		if (result != SL_RESULT_SUCCESS) { UE_LOG(LogAndroidAudio, Warning, TEXT("FAILED OPENSL BUFFER QUEUE RegisterCallback 0x%x "), result); return false; }
	}
	
	result = (*SL_PlayerBufferQueue)->Enqueue(SL_PlayerBufferQueue, Buffer->AudioData, Buffer->GetSize() );
	if (result != SL_RESULT_SUCCESS) { UE_LOG(LogAndroidAudio, Warning, TEXT("FAILED OPENSL BUFFER Enqueue SL_PlayerBufferQueue 0x%x params( %p, %d)"), result, Buffer->AudioData, int32(Buffer->GetSize())); return false; }

	bStreamedSound = false;
	bHasLooped = false;
	bBuffersToFlush = false;

	return true;
}

bool FSLESSoundSource::ReadProceduralData(const int32 BufferIndex)
{
	ensure(WaveInstance);
	const int32 MaxSamples = BufferSize / sizeof(int16);
	const int32 BytesWritten = WaveInstance->WaveData->GeneratePCMData(AudioBuffers[BufferIndex].AudioData, MaxSamples);
	
	AudioBuffers[BufferIndex].AudioDataSize = BytesWritten;

	// convenience return value: we're never actually "looping" here.
	return false;
}

bool FSLESSoundSource::ReadMorePCMData(const int32 BufferIndex)
{
	USoundWave *WaveData = WaveInstance->WaveData;
	if (WaveData && WaveData->bProcedural)
	{
		return ReadProceduralData(BufferIndex);
	}
	else
	{
		return Buffer->ReadCompressedData(AudioBuffers[BufferIndex].AudioData, WaveInstance->LoopingMode != LOOP_Never);
	}
}


bool FSLESSoundSource::EnqueuePCMRTBuffer( bool bLoop )
{
	if (AudioBuffers[0].AudioData || AudioBuffers[1].AudioData)
	{
		UE_LOG( LogAndroidAudio, Warning, TEXT("Enqueue PCMRT with buffers already allocated"));
	}
	FMemory::Memzero( AudioBuffers, sizeof( SLESAudioBuffer ) * 2 );

	// Set up double buffer area to decompress to
	BufferSize = Buffer->GetRTBufferSize() * Buffer->NumChannels;

	AudioBuffers[0].AudioData = (uint8*)FMemory::Malloc(BufferSize);
	AudioBuffers[0].AudioDataSize = BufferSize;

	AudioBuffers[1].AudioData = (uint8*)FMemory::Malloc(BufferSize);
	AudioBuffers[1].AudioDataSize = BufferSize;

	// Decompress two buffers worth of data to fill up the queue
	ReadMorePCMData(0);
	ReadMorePCMData(1);

	SLresult result;

	// callback is used to submit and decompress next buffer
	result = (*SL_PlayerBufferQueue)->RegisterCallback(SL_PlayerBufferQueue, OpenSLBufferQueueCallback, (void*)this);
	
	// queue one sound buffer, as that is all Android will accept
	if(result == SL_RESULT_SUCCESS) 
	{
		result = (*SL_PlayerBufferQueue)->Enqueue(SL_PlayerBufferQueue, AudioBuffers[0].AudioData, AudioBuffers[0].AudioDataSize );
		if (result != SL_RESULT_SUCCESS) { UE_LOG(LogAndroidAudio, Warning, TEXT("FAILED OPENSL BUFFER Enqueue SL_PlayerBufferQueue 0x%x params( %p, %d)"), result, Buffer->AudioData, int32(Buffer->GetSize())); return false; }
	}
	else
	{
		return false;
	}

	bStreamedSound = true;
	bHasLooped = false;
	bBuffersToFlush = false;
	BufferInUse = 1;
	return true;
}

/**
 * Initializes a source with a given wave instance and prepares it for playback.
 *
 * @param	WaveInstance	wave instance being primed for playback
 * @return	TRUE if initialization was successful, FALSE otherwise
 */
bool FSLESSoundSource::Init( FWaveInstance* InWaveInstance )
{
	// don't do anything if no volume! THIS APPEARS TO HAVE THE VOLUME IN TIME, CHECK HERE THOUGH IF ISSUES
	if( InWaveInstance && ( InWaveInstance->Volume * InWaveInstance->VolumeMultiplier ) <= 0 )
	{
		return false;
	}

	if (Buffer && Buffer->ResourceID == 0)
	{
		UE_LOG( LogAndroidAudio, Warning, TEXT(" InitSoundSouce with Buffer already allocated"));
		delete Buffer;
		Buffer = 0;
	}

	if (SL_PlayerObject)
	{
		UE_LOG( LogAndroidAudio, Warning, TEXT(" InitSoundSouce with PlayerObject not NULL, possible leak"));
	}
	
	// Find matching buffer.
	Buffer = FSLESSoundBuffer::Init( (FSLESAudioDevice *)AudioDevice, InWaveInstance->WaveData );

	if( Buffer && InWaveInstance->WaveData->NumChannels <= 2 && InWaveInstance->WaveData->SampleRate <= 48000 )
	{
		SCOPE_CYCLE_COUNTER( STAT_AudioSourceInitTime );
		
		bool bFailedSetup = false;

		if (CreatePlayer())
		{
			WaveInstance = InWaveInstance;

			switch( Buffer->Format)
			{
				case SoundFormat_PCM:
					bFailedSetup |= !EnqueuePCMBuffer( InWaveInstance->LoopingMode != LOOP_Never );
					break;
				case SoundFormat_PCMRT:
					bFailedSetup |= !EnqueuePCMRTBuffer( InWaveInstance->LoopingMode != LOOP_Never );
					break;
				default:
					bFailedSetup = true;
			}
		}
		else
		{
			bFailedSetup = true;
		}
		
		// clean up the madness if anything we need failed
		if( bFailedSetup )
		{
			UE_LOG( LogAndroidAudio, Warning, TEXT(" Setup failed %s"), *InWaveInstance->WaveData->GetName());
			DestroyPlayer();
			return false;
		}
		
		Update();
		
		// Initialization was successful.
		return true;
	}
	else
	{
		// Failed to initialize source.
		// These occurences appear to potentially lead to leaks
		UE_LOG( LogAndroidAudio, Warning, TEXT("Init SoundSource failed on %s"), *InWaveInstance->WaveData->GetName());
		UE_LOG( LogAndroidAudio, Warning, TEXT("  SampleRate %d"), InWaveInstance->WaveData->SampleRate);
		UE_LOG( LogAndroidAudio, Warning, TEXT("  Channels %d"), InWaveInstance->WaveData->NumChannels);

		if (Buffer && Buffer->ResourceID == 0)
		{
			delete Buffer;
			Buffer = 0;
		} 
	}
	return false;
}


FSLESSoundSource::FSLESSoundSource( class FAudioDevice* InAudioDevice )
	:	FSoundSource( InAudioDevice ),
		Device((FSLESAudioDevice *)InAudioDevice),
		Buffer( NULL ),
		bStreamedSound(false),
		bBuffersToFlush(false),
		BufferSize(0),
		BufferInUse(0),
		bHasLooped(false),
		SL_PlayerObject(NULL),
		SL_PlayerPlayInterface(NULL),
		SL_PlayerBufferQueue(NULL),
		SL_VolumeInterface(NULL)
{
	FMemory::Memzero( AudioBuffers, sizeof( AudioBuffers ) );
}

/**
 * Clean up any hardware referenced by the sound source
 */
FSLESSoundSource::~FSLESSoundSource( void )
{

	DestroyPlayer();

	ReleaseResources();
}

void FSLESSoundSource::ReleaseResources()
{
	FMemory::Free( AudioBuffers[0].AudioData);
	FMemory::Free( AudioBuffers[1].AudioData);

	FMemory::Memzero( AudioBuffers, sizeof( AudioBuffers ) );

	if (Buffer && Buffer->ResourceID == 0)
	{
		delete Buffer;
	}
	Buffer = NULL;
}

/**
 * Updates the source specific parameter like e.g. volume and pitch based on the associated
 * wave instance.	
 */
void FSLESSoundSource::Update( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioUpdateSources );
	
	if( !WaveInstance || Paused )
	{
		return;
	}
	
	float Volume = WaveInstance->Volume * WaveInstance->VolumeMultiplier;
	if (SetStereoBleed())
	{
		// Emulate the bleed to rear speakers followed by stereo fold down
		Volume *= 1.25f;
	}
	Volume *= FApp::GetVolumeMultiplier();
	Volume = FMath::Clamp(Volume, 0.0f, MAX_VOLUME);
	
	const float Pitch = FMath::Clamp<float>(WaveInstance->Pitch, MIN_PITCH, MAX_PITCH);

	// Set whether to apply reverb
	SetReverbApplied(true);

	// Set the HighFrequencyGain value
	SetHighFrequencyGain();
	
	FVector Location;
	FVector	Velocity;
	
	// See file header for coordinate system explanation.
	Location.X = WaveInstance->Location.X;
	Location.Y = WaveInstance->Location.Z; // Z/Y swapped to match UE coordinate system
	Location.Z = WaveInstance->Location.Y; // Z/Y swapped to match UE coordinate system
	
	Velocity.X = WaveInstance->Velocity.X;
	Velocity.Y = WaveInstance->Velocity.Z; // Z/Y swapped to match UE coordinate system
	Velocity.Z = WaveInstance->Velocity.Y; // Z/Y swapped to match UE coordinate system
	
	// We're using a relative coordinate system for un- spatialized sounds.
	if( !WaveInstance->bUseSpatialization )
	{
		Location = FVector( 0.f, 0.f, 0.f );
	}
	
	// Set volume (Pitch changes are not supported on current Android platforms!)
	// also Location & Velocity
	
	// Convert volume to millibels.
	SLmillibel MaxMillibel = 0;
	SLmillibel MinMillibel = -3000;
	(*SL_VolumeInterface)->GetMaxVolumeLevel( SL_VolumeInterface, &MaxMillibel );
	SLmillibel VolumeMillibel = (Volume * (MaxMillibel - MinMillibel)) + MinMillibel;
	VolumeMillibel = FMath::Clamp(VolumeMillibel, MinMillibel, MaxMillibel);
	
	SLresult result = (*SL_VolumeInterface)->SetVolumeLevel(SL_VolumeInterface, VolumeMillibel);
	check(SL_RESULT_SUCCESS == result);

}

/**
 * Plays the current wave instance.	
 */
void FSLESSoundSource::Play( void )
{
	if( WaveInstance )
	{
		
		// set the player's state to playing
		SLresult result = (*SL_PlayerPlayInterface)->SetPlayState(SL_PlayerPlayInterface, SL_PLAYSTATE_PLAYING);
		check(SL_RESULT_SUCCESS == result);

		Paused = false;
		Playing = true;
	}
}

/**
 * Stops the current wave instance and detaches it from the source.	
 */
void FSLESSoundSource::Stop( void )
{
	if( WaveInstance )
	{
		// set the player's state to stopped
		SLresult result = (*SL_PlayerPlayInterface)->SetPlayState(SL_PlayerPlayInterface, SL_PLAYSTATE_STOPPED);
		check(SL_RESULT_SUCCESS == result);
		
		// Unregister looping callback
		if( WaveInstance->LoopingMode != LOOP_Never ) 
		{
			result = (*SL_PlayerBufferQueue)->RegisterCallback(SL_PlayerBufferQueue, NULL, NULL);
		}
		
		DestroyPlayer();
		ReleaseResources();
		
		Paused = false;
		Playing = false;
		Buffer = NULL;
	}
	
	FSoundSource::Stop();
}

/**
 * Pauses playback of current wave instance.
 */
void FSLESSoundSource::Pause( void )
{
	if( WaveInstance )
	{
		Paused = true;
		
		// set the player's state to paused
		SLresult result = (*SL_PlayerPlayInterface)->SetPlayState(SL_PlayerPlayInterface, SL_PLAYSTATE_PAUSED);
		check(SL_RESULT_SUCCESS == result);
		
	}
}

/** 
 * Returns TRUE if the source has finished playing
 */
bool FSLESSoundSource::IsSourceFinished( void )
{
	SLuint32 PlayState;
	
	// set the player's state to playing
	SLresult result = (*SL_PlayerPlayInterface)->GetPlayState(SL_PlayerPlayInterface, &PlayState);
	check(SL_RESULT_SUCCESS == result);
	
	if( PlayState == SL_PLAYSTATE_STOPPED )
	{
		return true;
	}

	return false;
}

/**
 * Queries the status of the currently associated wave instance.
 *
 * @return	TRUE if the wave instance/ source has finished playback and FALSE if it is 
 *			currently playing or paused.
 */
bool FSLESSoundSource::IsFinished( void )
{
	if( WaveInstance )
	{
		// Check for a non starved, stopped source
		if( IsSourceFinished() )
		{
			// Notify the wave instance that it has finished playing
			WaveInstance->NotifyFinished();
			return true;
		}
		else 
		{
			if (bHasLooped)
			{
				switch (WaveInstance->LoopingMode)
				{
					case LOOP_Forever:
						bHasLooped = false;
						break;

					case LOOP_Never:
						bBuffersToFlush = true;
						break;

					case LOOP_WithNotification:
						bHasLooped = false;
						// Notify the wave instance that it has finished playing.
						WaveInstance->NotifyFinished();
						break;
				};
			}
		}
		
		return false;
	}
	return true;
}
