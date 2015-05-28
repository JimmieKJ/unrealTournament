// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	XeAudioDevice.cpp: Unreal XAudio2 Audio interface object.

	Unreal is RHS with Y and Z swapped (or technically LHS with flipped axis)

=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "XAudio2Device.h"
#include "XAudio2Effects.h"
#include "Engine.h"
#include "XAudio2Support.h"
#include "IAudioExtensionPlugin.h"

/*------------------------------------------------------------------------------------
	For muting user soundtracks during cinematics
------------------------------------------------------------------------------------*/
FXMPHelper XMPHelper;
FXMPHelper* FXMPHelper::GetXMPHelper( void ) 
{ 
	return( &XMPHelper ); 
}

/*------------------------------------------------------------------------------------
	FXAudio2SoundSource.
------------------------------------------------------------------------------------*/

/**
 * Simple constructor
 */
FXAudio2SoundSource::FXAudio2SoundSource(FAudioDevice* InAudioDevice)
:	FSoundSource( InAudioDevice ),
	Source( NULL ),
	CurrentBuffer( 0 ),
	bBuffersToFlush( false ),
	bLoopCallback( false ),
	bResourcesNeedFreeing(false),
	VoiceId(-1),
	bUsingDefaultSpatialization(true)
{
	AudioDevice = ( FXAudio2Device* )InAudioDevice;
	check( AudioDevice );
	Effects = (FXAudio2EffectsManager*)AudioDevice->Effects;
	check( Effects );

	Destinations[DEST_DRY].Flags = 0;
	Destinations[DEST_DRY].pOutputVoice = NULL;
	Destinations[DEST_REVERB].Flags = 0;
	Destinations[DEST_REVERB].pOutputVoice = NULL;
	Destinations[DEST_RADIO].Flags = 0;
	Destinations[DEST_RADIO].pOutputVoice = NULL;

	FMemory::Memzero( XAudio2Buffers, sizeof( XAudio2Buffers ) );
	FMemory::Memzero( XAudio2BufferXWMA, sizeof( XAudio2BufferXWMA ) );
}

/**
 * Destructor, cleaning up voice
 */
FXAudio2SoundSource::~FXAudio2SoundSource( void )
{
	FreeResources();
}

void FXAudio2SoundSource::InitializeSourceEffects(uint32 InVoiceId)
{
	VoiceId = InVoiceId;
	if (AudioDevice->SpatializeProcessor != nullptr)
	{
		AudioDevice->SpatializeProcessor->CreateSpatializationEffect(VoiceId);
	}
}

/**
 * Free up any allocated resources
 */
void FXAudio2SoundSource::FreeResources( void )
{
	// Release voice.
	if( Source )
	{
		Source->DestroyVoice();
		Source = NULL;
	}

	// If we're a streaming buffer...
	if( bResourcesNeedFreeing )
	{
		// ... free the buffers
		FMemory::Free( ( void* )XAudio2Buffers[0].pAudioData );
		FMemory::Free( ( void* )XAudio2Buffers[1].pAudioData );

		// Buffers without a valid resource ID are transient and need to be deleted.
		if( Buffer )
		{
			check( Buffer->ResourceID == 0 );
			delete Buffer;
			Buffer = NULL;
		}

		CurrentBuffer = 0;
	}
}

/** 
 * Submit the relevant audio buffers to the system
 */
void FXAudio2SoundSource::SubmitPCMBuffers( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioSubmitBuffersTime );

	FMemory::Memzero( XAudio2Buffers, sizeof( XAUDIO2_BUFFER ) );

	CurrentBuffer = 0;

	XAudio2Buffers[0].pAudioData = XAudio2Buffer->PCM.PCMData;
	XAudio2Buffers[0].AudioBytes = XAudio2Buffer->PCM.PCMDataSize;
	XAudio2Buffers[0].pContext = this;

	if( WaveInstance->LoopingMode == LOOP_Never )
	{
		XAudio2Buffers[0].Flags = XAUDIO2_END_OF_STREAM;

		AudioDevice->ValidateAPICall( TEXT( "SubmitSourceBuffer - PCM - LOOP_Never" ), 
			Source->SubmitSourceBuffer( XAudio2Buffers ) );
	}
	else
	{
		XAudio2Buffers[0].LoopCount = XAUDIO2_LOOP_INFINITE;

		AudioDevice->ValidateAPICall( TEXT( "SubmitSourceBuffer - PCM - LOOP_*" ), 
			Source->SubmitSourceBuffer( XAudio2Buffers ) );
	}
}


bool FXAudio2SoundSource::ReadProceduralData( const int32 BufferIndex )
{
	const int32 MaxSamples = ( MONO_PCM_BUFFER_SIZE * Buffer->NumChannels ) / sizeof( int16 );
	const int32 BytesWritten = WaveInstance->WaveData->GeneratePCMData( (uint8*)XAudio2Buffers[BufferIndex].pAudioData, MaxSamples );

	XAudio2Buffers[BufferIndex].AudioBytes = BytesWritten;

	// convenience return value: we're never actually "looping" here.
	return false;  
}

bool FXAudio2SoundSource::ReadMorePCMData( const int32 BufferIndex )
{
	USoundWave *WaveData = WaveInstance->WaveData;
	if( WaveData && WaveData->bProcedural )
	{
		return ReadProceduralData( BufferIndex );
	}
	else
	{
		return XAudio2Buffer->ReadCompressedData( ( uint8* )XAudio2Buffers[BufferIndex].pAudioData, WaveInstance->LoopingMode != LOOP_Never );
	}
}

/** 
 * Submit the relevant audio buffers to the system
 */
void FXAudio2SoundSource::SubmitPCMRTBuffers( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioSubmitBuffersTime );

	FMemory::Memzero( XAudio2Buffers, sizeof( XAUDIO2_BUFFER ) * 2 );

	// Set the buffer to be in real time mode
	CurrentBuffer = 0;

	// Set up double buffer area to decompress to
	XAudio2Buffers[0].pAudioData = ( uint8* )FMemory::Malloc( MONO_PCM_BUFFER_SIZE * Buffer->NumChannels );
	XAudio2Buffers[0].AudioBytes = MONO_PCM_BUFFER_SIZE * Buffer->NumChannels;

	ReadMorePCMData(0);

	AudioDevice->ValidateAPICall( TEXT( "SubmitSourceBuffer - PCMRT" ), 
		Source->SubmitSourceBuffer( XAudio2Buffers ) );

	XAudio2Buffers[1].pAudioData = ( uint8* )FMemory::Malloc( MONO_PCM_BUFFER_SIZE * Buffer->NumChannels );
	XAudio2Buffers[1].AudioBytes = MONO_PCM_BUFFER_SIZE * Buffer->NumChannels;

	ReadMorePCMData(1);

	if (XAudio2Buffers[1].AudioBytes > 0)
	{
		CurrentBuffer = 1;

		AudioDevice->ValidateAPICall( TEXT( "SubmitSourceBuffer - PCMRT" ), 
			Source->SubmitSourceBuffer( XAudio2Buffers + 1 ) );
	}

	bResourcesNeedFreeing = true;
}

/** 
 * Submit the relevant audio buffers to the system, accounting for looping modes
 */
void FXAudio2SoundSource::SubmitXMA2Buffers( void )
{
#if XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
	SCOPE_CYCLE_COUNTER( STAT_AudioSubmitBuffersTime );

	FMemory::Memzero( XAudio2Buffers, sizeof( XAUDIO2_BUFFER ) );

	CurrentBuffer = 0;

	XAudio2Buffers[0].pAudioData = XAudio2Buffer->XMA2.XMA2Data;
	XAudio2Buffers[0].AudioBytes = XAudio2Buffer->XMA2.XMA2DataSize;
	XAudio2Buffers[0].pContext = this;

	// Deal with looping behavior.
	if( WaveInstance->LoopingMode == LOOP_Never )
	{
		// Regular sound source, don't loop.
		XAudio2Buffers[0].Flags = XAUDIO2_END_OF_STREAM;

		AudioDevice->ValidateAPICall( TEXT( "SubmitSourceBuffer - XMA2 - LOOP_Never" ), 
			Source->SubmitSourceBuffer( XAudio2Buffers ) );
	}
	else
	{
		// Set to reserved "infinite" value.
		XAudio2Buffers[0].LoopCount = 255;
		XAudio2Buffers[0].LoopBegin = XAudio2Buffer->XMA2.XMA2Format.LoopBegin;
		XAudio2Buffers[0].LoopLength = XAudio2Buffer->XMA2.XMA2Format.LoopLength;

		AudioDevice->ValidateAPICall( TEXT( "SubmitSourceBuffer - XMA2 - LOOP_*" ), 
			Source->SubmitSourceBuffer( XAudio2Buffers ) );
	}
#else	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
	checkf(0, TEXT("SubmitXMA2Buffers on a platform that does not support XMA2!"));
#endif	//XAUDIO_SUPPORTS_XMA2WAVEFORMATEX
}

/** 
 * Submit the relevant audio buffers to the system
 */
void FXAudio2SoundSource::SubmitXWMABuffers( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioSubmitBuffersTime );

	FMemory::Memzero( XAudio2Buffers, sizeof( XAUDIO2_BUFFER ) );
	FMemory::Memzero( XAudio2BufferXWMA, sizeof( XAUDIO2_BUFFER_WMA ) );

	CurrentBuffer = 0;

	// Regular sound source, don't loop.
	XAudio2Buffers[0].pAudioData = XAudio2Buffer->XWMA.XWMAData;
	XAudio2Buffers[0].AudioBytes = XAudio2Buffer->XWMA.XWMADataSize;
	XAudio2Buffers[0].pContext = this;

	XAudio2BufferXWMA[0].pDecodedPacketCumulativeBytes = XAudio2Buffer->XWMA.XWMASeekData;
	XAudio2BufferXWMA[0].PacketCount = XAudio2Buffer->XWMA.XWMASeekDataSize / sizeof( UINT32 );

	if( WaveInstance->LoopingMode == LOOP_Never )
	{
		XAudio2Buffers[0].Flags = XAUDIO2_END_OF_STREAM;

		AudioDevice->ValidateAPICall( TEXT( "SubmitSourceBuffer - XWMA - LOOP_Never" ), 
			Source->SubmitSourceBuffer( XAudio2Buffers, XAudio2BufferXWMA ) );
	}
	else
	{
		XAudio2Buffers[0].LoopCount = 255;
		XAudio2Buffers[0].Flags = XAUDIO2_END_OF_STREAM;

		AudioDevice->ValidateAPICall( TEXT( "SubmitSourceBuffer - XWMA - LOOP_*" ), 
			Source->SubmitSourceBuffer( XAudio2Buffers, XAudio2BufferXWMA ) );
	}
}

/**
 * Create a new source voice
 */
bool FXAudio2SoundSource::CreateSource( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioSourceCreateTime );

	int32 NumSends = 0;

#if XAUDIO2_SUPPORTS_SENDLIST
	// Create a source that goes to the spatialisation code and reverb effect
	Destinations[NumSends].pOutputVoice = Effects->DryPremasterVoice;
	if( IsEQFilterApplied() )
	{
		Destinations[NumSends].pOutputVoice = Effects->EQPremasterVoice;
	}
	NumSends++;

	if( bReverbApplied )
	{
		Destinations[NumSends].pOutputVoice = Effects->ReverbEffectVoice;
		NumSends++;
	}

	if( WaveInstance->bApplyRadioFilter )
	{
		Destinations[NumSends].pOutputVoice = Effects->RadioEffectVoice;
		NumSends++;
	}

	const XAUDIO2_VOICE_SENDS SourceSendList =
	{
		NumSends, Destinations
	};
#endif	//XAUDIO2_SUPPORTS_SENDLIST

	// Mark the source as music if it is a member of the music group and allow low, band and high pass filters
	UINT32 Flags = 
#if XAUDIO2_SUPPORTS_MUSIC
		( WaveInstance->bIsMusic ? XAUDIO2_VOICE_MUSIC : 0 );
#else	//XAUDIO2_SUPPORTS_MUSIC
		0;
#endif	//XAUDIO2_SUPPORTS_MUSIC
	Flags |= XAUDIO2_VOICE_USEFILTER;

	// Reset the bUsingSpatializationEffect flag
	bUsingDefaultSpatialization = true;
	bool bCreatedWithSpatializationEffect = false;

	if (CreateWithSpatializationEffect())
	{
		check(AudioDevice->SpatializeProcessor != nullptr);
		IUnknown* Effect = (IUnknown*)AudioDevice->SpatializeProcessor->GetSpatializationEffect(VoiceId);
		if (Effect)
		{
			// Indicate that this source is currently using the 3d spatialization effect. We can't stop using it
			// for the lifetime of this sound, so if this if the spatialization effect is toggled off, we're still 
			// going to hear the sound for the duration of this sound.
			bUsingDefaultSpatialization = false;

			XAUDIO2_EFFECT_DESCRIPTOR EffectDescriptor[1];
			EffectDescriptor[0].pEffect = Effect;
			EffectDescriptor[0].OutputChannels = 2;
			EffectDescriptor[0].InitialState = true;

			const XAUDIO2_EFFECT_CHAIN EffectChain = { 1, EffectDescriptor };

			// All sound formats start with the WAVEFORMATEX structure (PCM, XMA2, XWMA)
			if (!AudioDevice->ValidateAPICall(TEXT("CreateSourceVoice (mono source)"),
#if XAUDIO2_SUPPORTS_SENDLIST
				AudioDevice->DeviceProperties->XAudio2->CreateSourceVoice(&Source, (WAVEFORMATEX*)&XAudio2Buffer->PCM, Flags, MAX_PITCH, &SourceCallback, &SourceSendList, &EffectChain)))
#else	//XAUDIO2_SUPPORTS_SENDLIST
				AudioDevice->DeviceProperties->XAudio2->CreateSourceVoice(&Source, (WAVEFORMATEX*)&XAudio2Buffer->PCM, Flags, MAX_PITCH, &SourceCallback, nullptr, &EffectChain)))
#endif	//XAUDIO2_SUPPORTS_SENDLIST
			{
				return(false);
			}

			// We succeeded, then return
			bCreatedWithSpatializationEffect = true;
		}
	}

	if (!bCreatedWithSpatializationEffect)
	{
		check(AudioDevice->DeviceProperties != nullptr);
		check(AudioDevice->DeviceProperties->XAudio2 != nullptr);
		// All sound formats start with the WAVEFORMATEX structure (PCM, XMA2, XWMA)
		if (!AudioDevice->ValidateAPICall(TEXT("CreateSourceVoice (source)"),
#if XAUDIO2_SUPPORTS_SENDLIST
			AudioDevice->DeviceProperties->XAudio2->CreateSourceVoice(&Source, (WAVEFORMATEX*)&XAudio2Buffer->PCM, Flags, MAX_PITCH, &SourceCallback, &SourceSendList, nullptr)))
#else	//XAUDIO2_SUPPORTS_SENDLIST
			AudioDevice->DeviceProperties->XAudio2->CreateSourceVoice(&Source, (WAVEFORMATEX*)&XAudio2Buffer->PCM, Flags, MAX_PITCH, &SourceCallback)))
#endif	//XAUDIO2_SUPPORTS_SENDLIST
		{
			return false;
		}
	}

	return true;
}

/**
 * Initializes a source with a given wave instance and prepares it for playback.
 *
 * @param	WaveInstance	wave instace being primed for playback
 * @return	true if initialization was successful, false otherwise
 */
bool FXAudio2SoundSource::Init(FWaveInstance* InWaveInstance)
{
	if (InWaveInstance->OutputTarget != EAudioOutputTarget::Controller)
	{
		// Find matching buffer.
		FAudioDevice* AudioDevice = nullptr;
		if (InWaveInstance->ActiveSound->AudioComponent.IsValid())
		{
			AudioDevice = InWaveInstance->ActiveSound->AudioComponent->GetAudioDevice();
		}
		else
		{
			AudioDevice = GEngine->GetMainAudioDevice();
		}
		check(AudioDevice);

		XAudio2Buffer = FXAudio2SoundBuffer::Init(AudioDevice, InWaveInstance->WaveData, InWaveInstance->StartTime > 0.f);
		Buffer = XAudio2Buffer;

		// Buffer failed to be created, or there was an error with the compressed data
		if (Buffer && Buffer->NumChannels > 0)
		{
			SCOPE_CYCLE_COUNTER( STAT_AudioSourceInitTime );

			WaveInstance = InWaveInstance;

			// Set whether to apply reverb
			SetReverbApplied(Effects->ReverbEffectVoice != nullptr);

			// Create a new source
			if (!CreateSource())
			{
				return false;
			}

			if (WaveInstance->StartTime > 0.f)
			{
				XAudio2Buffer->Seek(WaveInstance->StartTime);
			}

			// Submit audio buffers
			switch (XAudio2Buffer->SoundFormat)
			{
			case SoundFormat_PCM:
			case SoundFormat_PCMPreview:
				SubmitPCMBuffers();
				break;

			case SoundFormat_PCMRT:
			case SoundFormat_Streaming:
				SubmitPCMRTBuffers();
				break;

			case SoundFormat_XMA2:
				SubmitXMA2Buffers();
				break;

			case SoundFormat_XWMA:
				SubmitXWMABuffers();
				break;
			}

			// Updates the source which e.g. sets the pitch and volume.
			Update();
		
			// Initialization succeeded.
			return true;
		}
	}

	// Initialization failed.
	return false;
}

/**
 * Calculates the volume for each channel
 */
void FXAudio2SoundSource::GetChannelVolumes( float ChannelVolumes[CHANNELOUT_COUNT], float AttenuatedVolume )
{
	if (FApp::GetVolumeMultiplier() == 0.0f || AudioDevice->bIsDeviceMuted)
	{
		for( int32 i = 0; i < CHANNELOUT_COUNT; i++ )
		{
			ChannelVolumes[i] = 0;
		}
		return;
	}

	switch (Buffer->NumChannels)
	{
		case 1:
			GetMonoChannelVolumes(ChannelVolumes, AttenuatedVolume);
			break;

		case 2:
			GetStereoChannelVolumes(ChannelVolumes, AttenuatedVolume);
			break;

		case 4:
			GetQuadChannelVolumes(ChannelVolumes, AttenuatedVolume);
			break;

		case 6:
			GetHexChannelVolumes(ChannelVolumes, AttenuatedVolume);
			break;

		default:
			break;
	};

	// Apply any debug settings
	switch (AudioDevice->GetMixDebugState())
	{
	case DEBUGSTATE_IsolateReverb:
		for( int32 i = 0; i < SPEAKER_COUNT; i++ )
		{
			ChannelVolumes[i] = 0.0f;
		}
		break;

	case DEBUGSTATE_IsolateDryAudio:
		ChannelVolumes[CHANNELOUT_REVERB] = 0.0f;
		ChannelVolumes[CHANNELOUT_RADIO] = 0.0f;
		break;
	};

	for (int32 i = 0; i < CHANNELOUT_COUNT; i++)
	{
		// Detect and warn about NaN and INF volumes. XAudio does not do this internally and behavior is undefined.
		// This is known to happen in X3DAudioCalculate in channel 0 and the cause is unknown.
		if (!FMath::IsFinite(ChannelVolumes[i]))
		{
			const FString NaNorINF = FMath::IsNaN(ChannelVolumes[i]) ? TEXT("NaN") : TEXT("INF");
			UE_LOG(LogXAudio2, Warning, TEXT("FXAudio2SoundSource contains %s in channel %d: %s"), *NaNorINF, i, *Describe_Internal(true, false));
			ChannelVolumes[i] = 0;
		}
		// Detect and warn about unreasonable volumes. These are clamped anyway, but are good to know about.
		else if (ChannelVolumes[i] > FLT_MAX / 2.f || ChannelVolumes[i] < -FLT_MAX / 2.f)
		{
			UE_LOG(LogXAudio2, Warning, TEXT("FXAudio2SoundSource contains unreasonble value %f in channel %d: %s"), ChannelVolumes[i], i, *Describe_Internal(true, false));
		}

		ChannelVolumes[i] = FMath::Clamp<float>(ChannelVolumes[i] * FApp::GetVolumeMultiplier(), 0.0f, MAX_VOLUME);
	}
}

void FXAudio2SoundSource::GetMonoChannelVolumes(float ChannelVolumes[CHANNELOUT_COUNT], float AttenuatedVolume)
{
	if (IsUsingDefaultSpatializer())
	{
		// Calculate direction from listener to sound, where the sound is at the origin if unspatialised.
		FVector Direction = FVector::ZeroVector;
		float NormalizedOmniRadius = 0;
		if (WaveInstance->bUseSpatialization)
		{
			FVector UnnormalizedDirection = AudioDevice->InverseTransform.TransformPosition(WaveInstance->Location);
			Direction = UnnormalizedDirection.GetSafeNormal();
			float Distance = UnnormalizedDirection.Size();
			NormalizedOmniRadius = (Distance > 0) ? (WaveInstance->OmniRadius / Distance) : FLT_MAX;
		}

		// Calculate 5.1 channel volume
		FVector OrientFront;
		OrientFront.X = 0.0f;
		OrientFront.Y = 0.0f;
		OrientFront.Z = 1.0f;

		FVector ListenerPosition;
		ListenerPosition.X = 0.0f;
		ListenerPosition.Y = 0.0f;
		ListenerPosition.Z = 0.0f;

		FVector EmitterPosition;
		EmitterPosition.X = Direction.Y;
		EmitterPosition.Y = Direction.X;
		EmitterPosition.Z = -Direction.Z;

		// Calculate 5.1 channel dolby surround rate/multipliers.
		ChannelVolumes[CHANNELOUT_FRONTLEFT] = AttenuatedVolume;
		ChannelVolumes[CHANNELOUT_FRONTRIGHT] = AttenuatedVolume;
		ChannelVolumes[CHANNELOUT_FRONTCENTER] = AttenuatedVolume;
		ChannelVolumes[CHANNELOUT_LEFTSURROUND] = AttenuatedVolume;
		ChannelVolumes[CHANNELOUT_RIGHTSURROUND] = AttenuatedVolume;

		if (bReverbApplied)
		{
			ChannelVolumes[CHANNELOUT_REVERB] = AttenuatedVolume;
		}

		ChannelVolumes[CHANNELOUT_RADIO] = 0.0f;

		// Call the spatialisation magic
		AudioDevice->SpatializationHelper.CalculateDolbySurroundRate(OrientFront, ListenerPosition, EmitterPosition, NormalizedOmniRadius, ChannelVolumes);

		// Handle any special post volume processing
		if (WaveInstance->bApplyRadioFilter)
		{
			// If radio filter applied, output on radio channel only (no reverb)
			FMemory::Memzero(ChannelVolumes, CHANNELOUT_COUNT * sizeof(float));

			ChannelVolumes[CHANNELOUT_RADIO] = WaveInstance->RadioFilterVolume;
		}
		else if (WaveInstance->bCenterChannelOnly)
		{
			// If center channel only applied, output on center channel only (no reverb)
			FMemory::Memzero(ChannelVolumes, CHANNELOUT_COUNT * sizeof(float));

			ChannelVolumes[CHANNELOUT_FRONTCENTER] = WaveInstance->VoiceCenterChannelVolume * AttenuatedVolume;
		}
		else
		{
			if (FXAudioDeviceProperties::NumSpeakers == 6)
			{
				ChannelVolumes[CHANNELOUT_LOWFREQUENCY] = AttenuatedVolume * LFEBleed;

				// Smooth out the left and right channels with the center channel
				if (ChannelVolumes[CHANNELOUT_FRONTCENTER] > ChannelVolumes[CHANNELOUT_FRONTLEFT])
				{
					ChannelVolumes[CHANNELOUT_FRONTLEFT] = (ChannelVolumes[CHANNELOUT_FRONTCENTER] + ChannelVolumes[CHANNELOUT_FRONTLEFT]) / 2;
				}

				if (ChannelVolumes[CHANNELOUT_FRONTCENTER] > ChannelVolumes[CHANNELOUT_FRONTRIGHT])
				{
					ChannelVolumes[CHANNELOUT_FRONTRIGHT] = (ChannelVolumes[CHANNELOUT_FRONTCENTER] + ChannelVolumes[CHANNELOUT_FRONTRIGHT]) / 2;
				}
			}

			// Weight some of the sound to the center channel
			ChannelVolumes[CHANNELOUT_FRONTCENTER] = FMath::Max(ChannelVolumes[CHANNELOUT_FRONTCENTER], WaveInstance->VoiceCenterChannelVolume * AttenuatedVolume);
		}
	}
	else if (WaveInstance->bUseSpatialization)
	{
		check(WaveInstance->SpatializationAlgorithm == SPATIALIZATION_HRTF);
		check(AudioDevice->SpatializeProcessor != nullptr);

		// If we're using an HRTF spatialization algorithm, we need to find the position of the emitter and set it as a parameter
		FVector EmitterPosition = FVector::ZeroVector;
		FVector UnnormalizedDirection = AudioDevice->InverseTransform.TransformPosition(WaveInstance->Location);
		EmitterPosition = UnnormalizedDirection.GetSafeNormal();

		AudioDevice->SpatializeProcessor->SetSpatializationParameters(VoiceId, EmitterPosition, (ESpatializationEffectType)WaveInstance->SpatializationAlgorithm);
		GetStereoChannelVolumes(ChannelVolumes, AttenuatedVolume);
	}
}

void FXAudio2SoundSource::GetStereoChannelVolumes(float ChannelVolumes[CHANNELOUT_COUNT], float AttenuatedVolume)
{
	// Stereo is always treated as unspatialized (except when the oculus stereo effect is being used)
	ChannelVolumes[CHANNELOUT_FRONTLEFT] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_FRONTRIGHT] = AttenuatedVolume;

	// Potentially bleed to the rear speakers from 2.0 channel to simulated 4.0 channel
	if (IsUsingDefaultSpatializer() && FXAudioDeviceProperties::NumSpeakers == 6)
	{
		ChannelVolumes[CHANNELOUT_LEFTSURROUND] = AttenuatedVolume * StereoBleed;
		ChannelVolumes[CHANNELOUT_RIGHTSURROUND] = AttenuatedVolume * StereoBleed;

		ChannelVolumes[CHANNELOUT_LOWFREQUENCY] = AttenuatedVolume * LFEBleed * 0.5f;
	}

	if (bReverbApplied)
	{
		ChannelVolumes[CHANNELOUT_REVERB] = AttenuatedVolume;
	}

	// Handle radio distortion if the sound can handle it. 
	ChannelVolumes[CHANNELOUT_RADIO] = 0.0f;
	if (WaveInstance->bApplyRadioFilter)
	{
		ChannelVolumes[CHANNELOUT_RADIO] = WaveInstance->RadioFilterVolume;
	}
}

void FXAudio2SoundSource::GetQuadChannelVolumes(float ChannelVolumes[CHANNELOUT_COUNT], float AttenuatedVolume)
{
	ChannelVolumes[CHANNELOUT_FRONTLEFT] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_FRONTRIGHT] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_LEFTSURROUND] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_RIGHTSURROUND] = AttenuatedVolume;

	if (FXAudioDeviceProperties::NumSpeakers == 6)
	{
		ChannelVolumes[CHANNELOUT_LOWFREQUENCY] = AttenuatedVolume * LFEBleed * 0.25f;
	}
}

void FXAudio2SoundSource::GetHexChannelVolumes(float ChannelVolumes[CHANNELOUT_COUNT], float AttenuatedVolume)
{
	ChannelVolumes[CHANNELOUT_FRONTLEFT] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_FRONTRIGHT] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_FRONTCENTER] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_LOWFREQUENCY] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_LEFTSURROUND] = AttenuatedVolume;
	ChannelVolumes[CHANNELOUT_RIGHTSURROUND] = AttenuatedVolume;
}

/** 
 * Maps a sound with a given number of channels to to expected speakers
 */
void FXAudio2SoundSource::RouteDryToSpeakers( float ChannelVolumes[CHANNELOUT_COUNT] )
{
	// Only need to account to the special cases that are not a simple match of channel to speaker
	switch( Buffer->NumChannels )
	{
		case 1:
		RouteMonoToDry(ChannelVolumes);
		break;

		case 2:
		RouteStereoToDry(ChannelVolumes);
		break;

		case 4:
		RouteQuadToDry(ChannelVolumes);
		break;

		case 6:
		RouteHexToDry(ChannelVolumes);
		break;

		default:
			break;
	};
}

void FXAudio2SoundSource::RouteMonoToDry(float ChannelVolumes[CHANNELOUT_COUNT])
{
	if (IsUsingDefaultSpatializer())
	{
		// Spatialised audio maps 1 channel to 6 speakers
		float SpatialisationMatrix[SPEAKER_COUNT * 1] =
		{
			ChannelVolumes[CHANNELOUT_FRONTLEFT],
			ChannelVolumes[CHANNELOUT_FRONTRIGHT],
			ChannelVolumes[CHANNELOUT_FRONTCENTER],
			ChannelVolumes[CHANNELOUT_LOWFREQUENCY],
			ChannelVolumes[CHANNELOUT_LEFTSURROUND],
			ChannelVolumes[CHANNELOUT_RIGHTSURROUND]
		};

		// Update the dry output to the mastering voice
		AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (mono)"), Source->SetOutputMatrix(Destinations[DEST_DRY].pOutputVoice, 1, SPEAKER_COUNT, SpatialisationMatrix));
	}
	else
	{
		// If we're spatializing using HRTF algorithms, then our output is actually stereo
		RouteStereoToDry(ChannelVolumes);
	}
}

void FXAudio2SoundSource::RouteStereoToDry(float ChannelVolumes[CHANNELOUT_COUNT])
{
	if (IsUsingDefaultSpatializer())
	{
		float SpatialisationMatrix[SPEAKER_COUNT * 2] =
		{
			ChannelVolumes[CHANNELOUT_FRONTLEFT], 0.0f,
			0.0f, ChannelVolumes[CHANNELOUT_FRONTRIGHT],
			0.0f, 0.0f,
			ChannelVolumes[CHANNELOUT_LOWFREQUENCY], ChannelVolumes[CHANNELOUT_LOWFREQUENCY],
			ChannelVolumes[CHANNELOUT_LEFTSURROUND], 0.0f,
			0.0f, ChannelVolumes[CHANNELOUT_RIGHTSURROUND]
		};

		// Stereo sounds map 2 channels to 6 speakers
		AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (stereo)"), Source->SetOutputMatrix(Destinations[DEST_DRY].pOutputVoice, 2, SPEAKER_COUNT, SpatialisationMatrix));
	}
	else
	{
		float SpatialisationMatrix[SPEAKER_COUNT * 2] =
		{
			ChannelVolumes[CHANNELOUT_FRONTLEFT], 0.0f,
			0.0f, ChannelVolumes[CHANNELOUT_FRONTRIGHT],
			0.0f, 0.0f,
			0.0f, 0.0f,
			0.0f, 0.0f,
			0.0f, 0.0f
		};

		// Stereo sounds map 2 channels to 6 speakers
		AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (stereo)"), Source->SetOutputMatrix(Destinations[DEST_DRY].pOutputVoice, 2, SPEAKER_COUNT, SpatialisationMatrix));
	}
}

void FXAudio2SoundSource::RouteQuadToDry(float ChannelVolumes[CHANNELOUT_COUNT])
{
	float SpatialisationMatrix[SPEAKER_COUNT * 4] =
	{
		ChannelVolumes[CHANNELOUT_FRONTLEFT], 0.0f, 0.0f, 0.0f,
		0.0f, ChannelVolumes[CHANNELOUT_FRONTRIGHT], 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		ChannelVolumes[CHANNELOUT_LOWFREQUENCY], ChannelVolumes[CHANNELOUT_LOWFREQUENCY], ChannelVolumes[CHANNELOUT_LOWFREQUENCY], ChannelVolumes[CHANNELOUT_LOWFREQUENCY],
		0.0f, 0.0f, ChannelVolumes[CHANNELOUT_LEFTSURROUND], 0.0f,
		0.0f, 0.0f, 0.0f, ChannelVolumes[CHANNELOUT_RIGHTSURROUND]
	};

	// Quad sounds map 4 channels to 6 speakers
	AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (4 channel)"),
		Source->SetOutputMatrix(Destinations[DEST_DRY].pOutputVoice, 4, SPEAKER_COUNT, SpatialisationMatrix));
}

void FXAudio2SoundSource::RouteHexToDry(float ChannelVolumes[CHANNELOUT_COUNT])
{
	if ((XAudio2Buffer->DecompressionState && XAudio2Buffer->DecompressionState->UsesVorbisChannelOrdering())
		|| WaveInstance->WaveData->bDecompressedFromOgg)
	{
		// Ordering of channels is different for 6 channel OGG
		float SpatialisationMatrix[SPEAKER_COUNT * 6] =
		{
			ChannelVolumes[CHANNELOUT_FRONTLEFT], 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, ChannelVolumes[CHANNELOUT_FRONTRIGHT], 0.0f, 0.0f, 0.0f,
			0.0f, ChannelVolumes[CHANNELOUT_FRONTCENTER], 0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, ChannelVolumes[CHANNELOUT_LOWFREQUENCY],
			0.0f, 0.0f, 0.0f, ChannelVolumes[CHANNELOUT_LEFTSURROUND], 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f, ChannelVolumes[CHANNELOUT_RIGHTSURROUND], 0.0f
		};

		// 5.1 sounds map 6 channels to 6 speakers
		AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (6 channel OGG)"),
			Source->SetOutputMatrix(Destinations[DEST_DRY].pOutputVoice, 6, SPEAKER_COUNT, SpatialisationMatrix));
	}
	else
	{
		float SpatialisationMatrix[SPEAKER_COUNT * 6] =
		{
			ChannelVolumes[CHANNELOUT_FRONTLEFT], 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, ChannelVolumes[CHANNELOUT_FRONTRIGHT], 0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, ChannelVolumes[CHANNELOUT_FRONTCENTER], 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, ChannelVolumes[CHANNELOUT_LOWFREQUENCY], 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f, ChannelVolumes[CHANNELOUT_LEFTSURROUND], 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f, 0.0f, ChannelVolumes[CHANNELOUT_RIGHTSURROUND]
		};

		// 5.1 sounds map 6 channels to 6 speakers
		AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (6 channel)"),
			Source->SetOutputMatrix(Destinations[DEST_DRY].pOutputVoice, 6, SPEAKER_COUNT, SpatialisationMatrix));
	}
}

/** 
 * Maps the sound to the relevant reverb effect
 */
void FXAudio2SoundSource::RouteToReverb( float ChannelVolumes[CHANNELOUT_COUNT] )
{
	// Reverb must be applied to process this function because the index 
	// of the destination output voice may not be at DEST_REVERB.
	check( bReverbApplied );

	switch( Buffer->NumChannels )
	{
	case 1:
		RouteMonoToReverb(ChannelVolumes);
		break;

	case 2:
		RouteStereoToReverb(ChannelVolumes);
		break;
	}
}

void FXAudio2SoundSource::RouteMonoToReverb(float ChannelVolumes[CHANNELOUT_COUNT])
{
	if (IsUsingDefaultSpatializer())
	{
		float SpatialisationMatrix[2] =
		{
			ChannelVolumes[CHANNELOUT_REVERB],
			ChannelVolumes[CHANNELOUT_REVERB],
		};

		// Update the dry output to the mastering voice
		AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (Mono reverb)"),
			Source->SetOutputMatrix(Destinations[DEST_REVERB].pOutputVoice, 1, 2, SpatialisationMatrix));
	}
	else
	{
		RouteStereoToReverb(ChannelVolumes);
	}
}

void FXAudio2SoundSource::RouteStereoToReverb(float ChannelVolumes[CHANNELOUT_COUNT])
{
	float SpatialisationMatrix[4] =
	{
		ChannelVolumes[CHANNELOUT_REVERB], 0.0f,
		0.0f, ChannelVolumes[CHANNELOUT_REVERB]
	};

	// Stereo sounds map 2 channels to 6 speakers
	AudioDevice->ValidateAPICall(TEXT("SetOutputMatrix (Stereo reverb)"),
		Source->SetOutputMatrix(Destinations[DEST_REVERB].pOutputVoice, 2, 2, SpatialisationMatrix));
}


/** 
 * Maps the sound to the relevant radio effect.
 *
 * @param	ChannelVolumes	The volumes associated to each channel. 
 *							Note: Not all channels are mapped directly to a speaker.
 */
void FXAudio2SoundSource::RouteToRadio( float ChannelVolumes[CHANNELOUT_COUNT] )
{
	// Radio distortion must be applied to process this function because 
	// the index of the destination output voice would be incorrect. 
	check( WaveInstance->bApplyRadioFilter );

	// Get the index for the radio voice because it doesn't 
	// necessarily match up to the enum value for radio.
	const int32 Index = GetDestinationVoiceIndexForEffect( DEST_RADIO );

	// If the index is -1, something changed with the Destinations array or 
	// SourceDestinations enum without an update to GetDestinationIndexForEffect().
	check( Index != -1 );

	// NOTE: The radio-distorted audio will only get routed to the center speaker.
	switch( Buffer->NumChannels )
	{
	case 1:
		{
			// Audio maps 1 channel to 6 speakers
			float OutputMatrix[SPEAKER_COUNT * 1] = 
			{		
				0.0f,
				0.0f,
				ChannelVolumes[CHANNELOUT_RADIO],
				0.0f,
				0.0f,
				0.0f,
			};

			// Mono sounds map 1 channel to 6 speakers.
			AudioDevice->ValidateAPICall( TEXT( "SetOutputMatrix (Mono radio)" ), 
				Source->SetOutputMatrix( Destinations[Index].pOutputVoice, 1, SPEAKER_COUNT, OutputMatrix ) );
		}
		break;

	case 2:
		{
			// Audio maps 2 channels to 6 speakers
			float OutputMatrix[SPEAKER_COUNT * 2] = 
			{			
				0.0f, 0.0f,
				0.0f, 0.0f,
				ChannelVolumes[CHANNELOUT_RADIO], ChannelVolumes[CHANNELOUT_RADIO],
				0.0f, 0.0f,
				0.0f, 0.0f,
				0.0f, 0.0f,
			};

			// Stereo sounds map 2 channels to 6 speakers.
			AudioDevice->ValidateAPICall( TEXT( "SetOutputMatrix (Stereo radio)" ), 
				Source->SetOutputMatrix( Destinations[Index].pOutputVoice, 2, SPEAKER_COUNT, OutputMatrix ) );
		}
		break;
	}
}


/**
 * Utility function for determining the proper index of an effect. Certain effects (such as: reverb and radio distortion) 
 * are optional. Thus, they may be NULL, yet XAudio2 cannot have a NULL output voice in the send list for this source voice.
 *
 * @return	The index of the destination XAudio2 submix voice for the given effect; -1 if effect not in destination array. 
 *
 * @param	Effect	The effect type's (Reverb, Radio Distoriton, etc) index to find. 
 */
int32 FXAudio2SoundSource::GetDestinationVoiceIndexForEffect( SourceDestinations Effect )
{
	int32 Index = -1;

	switch( Effect )
	{
	case DEST_DRY:
		// The dry mix is ALWAYS the first voice because always set it. 
		Index = 0;
		break;

	case DEST_REVERB:
		// If reverb is applied, then it comes right after the dry mix.
		Index = bReverbApplied ? DEST_REVERB : -1;
		break;

	case DEST_RADIO:
		// If radio distortion is applied, it depends on if there is 
		// reverb in the chain. Radio will always come after reverb.
		Index = ( WaveInstance->bApplyRadioFilter ) ? ( bReverbApplied ? DEST_RADIO : DEST_REVERB ) : -1;
		break;

	default:
		Index = -1;
	}

	return Index;
}

/**
 * Callback to let the sound system know the sound has looped
 */
void FXAudio2SoundSourceCallback::OnLoopEnd( void* BufferContext )
{
	if( BufferContext )
	{
		FXAudio2SoundSource* Source = ( FXAudio2SoundSource* )BufferContext;
		Source->bLoopCallback = true;
	}
}

FString FXAudio2SoundSource::Describe(bool bUseLongName)
{
	return Describe_Internal(bUseLongName, true);
}

FString FXAudio2SoundSource::Describe_Internal(bool bUseLongName, bool bIncludeChannelVolumes)
{
	// look for a component and its owner
	AActor* SoundOwner = NULL;

	// TODO - Audio Threading. This won't work cross thread.
	if (WaveInstance->ActiveSound && WaveInstance->ActiveSound->AudioComponent.IsValid())
	{
		SoundOwner = WaveInstance->ActiveSound->AudioComponent->GetOwner();
	}

	FString SpatializedVolumeInfo;
	if (bIncludeChannelVolumes && WaveInstance->bUseSpatialization)
	{
		float ChannelVolumes[CHANNELOUT_COUNT] = { 0.0f };
		GetChannelVolumes( ChannelVolumes, WaveInstance->GetActualVolume() );

		SpatializedVolumeInfo = FString::Printf(TEXT(" (FL: %.2f FR: %.2f FC: %.2f LF: %.2f, LS: %.2f, RS: %.2f)"),
			ChannelVolumes[CHANNELOUT_FRONTLEFT],
			ChannelVolumes[CHANNELOUT_FRONTRIGHT],
			ChannelVolumes[CHANNELOUT_FRONTCENTER],
			ChannelVolumes[CHANNELOUT_LOWFREQUENCY],
			ChannelVolumes[CHANNELOUT_LEFTSURROUND],
			ChannelVolumes[CHANNELOUT_RIGHTSURROUND]);
	}

	return FString::Printf(TEXT("Wave: %s, Volume: %6.2f%s, Owner: %s"), 
		bUseLongName ? *WaveInstance->WaveData->GetPathName() : *WaveInstance->WaveData->GetName(),
		WaveInstance->GetActualVolume(),
		*SpatializedVolumeInfo,
		SoundOwner ? *SoundOwner->GetName() : TEXT("None"));

}

/**
 * Updates the source specific parameter like e.g. volume and pitch based on the associated
 * wave instance.	
 */
void FXAudio2SoundSource::Update( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioUpdateSources );

	if( !WaveInstance || !Source || Paused )
	{	
		return;
	}

	const float Pitch = FMath::Clamp<float>( WaveInstance->Pitch, MIN_PITCH, MAX_PITCH );
	AudioDevice->ValidateAPICall( TEXT( "SetFrequencyRatio" ), 
		Source->SetFrequencyRatio( Pitch ) );

	// Set whether to bleed to the rear speakers
	SetStereoBleed();

	// Set the amount to bleed to the LFE speaker
	SetLFEBleed();

	// Set the HighFrequencyGain value (aka low pass filter setting)
	SetHighFrequencyGain();

	// Apply the low pass filter
	XAUDIO2_FILTER_PARAMETERS LPFParameters = { LowPassFilter, 1.0f, 1.0f };
	if( HighFrequencyGain < 1.0f - KINDA_SMALL_NUMBER )
	{
		float FilterConstant = 2.0f * FMath::Sin( PI * 6000.0f * HighFrequencyGain / 48000.0f );
		LPFParameters.Frequency = FilterConstant;
		LPFParameters.OneOverQ = AudioDevice->GetLowPassFilterResonance();
	}

	AudioDevice->ValidateAPICall( TEXT( "SetFilterParameters" ), 
		Source->SetFilterParameters( &LPFParameters ) );	

	// Initialize channel volumes
	float ChannelVolumes[CHANNELOUT_COUNT] = { 0.0f };

	GetChannelVolumes( ChannelVolumes, WaveInstance->GetActualVolume() );

	// Send to the 5.1 channels
	RouteDryToSpeakers( ChannelVolumes );

	// Send to the reverb channel
	if( bReverbApplied )
	{
		RouteToReverb( ChannelVolumes );
	}

	// If this audio can have radio distortion applied, 
	// send the volumes to the radio distortion voice. 
	if( WaveInstance->bApplyRadioFilter )
	{
		RouteToRadio( ChannelVolumes );
	}
}

/**
 * Plays the current wave instance.	
 */
void FXAudio2SoundSource::Play( void )
{
	if( WaveInstance )
	{
		if( !Playing )
		{
			if( Buffer->NumChannels >= SPEAKER_COUNT )
			{
				XMPHelper.CinematicAudioStarted();
			}
		}

		if( Source )
		{
			AudioDevice->ValidateAPICall( TEXT( "Start" ), 
				Source->Start( 0 ) );
		}

		Paused = false;
		Playing = true;
		bBuffersToFlush = false;
		bLoopCallback = false;
	}
}

/**
 * Stops the current wave instance and detaches it from the source.	
 */
void FXAudio2SoundSource::Stop( void )
{
	if( WaveInstance )
	{	
		if( Playing )
		{
			if( Buffer->NumChannels >= SPEAKER_COUNT )
			{
				XMPHelper.CinematicAudioStopped();
			}
		}

		if( Source )
		{
			AudioDevice->ValidateAPICall( TEXT( "Stop" ), 
				Source->Stop( XAUDIO2_PLAY_TAILS ) );

			// Free resources
			FreeResources();
		}

		Paused = false;
		Playing = false;
		Buffer = NULL;
		bBuffersToFlush = false;
		bLoopCallback = false;
		bResourcesNeedFreeing = false;
	}

	FSoundSource::Stop();
}

/**
 * Pauses playback of current wave instance.
 */
void FXAudio2SoundSource::Pause( void )
{
	if( WaveInstance )
	{
		if( Source )
		{
			AudioDevice->ValidateAPICall( TEXT( "Stop" ), 
				Source->Stop( 0 ) );
		}

		Paused = true;
	}
}

/**
 * Handles feeding new data to a real time decompressed sound
 */
void FXAudio2SoundSource::HandleRealTimeSource( void )
{
	// Update the double buffer toggle
	++CurrentBuffer;

	// Get the next bit of streaming data
	const bool bLooped = ReadMorePCMData(CurrentBuffer & 1);

	// Have we reached the end of the compressed sound?
	if( bLooped )
	{
		switch( WaveInstance->LoopingMode )
		{
		case LOOP_Never:
			// Play out any queued buffers - once there are no buffers left, the state check at the beginning of IsFinished will fire
			bBuffersToFlush = true;
			XAudio2Buffers[CurrentBuffer & 1].Flags |= XAUDIO2_END_OF_STREAM;
			break;

		case LOOP_WithNotification:
			// If we have just looped, and we are programmatically looping, send notification
			WaveInstance->NotifyFinished();
			break;

		case LOOP_Forever:
			// Let the sound loop indefinitely
			break;
		}
	}
}

/**
 * Queries the status of the currently associated wave instance.
 *
 * @return	true if the wave instance/ source has finished playback and false if it is 
 *			currently playing or paused.
 */
bool FXAudio2SoundSource::IsFinished( void )
{
	// A paused source is not finished.
	if( Paused )
	{
		return( false );
	}

	if( WaveInstance && Source )
	{
		// Retrieve state source is in.
		XAUDIO2_VOICE_STATE SourceState;
		Source->GetState( &SourceState );

		const bool bIsRealTimeSource = XAudio2Buffer->SoundFormat == SoundFormat_PCMRT || XAudio2Buffer->SoundFormat == SoundFormat_Streaming;

		// If we have no queued buffers, we're either at the end of a sound, or starved
		if( SourceState.BuffersQueued == 0 )
		{
			// ... are we expecting the sound to be finishing?
			if( bBuffersToFlush || !bIsRealTimeSource )
			{
				// ... notify the wave instance that it has finished playing.
				WaveInstance->NotifyFinished();
				return( true );
			}
		}

		// Service any real time sounds
		if( bIsRealTimeSource )
		{
			if( SourceState.BuffersQueued <= 1 )
			{
				// Continue feeding new sound data (unless we are waiting for the sound to finish)
				if( !bBuffersToFlush )
				{
					HandleRealTimeSource();

					if (XAudio2Buffers[CurrentBuffer & 1].AudioBytes > 0)
					{
						AudioDevice->ValidateAPICall( TEXT( "SubmitSourceBuffer - IsFinished" ), 
							Source->SubmitSourceBuffer( &XAudio2Buffers[CurrentBuffer & 1] ) );
					}
					else
					{
						--CurrentBuffer;
					}

				}
			}

			return( false );
		}

		// Notify the wave instance that the looping callback was hit
		if( bLoopCallback && WaveInstance->LoopingMode == LOOP_WithNotification )
		{
			WaveInstance->NotifyFinished();

			bLoopCallback = false;
		}

		return( false );
	}

	return( true );
}

bool FXAudio2SoundSource::IsUsingDefaultSpatializer()
{
	return bUsingDefaultSpatialization;
}

bool FXAudio2SoundSource::CreateWithSpatializationEffect()
{
	return (Buffer->NumChannels == 1 &&
			AudioDevice->SpatializationPlugin != nullptr &&
			AudioDevice->IsSpatializationPluginEnabled() &&
			WaveInstance->SpatializationAlgorithm != SPATIALIZATION_Default);
}

/*------------------------------------------------------------------------------------
	FSpatializationHelper.
------------------------------------------------------------------------------------*/

/**
 * Constructor, initializing all member variables.
 */
FSpatializationHelper::FSpatializationHelper( void )
{
}

void FSpatializationHelper::Init()
{
	// Initialize X3DAudio
	//
	//  Speaker geometry configuration on the final mix, specifies assignment of channels
	//  to speaker positions, defined as per WAVEFORMATEXTENSIBLE.dwChannelMask
	X3DAudioInitialize( UE4_XAUDIO2_CHANNELMASK, X3DAUDIO_SPEED_OF_SOUND, X3DInstance );

	// Initialize 3D audio parameters
#if X3DAUDIO_VECTOR_IS_A_D3DVECTOR
	X3DAUDIO_VECTOR ZeroVector = { 0.0f, 0.0f, 0.0f };
#else	//X3DAUDIO_VECTOR_IS_A_D3DVECTOR
	X3DAUDIO_VECTOR ZeroVector(0.0f, 0.0f, 0.0f);
#endif	//X3DAUDIO_VECTOR_IS_A_D3DVECTOR

	// Set up listener parameters
	Listener.OrientFront.x = 0.0f;
	Listener.OrientFront.y = 0.0f;
	Listener.OrientFront.z = 1.0f;
	Listener.OrientTop.x = 0.0f;
	Listener.OrientTop.y = 1.0f;
	Listener.OrientTop.z = 0.0f;
	Listener.Position.x	= 0.0f;
	Listener.Position.y = 0.0f;
	Listener.Position.z = 0.0f;
	Listener.Velocity = ZeroVector;

	// Set up emitter parameters
	Emitter.OrientFront.x = 0.0f;
	Emitter.OrientFront.y = 0.0f;
	Emitter.OrientFront.z = 1.0f;
	Emitter.OrientTop.x = 0.0f;
	Emitter.OrientTop.y = 1.0f;
	Emitter.OrientTop.z = 0.0f;
	Emitter.Position = ZeroVector;
	Emitter.Velocity = ZeroVector;
	Emitter.pCone = &Cone;
	Emitter.pCone->InnerAngle = 0.0f; 
	Emitter.pCone->OuterAngle = 0.0f;
	Emitter.pCone->InnerVolume = 0.0f;
	Emitter.pCone->OuterVolume = 1.0f;
	Emitter.pCone->InnerLPF = 0.0f;
	Emitter.pCone->OuterLPF = 1.0f;
	Emitter.pCone->InnerReverb = 0.0f;
	Emitter.pCone->OuterReverb = 1.0f;

	Emitter.ChannelCount = UE4_XAUDIO3D_INPUTCHANNELS;
	Emitter.ChannelRadius = 0.0f;
	Emitter.pChannelAzimuths = EmitterAzimuths;

	// real volume -> 5.1-ch rate
	VolumeCurvePoint[0].Distance = 0.0f;
	VolumeCurvePoint[0].DSPSetting = 1.0f;
	VolumeCurvePoint[1].Distance = 1.0f;
	VolumeCurvePoint[1].DSPSetting = 1.0f;
	VolumeCurve.PointCount = ARRAY_COUNT( VolumeCurvePoint );
	VolumeCurve.pPoints = VolumeCurvePoint;

	ReverbVolumeCurvePoint[0].Distance = 0.0f;
	ReverbVolumeCurvePoint[0].DSPSetting = 0.5f;
	ReverbVolumeCurvePoint[1].Distance = 1.0f;
	ReverbVolumeCurvePoint[1].DSPSetting = 0.5f;
	ReverbVolumeCurve.PointCount = ARRAY_COUNT( ReverbVolumeCurvePoint );
	ReverbVolumeCurve.pPoints = ReverbVolumeCurvePoint;

	Emitter.pVolumeCurve = &VolumeCurve;
	Emitter.pLFECurve = NULL;
	Emitter.pLPFDirectCurve = NULL;
	Emitter.pLPFReverbCurve = NULL;
	Emitter.pReverbCurve = &ReverbVolumeCurve;
	Emitter.CurveDistanceScaler = 1.0f;
	Emitter.DopplerScaler = 1.0f;

	DSPSettings.SrcChannelCount = UE4_XAUDIO3D_INPUTCHANNELS;
	DSPSettings.DstChannelCount = SPEAKER_COUNT;
	DSPSettings.pMatrixCoefficients = MatrixCoefficients;
}

void FSpatializationHelper::DumpSpatializationState() const
{
	struct FLocal
	{
		static void DumpChannelArray(const FString& Indent, const FString& ArrayName, int32 NumChannels, const float* pChannelArray)
		{
			if (pChannelArray)
			{
				FString CommaSeparatedValueString;
				for (int32 ChannelIdx = 0; ChannelIdx < NumChannels; ++ChannelIdx)
				{
					if ( ChannelIdx > 0 )
					{
						CommaSeparatedValueString += TEXT(",");
					}

					CommaSeparatedValueString += FString::Printf(TEXT("%f"), pChannelArray[ChannelIdx]);
				}

				UE_LOG(LogXAudio2, Log, TEXT("%s%s: {%s}"), *Indent, *ArrayName, *CommaSeparatedValueString);
			}
			else
			{
				UE_LOG(LogXAudio2, Log, TEXT("%s%s: NULL"), *Indent, *ArrayName);
			}
		}

		static void DumpCone(const FString& Indent, const FString& ConeName, const X3DAUDIO_CONE* pCone)
		{
			if (pCone)
			{
				UE_LOG(LogXAudio2, Log, TEXT("%s%s"), *Indent, *ConeName);
				UE_LOG(LogXAudio2, Log, TEXT("%s  InnerAngle: %f"), *Indent, pCone->InnerAngle);
				UE_LOG(LogXAudio2, Log, TEXT("%s  OuterAngle: %f"), *Indent, pCone->OuterAngle);
				UE_LOG(LogXAudio2, Log, TEXT("%s  InnerVolume: %f"), *Indent, pCone->InnerVolume);
				UE_LOG(LogXAudio2, Log, TEXT("%s  OuterVolume: %f"), *Indent, pCone->OuterVolume);
				UE_LOG(LogXAudio2, Log, TEXT("%s  InnerLPF: %f"), *Indent, pCone->InnerLPF);
				UE_LOG(LogXAudio2, Log, TEXT("%s  OuterLPF: %f"), *Indent, pCone->OuterLPF);
				UE_LOG(LogXAudio2, Log, TEXT("%s  InnerReverb: %f"), *Indent, pCone->InnerReverb);
				UE_LOG(LogXAudio2, Log, TEXT("%s  OuterReverb: %f"), *Indent, pCone->OuterReverb);
			}
			else
			{
				UE_LOG(LogXAudio2, Log, TEXT("%s%s: NULL"), *Indent, *ConeName);
			}
		}

		static void DumpDistanceCurvePoint(const FString& Indent, const FString& PointName, uint32 Index, const X3DAUDIO_DISTANCE_CURVE_POINT& Point)
		{
			UE_LOG(LogXAudio2, Log, TEXT("%s%s[%u]: {%f,%f}"), *Indent, *PointName, Index, Point.Distance, Point.DSPSetting);
		}

		static void DumpDistanceCurve(const FString& Indent, const FString& CurveName, const X3DAUDIO_DISTANCE_CURVE* pCurve)
		{
			const uint32 MaxPointsToDump = 20;
			if (pCurve)
			{
				UE_LOG(LogXAudio2, Log, TEXT("%s%s: %u points"), *Indent, *CurveName, pCurve->PointCount);
				for (uint32 PointIdx = 0; PointIdx < pCurve->PointCount && PointIdx < MaxPointsToDump; ++PointIdx)
				{
					const X3DAUDIO_DISTANCE_CURVE_POINT& CurPoint = pCurve->pPoints[PointIdx];
					DumpDistanceCurvePoint(Indent + TEXT("  "), TEXT("pPoints"), PointIdx, CurPoint);
				}
			}
			else
			{
				UE_LOG(LogXAudio2, Log, TEXT("%s%s: NULL"), *Indent, *CurveName);
			}
		}
	};

	UE_LOG(LogXAudio2, Log, TEXT("Dumping all XAudio2 Spatialization"));
	UE_LOG(LogXAudio2, Log, TEXT("==================================="));
	
	// X3DInstance
	UE_LOG(LogXAudio2, Log, TEXT("  X3DInstance: %#010x"), X3DInstance);

	// DSPSettings
	UE_LOG(LogXAudio2, Log, TEXT("  DSPSettings"));
	FLocal::DumpChannelArray(TEXT("    "), TEXT("pMatrixCoefficients"), ARRAY_COUNT(MatrixCoefficients), DSPSettings.pMatrixCoefficients);
	FLocal::DumpChannelArray(TEXT("    "), TEXT("pDelayTimes"), ARRAY_COUNT(MatrixCoefficients), DSPSettings.pDelayTimes);
	UE_LOG(LogXAudio2, Log, TEXT("    SrcChannelCount: %u"), DSPSettings.SrcChannelCount);
	UE_LOG(LogXAudio2, Log, TEXT("    DstChannelCount: %u"), DSPSettings.DstChannelCount);
	UE_LOG(LogXAudio2, Log, TEXT("    LPFDirectCoefficient: %f"), DSPSettings.LPFDirectCoefficient);
	UE_LOG(LogXAudio2, Log, TEXT("    LPFReverbCoefficient: %f"), DSPSettings.LPFReverbCoefficient);
	UE_LOG(LogXAudio2, Log, TEXT("    ReverbLevel: %f"), DSPSettings.ReverbLevel);
	UE_LOG(LogXAudio2, Log, TEXT("    DopplerFactor: %f"), DSPSettings.DopplerFactor);
	UE_LOG(LogXAudio2, Log, TEXT("    EmitterToListenerAngle: %f"), DSPSettings.EmitterToListenerAngle);
	UE_LOG(LogXAudio2, Log, TEXT("    EmitterToListenerDistance: %f"), DSPSettings.EmitterToListenerDistance);
	UE_LOG(LogXAudio2, Log, TEXT("    EmitterVelocityComponent: %f"), DSPSettings.EmitterVelocityComponent);
	UE_LOG(LogXAudio2, Log, TEXT("    ListenerVelocityComponent: %f"), DSPSettings.ListenerVelocityComponent);

	// Listener
	UE_LOG(LogXAudio2, Log, TEXT("  Listener"));
	UE_LOG(LogXAudio2, Log, TEXT("    OrientFront: {%f,%f,%f}"), Listener.OrientFront.x, Listener.OrientFront.y, Listener.OrientFront.z);
	UE_LOG(LogXAudio2, Log, TEXT("    OrientTop: {%f,%f,%f}"), Listener.OrientTop.x, Listener.OrientTop.y, Listener.OrientTop.z);
	UE_LOG(LogXAudio2, Log, TEXT("    Position: {%f,%f,%f}"), Listener.Position.x, Listener.Position.y, Listener.Position.z);
	UE_LOG(LogXAudio2, Log, TEXT("    Velocity: {%f,%f,%f}"), Listener.Velocity.x, Listener.Velocity.y, Listener.Velocity.z);
	FLocal::DumpCone(TEXT("    "), TEXT("pCone"), Listener.pCone);

	// Emitter
	UE_LOG(LogXAudio2, Log, TEXT("  Emitter"));
	FLocal::DumpCone(TEXT("    "), TEXT("pCone"), Emitter.pCone);
	UE_LOG(LogXAudio2, Log, TEXT("    OrientFront: {%f,%f,%f}"), Emitter.OrientFront.x, Emitter.OrientFront.y, Emitter.OrientFront.z);
	UE_LOG(LogXAudio2, Log, TEXT("    OrientTop: {%f,%f,%f}"), Emitter.OrientTop.x, Emitter.OrientTop.y, Emitter.OrientTop.z);
	UE_LOG(LogXAudio2, Log, TEXT("    Position: {%f,%f,%f}"), Emitter.Position.x, Emitter.Position.y, Emitter.Position.z);
	UE_LOG(LogXAudio2, Log, TEXT("    Velocity: {%f,%f,%f}"), Emitter.Velocity.x, Emitter.Velocity.y, Emitter.Velocity.z);
	UE_LOG(LogXAudio2, Log, TEXT("    InnerRadius: %f"), Emitter.InnerRadius);
	UE_LOG(LogXAudio2, Log, TEXT("    InnerRadiusAngle: %f"), Emitter.InnerRadiusAngle);
	UE_LOG(LogXAudio2, Log, TEXT("    ChannelCount: %u"), Emitter.ChannelCount);
	UE_LOG(LogXAudio2, Log, TEXT("    ChannelRadius: %f"), Emitter.ChannelRadius);

	if ( Emitter.pChannelAzimuths )
	{
		UE_LOG(LogXAudio2, Log, TEXT("    pChannelAzimuths: %f"), *Emitter.pChannelAzimuths);
	}
	else
	{
		UE_LOG(LogXAudio2, Log, TEXT("    pChannelAzimuths: NULL"));
	}

	FLocal::DumpDistanceCurve(TEXT("    "), TEXT("pVolumeCurve"), Emitter.pVolumeCurve);
	FLocal::DumpDistanceCurve(TEXT("    "), TEXT("pLFECurve"), Emitter.pLFECurve);
	FLocal::DumpDistanceCurve(TEXT("    "), TEXT("pLPFDirectCurve"), Emitter.pLPFDirectCurve);
	FLocal::DumpDistanceCurve(TEXT("    "), TEXT("pLPFReverbCurve"), Emitter.pLPFReverbCurve);
	FLocal::DumpDistanceCurve(TEXT("    "), TEXT("pReverbCurve"), Emitter.pReverbCurve);

	UE_LOG(LogXAudio2, Log, TEXT("    CurveDistanceScaler: %f"), Emitter.CurveDistanceScaler);
	UE_LOG(LogXAudio2, Log, TEXT("    DopplerScaler: %f"), Emitter.DopplerScaler);

	// Cone
	FLocal::DumpCone(TEXT("  "), TEXT("Cone"), &Cone);

	// VolumeCurvePoint
	FLocal::DumpDistanceCurvePoint(TEXT("  "), TEXT("VolumeCurvePoint"), 0, VolumeCurvePoint[0]);
	FLocal::DumpDistanceCurvePoint(TEXT("  "), TEXT("VolumeCurvePoint"), 1, VolumeCurvePoint[1]);
	
	// VolumeCurve
	FLocal::DumpDistanceCurve(TEXT("  "), TEXT("VolumeCurve"), &VolumeCurve);

	// ReverbVolumeCurvePoint
	FLocal::DumpDistanceCurvePoint(TEXT("  "), TEXT("ReverbVolumeCurvePoint"), 0, ReverbVolumeCurvePoint[0]);
	FLocal::DumpDistanceCurvePoint(TEXT("  "), TEXT("ReverbVolumeCurvePoint"), 1, ReverbVolumeCurvePoint[1]);

	// ReverbVolumeCurve
	FLocal::DumpDistanceCurve(TEXT("  "), TEXT("ReverbVolumeCurve"), &ReverbVolumeCurve);
	
	// EmitterAzimuths
	FLocal::DumpChannelArray(TEXT("  "), TEXT("EmitterAzimuths"), UE4_XAUDIO3D_INPUTCHANNELS, EmitterAzimuths);
	
	// MatrixCoefficients
	FLocal::DumpChannelArray(TEXT("  "), TEXT("MatrixCoefficients"), ARRAY_COUNT(MatrixCoefficients), MatrixCoefficients);
}

void FSpatializationHelper::CalculateDolbySurroundRate( const FVector& OrientFront, const FVector& ListenerPosition, const FVector& EmitterPosition, float OmniRadius, float* OutVolumes  )
{
	uint32 CalculateFlags = X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER | X3DAUDIO_CALCULATE_REVERB;

	Listener.OrientFront.x = OrientFront.X;
	Listener.OrientFront.y = OrientFront.Y;
	Listener.OrientFront.z = OrientFront.Z;
	Listener.Position.x = ListenerPosition.X;
	Listener.Position.y = ListenerPosition.Y;
	Listener.Position.z = ListenerPosition.Z;
	Emitter.Position.x = EmitterPosition.X;
	Emitter.Position.y = EmitterPosition.Y;
	Emitter.Position.z = EmitterPosition.Z;
	Emitter.InnerRadius = OmniRadius*OmniRadius;
	Emitter.InnerRadiusAngle = 0;

	X3DAudioCalculate( X3DInstance, &Listener, &Emitter, CalculateFlags, &DSPSettings );

	for( int32 SpeakerIndex = 0; SpeakerIndex < SPEAKER_COUNT; SpeakerIndex++ )
	{
		OutVolumes[SpeakerIndex] *= DSPSettings.pMatrixCoefficients[SpeakerIndex];

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		// Detect and warn about NaN and INF volumes. XAudio does not do this internally and behavior is undefined.
		if (!FMath::IsFinite(OutVolumes[SpeakerIndex]))
		{
			const FString NaNorINF = FMath::IsNaN(OutVolumes[SpeakerIndex]) ? TEXT("NaN") : TEXT("INF");
			UE_LOG(LogXAudio2, Warning, TEXT("CalculateDolbySurroundRate generated a %s in channel %d. OmniRadius:%f MatrixCoefficient:%f"),
				*NaNorINF, SpeakerIndex, OmniRadius, DSPSettings.pMatrixCoefficients[SpeakerIndex]);
			//DumpSpatializationState();
		}
#endif
	}

	OutVolumes[CHANNELOUT_REVERB] *= DSPSettings.ReverbLevel;
}

/*------------------------------------------------------------------------------------
	FXMPHelper.
------------------------------------------------------------------------------------*/

/**
 * FXMPHelper constructor - Protected.
 */
FXMPHelper::FXMPHelper( void )
	: CinematicAudioCount( 0 )
, MoviePlaying( false )
, XMPEnabled( true )
, XMPBlocked( false )
{
}

/**
 * FXMPHelper destructor.
 */
FXMPHelper::~FXMPHelper( void )
{
}

/**
 * Records that a cinematic audio track has started playing.
 */
void FXMPHelper::CinematicAudioStarted( void )
{
	check( CinematicAudioCount >= 0 );
	CinematicAudioCount++;
	CountsUpdated();
}

/**
 * Records that a cinematic audio track has stopped playing.
 */
void FXMPHelper::CinematicAudioStopped( void )
{
	check( CinematicAudioCount > 0 );
	CinematicAudioCount--;
	CountsUpdated();
}

/**
 * Records that a cinematic audio track has started playing.
 */
void FXMPHelper::MovieStarted( void )
{
	MoviePlaying = true;
	CountsUpdated();
}

/**
 * Records that a cinematic audio track has stopped playing.
 */
void FXMPHelper::MovieStopped( void )
{
	MoviePlaying = false;
	CountsUpdated();
}

/**
 * Called every frame to update XMP status if necessary
 */
void FXMPHelper::CountsUpdated( void )
{
	if( XMPEnabled )
	{
		if( ( MoviePlaying == true ) || ( CinematicAudioCount > 0 ) )
		{
			XMPEnabled = false;
		}
	}
	else
	{
		if( ( MoviePlaying == false ) && ( CinematicAudioCount == 0 ) )
		{
			XMPEnabled = true;
		}
	}
}

// end
