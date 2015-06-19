// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 	CoreAudioSource.cpp: Unreal CoreAudio source interface object.
 =============================================================================*/

/*------------------------------------------------------------------------------------
 Audio includes.
 ------------------------------------------------------------------------------------*/

#include "CoreAudioDevice.h"
#include "CoreAudioEffects.h"
#include "Engine.h"

#define AUDIO_DISTANCE_FACTOR ( 0.0127f )

// CoreAudio functions may return error -10863 (output device not available) if called while OS X is switching to a different output device.
// This happens, for example, when headphones are plugged in or unplugged.
#define SAFE_CA_CALL(Expression)\
{\
	OSStatus Status = noErr;\
	do {\
		Status = Expression;\
	} while (Status == -10863);\
	check(Status == noErr);\
}

FCoreAudioSoundSource *GAudioChannels[MAX_AUDIOCHANNELS + 1];

static int32 FindFreeAudioChannel()
{
	for( int32 Index = 1; Index < MAX_AUDIOCHANNELS + 1; Index++ )
	{
		if( GAudioChannels[Index] == NULL )
		{
			return Index;
		}
	}

	return 0;
}

/*------------------------------------------------------------------------------------
 FCoreAudioSoundSource.
 ------------------------------------------------------------------------------------*/

/**
 * Simple constructor
 */
FCoreAudioSoundSource::FCoreAudioSoundSource( FAudioDevice* InAudioDevice )
:	FSoundSource( InAudioDevice ),
	Buffer( NULL ),
	CoreAudioConverter( NULL ),
	bStreamedSound( false ),
	bBuffersToFlush( false ),
	SourceNode( 0 ),
	SourceUnit( NULL ),
	StreamSplitterNode( 0 ),
	StreamSplitterUnit( NULL ),
	EQNode( 0 ),
	EQUnit( NULL ),
	RadioNode( 0 ),
	RadioUnit( NULL ),
	bRadioMuted( false ),
	ReverbNode( 0 ),
	ReverbUnit( NULL ),
	bReverbMuted( false ),
	bDryMuted( false ),
	StreamMergerNode( 0 ),
	StreamMergerUnit( NULL ),
	AudioChannel( 0 ),
	BufferInUse( 0 ),
	NumActiveBuffers( 0 ),
	MixerInputNumber( -1 )
{
	AudioDevice = ( FCoreAudioDevice *)InAudioDevice;
	check( AudioDevice );
	Effects = ( FCoreAudioEffectsManager* )AudioDevice->Effects;
	check( Effects );

	FMemory::Memzero( CoreAudioBuffers, sizeof( CoreAudioBuffers ) );
}

/**
 * Destructor, cleaning up voice
 */
FCoreAudioSoundSource::~FCoreAudioSoundSource()
{
	FreeResources();
}

/**
 * Free up any allocated resources
 */
void FCoreAudioSoundSource::FreeResources( void )
{
	// If we're a streaming buffer...
	if( bStreamedSound )
	{
		// ... free the buffers
		FMemory::Free( ( void* )CoreAudioBuffers[0].AudioData );
		FMemory::Free( ( void* )CoreAudioBuffers[1].AudioData );
		
		// Buffers without a valid resource ID are transient and need to be deleted.
		if( Buffer )
		{
			check( Buffer->ResourceID == 0 );
			delete Buffer;
			Buffer = NULL;
		}
		
		bStreamedSound = false;
	}
}

/** 
 * Submit the relevant audio buffers to the system
 */
void FCoreAudioSoundSource::SubmitPCMBuffers( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioSubmitBuffersTime );

	FMemory::Memzero( CoreAudioBuffers, sizeof( CoreAudioBuffers ) );
	
	bStreamedSound = false;

	NumActiveBuffers=1;
	BufferInUse=0;

	CoreAudioBuffers[0].AudioData = Buffer->PCMData;
	CoreAudioBuffers[0].AudioDataSize = Buffer->PCMDataSize;
}

bool FCoreAudioSoundSource::ReadProceduralData( const int32 BufferIndex )
{
	const int32 MaxSamples = ( MONO_PCM_BUFFER_SIZE * Buffer->NumChannels ) / sizeof( int16 );
	const int32 BytesRead = WaveInstance->WaveData->GeneratePCMData( CoreAudioBuffers[BufferIndex].AudioData, MaxSamples );

	CoreAudioBuffers[BufferIndex].AudioDataSize = BytesRead;

	if (BytesRead > 0)
	{
		++NumActiveBuffers;
	}
	
	// convenience return value: we're never actually "looping" here.
	return false;
}

bool FCoreAudioSoundSource::ReadMorePCMData( const int32 BufferIndex )
{
	CoreAudioBuffers[BufferIndex].ReadCursor = 0;

	USoundWave *WaveData = WaveInstance->WaveData;
	if( WaveData && WaveData->bProcedural )
	{
		return ReadProceduralData( BufferIndex );
	}
	else
	{
		NumActiveBuffers++;
		return Buffer->ReadCompressedData( CoreAudioBuffers[BufferIndex].AudioData, WaveInstance->LoopingMode != LOOP_Never );
	}
}

/** 
 * Submit the relevant audio buffers to the system
 */
void FCoreAudioSoundSource::SubmitPCMRTBuffers( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioSubmitBuffersTime );

	FMemory::Memzero( CoreAudioBuffers, sizeof( CoreAudioBuffer ) * 2 );

	bStreamedSound = true;

	// Set up double buffer area to decompress to
	CoreAudioBuffers[0].AudioData = ( uint8* )FMemory::Malloc( MONO_PCM_BUFFER_SIZE * Buffer->NumChannels );
	CoreAudioBuffers[0].AudioDataSize = MONO_PCM_BUFFER_SIZE * Buffer->NumChannels;

	CoreAudioBuffers[1].AudioData = ( uint8* )FMemory::Malloc( MONO_PCM_BUFFER_SIZE * Buffer->NumChannels );
	CoreAudioBuffers[1].AudioDataSize = MONO_PCM_BUFFER_SIZE * Buffer->NumChannels;

	NumActiveBuffers = 0;
	BufferInUse = 0;
	
	ReadMorePCMData(0);
	ReadMorePCMData(1);
}

/**
 * Initializes a source with a given wave instance and prepares it for playback.
 *
 * @param	WaveInstance	wave instance being primed for playback
 * @return	true			if initialization was successful, false otherwise
 */
bool FCoreAudioSoundSource::Init( FWaveInstance* InWaveInstance )
{
	if (InWaveInstance->OutputTarget != EAudioOutputTarget::Controller)
	{
		// Find matching buffer.
		Buffer = FCoreAudioSoundBuffer::Init( AudioDevice, InWaveInstance->WaveData, InWaveInstance->StartTime > 0.f );
	
		// Buffer failed to be created, or there was an error with the compressed data
		if( Buffer && Buffer->NumChannels > 0 )
		{
			SCOPE_CYCLE_COUNTER( STAT_AudioSourceInitTime );
		
			WaveInstance = InWaveInstance;

			// Set whether to apply reverb
			SetReverbApplied( true );

			if (WaveInstance->StartTime > 0.f)
			{
				Buffer->Seek(WaveInstance->StartTime);
			}

			// Submit audio buffers
			switch( Buffer->SoundFormat )
			{
				case SoundFormat_PCM:
				case SoundFormat_PCMPreview:
					SubmitPCMBuffers();
					break;
				
				case SoundFormat_PCMRT:
					SubmitPCMRTBuffers();
					break;
			}

			// Initialization succeeded.
			return( true );
		}
	}
	
	// Initialization failed.
	return false;
}

/**
 * Updates the source specific parameter like e.g. volume and pitch based on the associated
 * wave instance.	
 */
void FCoreAudioSoundSource::Update( void )
{
	SCOPE_CYCLE_COUNTER( STAT_AudioUpdateSources );

	if( !WaveInstance || Paused || !AudioChannel )
	{	
		return;
	}

	float Volume = 0.0f;
	
	if (!AudioDevice->bIsDeviceMuted)
	{
		Volume = WaveInstance->GetActualVolume();
	}
	
	if( Buffer->NumChannels < 3 )
	{
		float Azimuth = 0.0f;
		float Elevation = 0.0f;

		if( SetStereoBleed() )
		{
			// Emulate the bleed to rear speakers followed by stereo fold down
			Volume *= 1.25f;
		}
		
		// apply global multiplier (ie to disable sound when not the foreground app)
		Volume *= FApp::GetVolumeMultiplier();
		Volume = FMath::Clamp<float>( Volume, 0.0f, MAX_VOLUME );
		
		// Convert to dB
		Volume = 20.0 * log10(Volume);
		Volume = FMath::Clamp<float>( Volume, -120.0f, 20.0f );

		const float Pitch = FMath::Clamp<float>( WaveInstance->Pitch, MIN_PITCH, MAX_PITCH );
		
		// Set the HighFrequencyGain value
		SetHighFrequencyGain();
		
		if( WaveInstance->bApplyRadioFilter )
		{
			Volume = WaveInstance->RadioFilterVolume;
		}
		else if( WaveInstance->bUseSpatialization )
		{
			FVector Direction = AudioDevice->InverseTransform.TransformPosition(WaveInstance->Location).GetSafeNormal();

			FVector EmitterPosition;
			EmitterPosition.X = -Direction.Z;
			EmitterPosition.Y = Direction.Y;
			EmitterPosition.Z = Direction.X;

			FRotator Rotation = EmitterPosition.Rotation();
			Azimuth = Rotation.Yaw;
			Elevation = Rotation.Pitch;
		}

		// Apply any debug settings
		{
			bool bMuteDry, bMuteReverb, bMuteRadio;
			switch( AudioDevice->GetMixDebugState() )
			{
			case DEBUGSTATE_IsolateReverb:		bMuteDry = true;	bMuteReverb = false;	bMuteRadio = false;	break;
			case DEBUGSTATE_IsolateDryAudio:	bMuteDry = false;	bMuteReverb = true;		bMuteRadio = true;	break;
			default:							bMuteDry = false;	bMuteReverb = false;	bMuteRadio = false;	break;
			};

			// Dry audio or EQ node
			if( bMuteDry != bDryMuted )
			{
				if( bMuteDry )
				{
					if( StreamSplitterUnit )
					{
						for( int32 OutputChannelIndex = 0; OutputChannelIndex < AudioDevice->Mixer3DFormat.mChannelsPerFrame; ++OutputChannelIndex )
						{
							SAFE_CA_CALL( AudioUnitSetParameter( StreamSplitterUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Output, OutputChannelIndex, 0.0, 0 ) );
						}
					}
					else
					{
						SAFE_CA_CALL( AudioUnitSetParameter( AudioDevice->GetMixer3DUnit(), k3DMixerParam_Gain, kAudioUnitScope_Input, MixerInputNumber, 0.0, 0 ) );
					}
					bDryMuted = true;
				}
				else
				{
					if( StreamSplitterUnit )
					{
						for( int32 OutputChannelIndex = 0; OutputChannelIndex < AudioDevice->Mixer3DFormat.mChannelsPerFrame; ++OutputChannelIndex )
						{
							SAFE_CA_CALL( AudioUnitSetParameter( StreamSplitterUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Output, OutputChannelIndex, 1.0, 0 ) );
						}
					}
					bDryMuted = false;
				}
			}

			if( ReverbNode && bMuteReverb != bReverbMuted )
			{
				int ReverbNodeBaseIndex = AudioDevice->Mixer3DFormat.mChannelsPerFrame;
				if( bMuteReverb )
				{
					for( int32 OutputChannelIndex = 0; OutputChannelIndex < AudioDevice->Mixer3DFormat.mChannelsPerFrame; ++OutputChannelIndex )
					{
						SAFE_CA_CALL( AudioUnitSetParameter( StreamSplitterUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Output, ReverbNodeBaseIndex + OutputChannelIndex, 0.0, 0 ) );
					}
					bReverbMuted = true;
				}
				else
				{
					for( int32 OutputChannelIndex = 0; OutputChannelIndex < AudioDevice->Mixer3DFormat.mChannelsPerFrame; ++OutputChannelIndex )
					{
						SAFE_CA_CALL( AudioUnitSetParameter( StreamSplitterUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Output, ReverbNodeBaseIndex + OutputChannelIndex, 1.0, 0 ) );
					}
					bReverbMuted = false;
				}
			}

			if( RadioNode && bMuteRadio != bRadioMuted )
			{
				int RadioNodeBaseIndex = ( 1 + ( ReverbNode ? 1 : 0 ) ) * AudioDevice->Mixer3DFormat.mChannelsPerFrame;
				if( bMuteRadio )
				{
					for( int32 OutputChannelIndex = 0; OutputChannelIndex < AudioDevice->Mixer3DFormat.mChannelsPerFrame; ++OutputChannelIndex )
					{
						SAFE_CA_CALL( AudioUnitSetParameter( StreamSplitterUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Output, RadioNodeBaseIndex + OutputChannelIndex, 0.0, 0 ) );
					}
					bRadioMuted = true;
				}
				else
				{
					for( int32 OutputChannelIndex = 0; OutputChannelIndex < AudioDevice->Mixer3DFormat.mChannelsPerFrame; ++OutputChannelIndex )
					{
						SAFE_CA_CALL( AudioUnitSetParameter( StreamSplitterUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Output, RadioNodeBaseIndex + OutputChannelIndex, 1.0, 0 ) );
					}
					bRadioMuted = false;
				}
			}
		}

		if( !bDryMuted || StreamSplitterUnit )
		{
			SAFE_CA_CALL( AudioUnitSetParameter( AudioDevice->GetMixer3DUnit(), k3DMixerParam_Gain, kAudioUnitScope_Input, MixerInputNumber, Volume, 0 ) );
		}

		SAFE_CA_CALL( AudioUnitSetParameter( AudioDevice->GetMixer3DUnit(), k3DMixerParam_PlaybackRate, kAudioUnitScope_Input, MixerInputNumber, Pitch, 0 ) );
		SAFE_CA_CALL( AudioUnitSetParameter( AudioDevice->GetMixer3DUnit(), k3DMixerParam_Azimuth, kAudioUnitScope_Input, MixerInputNumber, Azimuth, 0 ) );
		SAFE_CA_CALL( AudioUnitSetParameter( AudioDevice->GetMixer3DUnit(), k3DMixerParam_Elevation, kAudioUnitScope_Input, MixerInputNumber, Elevation, 0 ) );
	}
	else
	{
		// apply global multiplier (ie to disable sound when not the foreground app)
		Volume *= FApp::GetVolumeMultiplier();
		Volume = FMath::Clamp<float>( Volume, 0.0f, MAX_VOLUME );
		
		if( AudioDevice->GetMixDebugState() == DEBUGSTATE_IsolateReverb )
		{
			Volume = 0.0f;
		}

		AudioDevice->SetMatrixMixerInputVolume( MixerInputNumber, Volume );
	}
}

/**
 * Plays the current wave instance.	
 */
void FCoreAudioSoundSource::Play( void )
{
	if( WaveInstance )
	{
		if( AttachToAUGraph() )
		{
			Paused = false;
			Playing = true;

			// Updates the source which e.g. sets the pitch and volume.
			Update();
		}
	}
}

/**
 * Stops the current wave instance and detaches it from the source.	
 */
void FCoreAudioSoundSource::Stop( void )
{
	if( WaveInstance )
	{	
		if( Playing && AudioChannel )
		{
			DetachFromAUGraph();

			// Free resources
			FreeResources();
		}

		Paused = false;
		Playing = false;
		Buffer = NULL;
		bBuffersToFlush = false;
	}

	FSoundSource::Stop();
}

/**
 * Pauses playback of current wave instance.
 */
void FCoreAudioSoundSource::Pause( void )
{
	if( WaveInstance )
	{	
		if( Playing && AudioChannel )
		{
			DetachFromAUGraph();
		}

		Paused = true;
	}
}

/**
 * Handles feeding new data to a real time decompressed sound
 */
void FCoreAudioSoundSource::HandleRealTimeSource( void )
{
	// Get the next bit of streaming data
	const bool bLooped = ReadMorePCMData(1 - BufferInUse);

	// Have we reached the end of the compressed sound?
	if( bLooped )
	{
		switch( WaveInstance->LoopingMode )
		{
			case LOOP_Never:
				// Play out any queued buffers - once there are no buffers left, the state check at the beginning of IsFinished will fire
				bBuffersToFlush = true;
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
bool FCoreAudioSoundSource::IsFinished( void )
{
	// A paused source is not finished.
	if( Paused )
	{
		return( false );
	}
	
	if( WaveInstance )
	{
		// If not rendering, we're either at the end of a sound, or starved
		if( NumActiveBuffers == 0 )
		{
			// ... are we expecting the sound to be finishing?
			if( bBuffersToFlush || !bStreamedSound )
			{
				// ... notify the wave instance that it has finished playing.
				WaveInstance->NotifyFinished();
				return( true );
			}
		}

		// Service any real time sounds
		if( bStreamedSound )
		{
			if( NumActiveBuffers < 2 )
			{
				// Continue feeding new sound data (unless we are waiting for the sound to finish)
				if( !bBuffersToFlush )
				{
					HandleRealTimeSource();
				}

				return( false );
			}
		}

		return( false );
	}

	return( true );
}

OSStatus FCoreAudioSoundSource::CreateAudioUnit( OSType Type, OSType SubType, OSType Manufacturer, AudioStreamBasicDescription* InputFormat, AudioStreamBasicDescription* OutputFormat, AUNode* OutNode, AudioUnit* OutUnit )
{
	AudioComponentDescription Desc;
	Desc.componentFlags = 0;
	Desc.componentFlagsMask = 0;
	Desc.componentType = Type;
	Desc.componentSubType = SubType;
	Desc.componentManufacturer = Manufacturer;

	OSStatus Status = AUGraphAddNode( AudioDevice->GetAudioUnitGraph(), &Desc, OutNode );
	if( Status == noErr )
	{
		Status = AUGraphNodeInfo( AudioDevice->GetAudioUnitGraph(), *OutNode, NULL, OutUnit );
	}

	if( Status == noErr )
	{
		if( InputFormat )
		{
			Status = AudioUnitSetProperty( *OutUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, InputFormat, sizeof( AudioStreamBasicDescription ) );
		}
		if( Status == noErr )
		{
			if( OutputFormat )
			{
				Status = AudioUnitSetProperty( *OutUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, OutputFormat, sizeof( AudioStreamBasicDescription ) );
			}
		}
	}

	return Status;
}

OSStatus FCoreAudioSoundSource::ConnectAudioUnit( AUNode DestNode, uint32 DestInputNumber, AUNode OutNode, AudioUnit OutUnit )
{
	OSStatus Status = AudioUnitInitialize( OutUnit );
	if( Status == noErr )
	{
		Status = AUGraphConnectNodeInput( AudioDevice->GetAudioUnitGraph(), OutNode, 0, DestNode, DestInputNumber );
	}

	return Status;
}

OSStatus FCoreAudioSoundSource::CreateAndConnectAudioUnit( OSType Type, OSType SubType, OSType Manufacturer, AUNode DestNode, uint32 DestInputNumber, AudioStreamBasicDescription* InputFormat, AudioStreamBasicDescription* OutputFormat, AUNode* OutNode, AudioUnit* OutUnit )
{
	OSStatus Status = CreateAudioUnit( Type, SubType, Manufacturer, InputFormat, OutputFormat, OutNode, OutUnit );

	if( Status == noErr )
	{
		Status = ConnectAudioUnit( DestNode, DestInputNumber, *OutNode, *OutUnit );
	}
	
	return Status;
}

bool FCoreAudioSoundSource::AttachToAUGraph()
{
	AudioChannel = FindFreeAudioChannel();

	OSStatus ErrorStatus = noErr;
	AUNode DestNode = -1;
	int32 DestInputNumber = 0;
	AudioStreamBasicDescription* StreamFormat = NULL;
	
	if( Buffer->NumChannels < 3 )
	{
		ErrorStatus = AudioConverterNew( &Buffer->PCMFormat, &AudioDevice->Mixer3DFormat, &CoreAudioConverter );
		DestNode = AudioDevice->GetMixer3DNode();
		MixerInputNumber = DestInputNumber = AudioDevice->GetFreeMixer3DInput();
		
		uint32 SpatialSetting = ( Buffer->NumChannels == 1 ) ? kSpatializationAlgorithm_SoundField : kSpatializationAlgorithm_StereoPassThrough;
		AudioUnitSetProperty( AudioDevice->GetMixer3DUnit(), kAudioUnitProperty_SpatializationAlgorithm, kAudioUnitScope_Input, MixerInputNumber, &SpatialSetting, sizeof( SpatialSetting ) );
		AudioUnitSetParameter( AudioDevice->GetMixer3DUnit(), k3DMixerParam_Distance, kAudioUnitScope_Input, MixerInputNumber, 1.0f, 0 );

		StreamFormat = &AudioDevice->Mixer3DFormat;
	}
	else
	{
		MixerInputNumber = DestInputNumber = AudioDevice->GetFreeMatrixMixerInput();
		DestNode = AudioDevice->GetMatrixMixerNode();
		StreamFormat = &AudioDevice->MatrixMixerInputFormat;
		
		ErrorStatus = AudioConverterNew( &Buffer->PCMFormat, &AudioDevice->MatrixMixerInputFormat, &CoreAudioConverter );

		bool bIs6ChannelOGG = Buffer->NumChannels == 6
							&& ((Buffer->DecompressionState && Buffer->DecompressionState->UsesVorbisChannelOrdering())
							|| WaveInstance->WaveData->bDecompressedFromOgg);

		AudioDevice->SetupMatrixMixerInput( DestInputNumber, bIs6ChannelOGG );
	}

	if( ErrorStatus != noErr )
	{
		UE_LOG(LogCoreAudio, Warning, TEXT("CoreAudioConverter creation failed, error code %d"), ErrorStatus);
	}

	int FiltersNeeded = 0;
#if EQ_ENABLED
	bool bNeedEQFilter = IsEQFilterApplied();
	if( bNeedEQFilter )
	{
		++FiltersNeeded;
	}
#else
	bool bNeedEQFilter = false;
#endif

#if RADIO_ENABLED
	bool bNeedRadioFilter = Effects->bRadioAvailable && WaveInstance->bApplyRadioFilter;
	if( bNeedRadioFilter )
	{
		++FiltersNeeded;
	}
#else
	bool bNeedRadioFilter = false;
#endif

#if REVERB_ENABLED
	bool bNeedReverbFilter = bReverbApplied;
	if( bNeedReverbFilter )
	{
		++FiltersNeeded;
	}
#else
	bool bNeedReverbFilter = false;
#endif

	if( FiltersNeeded > 0 )
	{
		uint32 BusCount = FiltersNeeded + ( bNeedEQFilter ? 0 : 1 );	// one for each filter, plus one for dry voice if there's no EQ filter

		// Prepare Voice Merger
		SAFE_CA_CALL( CreateAudioUnit( kAudioUnitType_Mixer, kAudioUnitSubType_MatrixMixer, kAudioUnitManufacturer_Apple, NULL, NULL, &StreamMergerNode, &StreamMergerUnit ) );

		// Set Bus Counts
		uint32 NumBuses = BusCount;
		SAFE_CA_CALL( AudioUnitSetProperty( StreamMergerUnit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &NumBuses, sizeof(uint32) ) );
		NumBuses = 1;
		SAFE_CA_CALL( AudioUnitSetProperty(	StreamMergerUnit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Output, 0, &NumBuses, sizeof(uint32) ) );

		// Set Input Formats
		for( int32 InputIndex = 0; InputIndex < BusCount; ++InputIndex )
		{
			SAFE_CA_CALL( AudioUnitSetProperty( StreamMergerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, InputIndex, StreamFormat, sizeof( AudioStreamBasicDescription ) ) );
		}

		// Set Output Format
		SAFE_CA_CALL( AudioUnitSetProperty( StreamMergerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, StreamFormat, sizeof( AudioStreamBasicDescription ) ) );

		SAFE_CA_CALL( AudioUnitInitialize( StreamMergerUnit ) );

		// Set Master volume
		SAFE_CA_CALL( AudioUnitSetParameter( StreamMergerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, 0xFFFFFFFF, 1.0, 0 ) );

		// Enable Output
		SAFE_CA_CALL( AudioUnitSetParameter( StreamMergerUnit, kMatrixMixerParam_Enable, kAudioUnitScope_Output, 0, 1.0, 0 ) );

		// Set Output volumes
		for( int32 ChannelIndex = 0; ChannelIndex < StreamFormat->mChannelsPerFrame; ++ChannelIndex )
		{
			SAFE_CA_CALL( AudioUnitSetParameter( StreamMergerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Output, ChannelIndex, 1.0, 0 ) );
		}

		for( int32 InputIndex = 0; InputIndex < BusCount; ++InputIndex )
		{
			// Enable Input
			SAFE_CA_CALL( AudioUnitSetParameter( StreamMergerUnit, kMatrixMixerParam_Enable, kAudioUnitScope_Input, InputIndex, 1.0, 0 ) );
		}

		for( int32 ChannelIndex = 0; ChannelIndex < StreamFormat->mChannelsPerFrame; ++ChannelIndex )
		{
			for( int32 InputBusIndex = 0; InputBusIndex < BusCount; ++InputBusIndex )
			{
				int32 InputChannelIndex = InputBusIndex*StreamFormat->mChannelsPerFrame + ChannelIndex;

				// Set Input Channel Volume
				SAFE_CA_CALL( AudioUnitSetParameter( StreamMergerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Input, InputChannelIndex, 1.0, 0 ) );

				// Set Crossfade Volume - each input channel goes to specific output channel. The rest of connections is left at zero.
				SAFE_CA_CALL( AudioUnitSetParameter( StreamMergerUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, ( InputChannelIndex << 16 ) | ChannelIndex, 1.0, 0 ) );
			}
		}

		SAFE_CA_CALL( AUGraphConnectNodeInput( AudioDevice->GetAudioUnitGraph(), StreamMergerNode, 0, DestNode, DestInputNumber ) );

		DestNode = StreamMergerNode;
		DestInputNumber = 0;

		// Prepare and initialize stream splitter
		SAFE_CA_CALL( CreateAudioUnit( kAudioUnitType_Mixer, kAudioUnitSubType_MatrixMixer, kAudioUnitManufacturer_Apple, NULL, NULL, &StreamSplitterNode, &StreamSplitterUnit ) );

		// Set bus counts
		NumBuses = 1;
		SAFE_CA_CALL( AudioUnitSetProperty( StreamSplitterUnit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &NumBuses, sizeof(uint32) ) );
		NumBuses = BusCount;
		SAFE_CA_CALL( AudioUnitSetProperty(	StreamSplitterUnit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Output, 0, &NumBuses, sizeof(uint32) ) );

		// Set Input format
		SAFE_CA_CALL( AudioUnitSetProperty( StreamSplitterUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, StreamFormat, sizeof( AudioStreamBasicDescription ) ) );

		// Set Output formats
		for( int32 OutputIndex = 0; OutputIndex < BusCount; ++OutputIndex )
		{
			SAFE_CA_CALL( AudioUnitSetProperty( StreamSplitterUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, OutputIndex, StreamFormat, sizeof( AudioStreamBasicDescription ) ) );
		}

		SAFE_CA_CALL( AudioUnitInitialize( StreamSplitterUnit ) );

		// Set Master volume
		SAFE_CA_CALL( AudioUnitSetParameter( StreamSplitterUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, 0xFFFFFFFF, 1.0, 0 ) );

		// Enable Input
		SAFE_CA_CALL( AudioUnitSetParameter( StreamSplitterUnit, kMatrixMixerParam_Enable, kAudioUnitScope_Input, 0, 1.0, 0 ) );

		// Set Input Volumes
		for( int32 ChannelIndex = 0; ChannelIndex < StreamFormat->mChannelsPerFrame; ++ChannelIndex )
		{
			SAFE_CA_CALL( AudioUnitSetParameter( StreamSplitterUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Input, ChannelIndex, 1.0, 0 ) );
		}

		for( int32 OutputIndex = 0; OutputIndex < BusCount; ++OutputIndex )
		{
			// Enable Output
			SAFE_CA_CALL( AudioUnitSetParameter( StreamSplitterUnit, kMatrixMixerParam_Enable, kAudioUnitScope_Output, OutputIndex, 1.0, 0 ) );
		}

		for( int32 ChannelIndex = 0; ChannelIndex < StreamFormat->mChannelsPerFrame; ++ChannelIndex )
		{
			for( int32 OutputBusIndex = 0; OutputBusIndex < BusCount; ++OutputBusIndex )
			{
				int32 OutputChannelIndex = OutputBusIndex*StreamFormat->mChannelsPerFrame + ChannelIndex;

				// Set Output Channel Volume
				SAFE_CA_CALL( AudioUnitSetParameter( StreamSplitterUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Output, OutputChannelIndex, 1.0, 0 ) );

				// Set Crossfade Volume - each output channel goes from specific input channel. The rest of connections is left at zero.
				SAFE_CA_CALL( AudioUnitSetParameter( StreamSplitterUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Global, ( ChannelIndex << 16 ) | OutputChannelIndex, 1.0, 0 ) );
			}
		}

		// Prepare and connect appropriate filters
#if EQ_ENABLED
		if( bNeedEQFilter )
		{
			SAFE_CA_CALL( CreateAndConnectAudioUnit( kAudioUnitType_Effect, kAudioUnitSubType_AUFilter, kAudioUnitManufacturer_Apple, DestNode, DestInputNumber, StreamFormat, StreamFormat, &EQNode, &EQUnit ) );
			SAFE_CA_CALL( AUGraphConnectNodeInput( AudioDevice->GetAudioUnitGraph(), StreamSplitterNode, DestInputNumber, EQNode, 0 ) );
		}
		else
#endif
		{
			// Add direct connection between stream splitter and stream merger, for dry voice
			SAFE_CA_CALL( AUGraphConnectNodeInput( AudioDevice->GetAudioUnitGraph(), StreamSplitterNode, 0, StreamMergerNode, 0 ) );

			// Silencing dry voice (for testing)
//			for( int32 ChannelIndex = 0; ChannelIndex < StreamFormat->mChannelsPerFrame; ++ChannelIndex )
//			{
//				SAFE_CA_CALL( AudioUnitSetParameter( StreamSplitterUnit, kMatrixMixerParam_Volume, kAudioUnitScope_Output, ChannelIndex, 0.0, 0 ) );
//			}
//			SAFE_CA_CALL( AudioUnitSetParameter( StreamSplitterUnit, kMatrixMixerParam_Enable, kAudioUnitScope_Output, 0, 0.0, 0 ) );
		}
		++DestInputNumber;

#if RADIO_ENABLED
		if( bNeedRadioFilter )
		{
			SAFE_CA_CALL( CreateAndConnectAudioUnit( kAudioUnitType_Effect, 'Rdio', 'Epic', DestNode, DestInputNumber, StreamFormat, StreamFormat, &RadioNode, &RadioUnit ) );
			SAFE_CA_CALL( AUGraphConnectNodeInput( AudioDevice->GetAudioUnitGraph(), StreamSplitterNode, DestInputNumber, RadioNode, 0 ) );
			++DestInputNumber;
		}
#endif
#if REVERB_ENABLED
		if( bNeedReverbFilter )
		{
			SAFE_CA_CALL( CreateAndConnectAudioUnit( kAudioUnitType_Effect, kAudioUnitSubType_MatrixReverb, kAudioUnitManufacturer_Apple, DestNode, DestInputNumber, StreamFormat, StreamFormat, &ReverbNode, &ReverbUnit ) );
			SAFE_CA_CALL( AUGraphConnectNodeInput( AudioDevice->GetAudioUnitGraph(), StreamSplitterNode, DestInputNumber, ReverbNode, 0 ) );
			++DestInputNumber;
		}
#endif

		DestNode = StreamSplitterNode;
		DestInputNumber = 0;
	}

	SAFE_CA_CALL( CreateAndConnectAudioUnit( kAudioUnitType_FormatConverter, kAudioUnitSubType_AUConverter, kAudioUnitManufacturer_Apple, DestNode, DestInputNumber, StreamFormat, StreamFormat, &SourceNode, &SourceUnit ) );

	if( ErrorStatus == noErr )
	{
		AURenderCallbackStruct Input;
		Input.inputProc = &CoreAudioRenderCallback;
		Input.inputProcRefCon = this;
		SAFE_CA_CALL( AudioUnitSetProperty( SourceUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &Input, sizeof( Input ) ) );

		SAFE_CA_CALL( AUGraphUpdate( AudioDevice->GetAudioUnitGraph(), NULL ) );

		GAudioChannels[AudioChannel] = this;
	}
	return ErrorStatus == noErr;
}

bool FCoreAudioSoundSource::DetachFromAUGraph()
{
	AURenderCallbackStruct Input;
	Input.inputProc = NULL;
	Input.inputProcRefCon = NULL;
	SAFE_CA_CALL( AudioUnitSetProperty( SourceUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &Input, sizeof( Input ) ) );

	if( StreamSplitterNode )
	{
		SAFE_CA_CALL( AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), StreamSplitterNode, 0 ) );
	}
	if( ReverbNode )
	{
		SAFE_CA_CALL( AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), ReverbNode, 0 ) );
	}
	if( RadioNode )
	{
		SAFE_CA_CALL( AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), RadioNode, 0 ) );
	}
	if( EQNode )
	{
		SAFE_CA_CALL( AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), EQNode, 0 ) );
	}
	if( StreamMergerNode )
	{
		SAFE_CA_CALL( AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), StreamMergerNode, 0 ) );
	}

	if( AudioChannel )
	{
		if( Buffer->NumChannels < 3 )
		{
			SAFE_CA_CALL( AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), AudioDevice->GetMixer3DNode(), MixerInputNumber ) );
			AudioDevice->SetFreeMixer3DInput( MixerInputNumber );
		}
		else
		{
			SAFE_CA_CALL( AUGraphDisconnectNodeInput( AudioDevice->GetAudioUnitGraph(), AudioDevice->GetMatrixMixerNode(), MixerInputNumber ) );
			AudioDevice->SetFreeMatrixMixerInput( MixerInputNumber );
		}
	}

	if( StreamMergerNode )
	{
		SAFE_CA_CALL( AUGraphRemoveNode( AudioDevice->GetAudioUnitGraph(), StreamMergerNode ) );
	}
	if( EQNode )
	{
		SAFE_CA_CALL( AUGraphRemoveNode( AudioDevice->GetAudioUnitGraph(), EQNode ) );
	}
	if( RadioNode )
	{
		SAFE_CA_CALL( AUGraphRemoveNode( AudioDevice->GetAudioUnitGraph(), RadioNode ) );
	}
	if( ReverbNode )
	{
		SAFE_CA_CALL( AUGraphRemoveNode( AudioDevice->GetAudioUnitGraph(), ReverbNode ) );
	}
	if( StreamSplitterNode )
	{
		SAFE_CA_CALL( AUGraphRemoveNode( AudioDevice->GetAudioUnitGraph(), StreamSplitterNode ) );
	}
	if( AudioChannel )
	{
		SAFE_CA_CALL( AUGraphRemoveNode( AudioDevice->GetAudioUnitGraph(), SourceNode ) );
	}

	SAFE_CA_CALL( AUGraphUpdate( AudioDevice->GetAudioUnitGraph(), NULL ) );

	AudioConverterDispose( CoreAudioConverter );
	CoreAudioConverter = NULL;

	StreamMergerNode = 0;
	StreamMergerUnit = NULL;
	EQNode = 0;
	EQUnit = NULL;
	RadioNode = 0;
	RadioUnit = NULL;
	ReverbNode = 0;
	ReverbUnit = NULL;
	StreamSplitterNode = 0;
	StreamSplitterUnit = NULL;
	SourceNode = 0;
	SourceUnit = NULL;
	MixerInputNumber = -1;
	
	GAudioChannels[AudioChannel] = NULL;
	AudioChannel = 0;
	
	return true;
}


OSStatus FCoreAudioSoundSource::CoreAudioRenderCallback( void *InRefCon, AudioUnitRenderActionFlags *IOActionFlags,
														const AudioTimeStamp *InTimeStamp, UInt32 InBusNumber,
														UInt32 InNumberFrames, AudioBufferList *IOData )
{
	OSStatus Status = noErr;
	FCoreAudioSoundSource *Source = ( FCoreAudioSoundSource *)InRefCon;

	uint32 DataByteSize = InNumberFrames * sizeof( Float32 );
	uint32 PacketsRequested = InNumberFrames;
	uint32 PacketsObtained = 0;

	// AudioBufferList itself holds only one buffer, while AudioConverterFillComplexBuffer expects a couple of them
	struct
	{
		AudioBufferList BufferList;		
		AudioBuffer		AdditionalBuffers[5];
	} LocalBuffers;

	AudioBufferList *LocalBufferList = &LocalBuffers.BufferList;
	LocalBufferList->mNumberBuffers = IOData->mNumberBuffers;

	if( Source->Buffer && Source->Playing )
	{
		while( PacketsObtained < PacketsRequested )
		{
			int32 BufferFilledBytes = PacketsObtained * sizeof( Float32 );
			for( uint32 Index = 0; Index < LocalBufferList->mNumberBuffers; Index++ )
			{
				LocalBufferList->mBuffers[Index].mDataByteSize = DataByteSize - BufferFilledBytes;
				LocalBufferList->mBuffers[Index].mData = ( uint8 *)IOData->mBuffers[Index].mData + BufferFilledBytes;
			}

			uint32 PacketCount = PacketsRequested - PacketsObtained;
			Status = AudioConverterFillComplexBuffer( Source->CoreAudioConverter, &CoreAudioConvertCallback, InRefCon, &PacketCount, LocalBufferList, NULL );
			PacketsObtained += PacketCount;

			if( PacketCount == 0 || Status != noErr )
			{
				AudioConverterReset( Source->CoreAudioConverter );
				break;
			}
		}

		if( PacketsObtained == 0 )
		{
			*IOActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
		}
	}
	else
	{
		*IOActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
	}

	if( PacketsObtained < PacketsRequested )
	{
		// Fill the rest of buffers provided with zeroes
		int32 BufferFilledBytes = PacketsObtained * sizeof( Float32 );
		for( uint32 Index = 0; Index < IOData->mNumberBuffers; ++Index )
		{
			FMemory::Memzero( ( uint8 *)IOData->mBuffers[Index].mData + BufferFilledBytes, DataByteSize - BufferFilledBytes );
		}
	}
	
	return Status;
}

OSStatus FCoreAudioSoundSource::CoreAudioConvertCallback( AudioConverterRef Converter, UInt32 *IONumberDataPackets, AudioBufferList *IOData,
														 AudioStreamPacketDescription **OutPacketDescription, void *InUserData )
{
	FCoreAudioSoundSource *Source = ( FCoreAudioSoundSource *)InUserData;

	uint8 *Buffer = Source->CoreAudioBuffers[Source->BufferInUse].AudioData;
	int32 BufferSize = Source->CoreAudioBuffers[Source->BufferInUse].AudioDataSize;
	int32 ReadCursor = Source->CoreAudioBuffers[Source->BufferInUse].ReadCursor;

	int32 PacketsAvailable = Source->Buffer ? ( BufferSize - ReadCursor ) / Source->Buffer->PCMFormat.mBytesPerPacket : 0;
	if( PacketsAvailable < *IONumberDataPackets )
	{
		*IONumberDataPackets = PacketsAvailable;
	}

	IOData->mBuffers[0].mData = *IONumberDataPackets ? Buffer + ReadCursor : NULL;
	IOData->mBuffers[0].mDataByteSize = Source->Buffer ? *IONumberDataPackets * Source->Buffer->PCMFormat.mBytesPerPacket : 0;
	ReadCursor += IOData->mBuffers[0].mDataByteSize;
	Source->CoreAudioBuffers[Source->BufferInUse].ReadCursor = ReadCursor;

	if( ReadCursor == BufferSize && Source->NumActiveBuffers )
	{
		if( Source->bStreamedSound )
		{
			Source->NumActiveBuffers--;
			Source->BufferInUse = 1 - Source->BufferInUse;
		}
		else if( Source->WaveInstance )
		{
			switch( Source->WaveInstance->LoopingMode )
			{
			case LOOP_Never:
				Source->NumActiveBuffers--;
				break;

			case LOOP_WithNotification:
				Source->WaveInstance->NotifyFinished();
				// pass-through

			case LOOP_Forever:
				Source->CoreAudioBuffers[Source->BufferInUse].ReadCursor = 0;	// loop to start
				break;
			}
		}
	}

	return noErr;
}
