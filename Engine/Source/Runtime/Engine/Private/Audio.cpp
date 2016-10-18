// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Audio.cpp: Unreal base audio.
=============================================================================*/

#include "EnginePrivate.h"
#include "ActiveSound.h"
#include "Audio.h"
#include "AudioDevice.h"
#include "AudioThread.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "AudioEffect.h"
#include "Net/UnrealNetwork.h"
#include "TargetPlatform.h"
#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "ContentStreaming.h"
#include "IAudioExtensionPlugin.h"

DEFINE_LOG_CATEGORY(LogAudio);

DEFINE_LOG_CATEGORY(LogAudioDebug);

/** Audio stats */

DEFINE_STAT(STAT_AudioMemorySize);
DEFINE_STAT(STAT_ActiveSounds);
DEFINE_STAT(STAT_AudioSources);
DEFINE_STAT(STAT_WaveInstances);
DEFINE_STAT(STAT_WavesDroppedDueToPriority);
DEFINE_STAT(STAT_AudibleWavesDroppedDueToPriority);
DEFINE_STAT(STAT_AudioFinishedDelegatesCalled);
DEFINE_STAT(STAT_AudioFinishedDelegates);
DEFINE_STAT(STAT_AudioBufferTime);
DEFINE_STAT(STAT_AudioBufferTimeChannels);

DEFINE_STAT(STAT_OggWaveInstances);
DEFINE_STAT(STAT_VorbisDecompressTime);
DEFINE_STAT(STAT_VorbisPrepareDecompressionTime);
DEFINE_STAT(STAT_AudioDecompressTime);
DEFINE_STAT(STAT_AudioPrepareDecompressionTime);
DEFINE_STAT(STAT_OpusDecompressTime);

DEFINE_STAT(STAT_AudioUpdateEffects);
DEFINE_STAT(STAT_AudioEvaluateConcurrency);
DEFINE_STAT(STAT_AudioUpdateSources);
DEFINE_STAT(STAT_AudioResourceCreationTime);
DEFINE_STAT(STAT_AudioSourceInitTime);
DEFINE_STAT(STAT_AudioSourceCreateTime);
DEFINE_STAT(STAT_AudioSubmitBuffersTime);
DEFINE_STAT(STAT_AudioStartSources);
DEFINE_STAT(STAT_AudioGatherWaveInstances);
DEFINE_STAT(STAT_AudioFindNearestLocation);


bool IsAudioPluginEnabled(EAudioPlugin::Type PluginType)
{
	if (PluginType == EAudioPlugin::SPATIALIZATION)
	{
		TArray<IAudioSpatializationPlugin *> SpatializationPlugins = IModularFeatures::Get().GetModularFeatureImplementations<IAudioSpatializationPlugin>(IAudioSpatializationPlugin::GetModularFeatureName());
		return SpatializationPlugins.Num() > 0;
	}

	return false;
}


/*-----------------------------------------------------------------------------
	FSoundBuffer implementation.
-----------------------------------------------------------------------------*/

FSoundBuffer::~FSoundBuffer()
{
	// remove ourselves from the set of waves that are tracked by the audio device
	if (ResourceID && GEngine && GEngine->GetAudioDeviceManager())
	{
		GEngine->GetAudioDeviceManager()->RemoveSoundBufferForResourceID(ResourceID);
	}
}

/**
 * This will return the name of the SoundClass of the Sound that this buffer(SoundWave) belongs to.
 * NOTE: This will find the first cue in the ObjectIterator list.  So if we are using SoundWaves in multiple
 * places we will pick up the first one only.
 **/
FName FSoundBuffer::GetSoundClassName()
{
	// need to look in all cues
	for (TObjectIterator<USoundBase> It; It; ++It)
	{
		USoundCue* Cue = Cast<USoundCue>(*It);
		if (Cue)
		{
			// get all the waves this cue uses
			TArray<USoundNodeWavePlayer*> WavePlayers;
			Cue->RecursiveFindNode<USoundNodeWavePlayer>(Cue->FirstNode, WavePlayers);

			// look through them to see if this cue uses a wave this buffer is bound to, via ResourceID
			for (int32 WaveIndex = 0; WaveIndex < WavePlayers.Num(); ++WaveIndex)
			{
				USoundWave* WaveNode = WavePlayers[WaveIndex]->GetSoundWave();
				if (WaveNode != NULL)
				{
					if (WaveNode->ResourceID == ResourceID)
					{
						if (Cue->GetSoundClass())
						{
							return Cue->GetSoundClass()->GetFName();
						}
						else
						{
							return NAME_None;
						}
					}
				}
			}
		}
		else
		{
			USoundWave* Wave = Cast<USoundWave>(*It);
			if (Wave && Wave->ResourceID == ResourceID)
			{
				if (Wave->GetSoundClass())
				{
					return Wave->GetSoundClass()->GetFName();
				}
				else
				{
					return NAME_None;
				}
			}
		}
	}

	return NAME_None;
}

FString FSoundBuffer::GetChannelsDesc()
{
	switch (NumChannels)
	{
		case 1:
			return FString("Mono");
		case 2:
			return FString("Stereo");
		case 6:
			return FString("5.1");
		case 8:
			return FString("7.1");
		default:
			return FString::Printf(TEXT("%d Channels"), NumChannels);
	}
}

FString FSoundBuffer::Describe(bool bUseLongName)
{
	// format info string
	const FName SoundClassName = GetSoundClassName();
	FString AllocationString = bAllocationInPermanentPool ? TEXT("Permanent, ") : TEXT("");
	FString ChannelsDesc = GetChannelsDesc();
	FString SoundName = bUseLongName ? ResourceName : FPaths::GetExtension(ResourceName);

	return FString::Printf(TEXT("%8.2fkb, %s%s, '%s', Class: %s"), GetSize() / 1024.0f, *AllocationString, *ChannelsDesc, *ResourceName, *SoundClassName.ToString());
}

/*-----------------------------------------------------------------------------
	FSoundSource implementation.
-----------------------------------------------------------------------------*/

FString FSoundSource::Describe(bool bUseLongName)
{
	return FString::Printf(TEXT("Wave: %s, Volume: %6.2f, Owner: %s"), 
		bUseLongName ? *WaveInstance->WaveData->GetPathName() : *WaveInstance->WaveData->GetName(),
		WaveInstance->GetActualVolume(), 
		WaveInstance->ActiveSound ? *WaveInstance->ActiveSound->GetOwnerName() : TEXT("None"));
}

void FSoundSource::Stop( void )
{
	IStreamingManager::Get().GetAudioStreamingManager().RemoveStreamingSoundSource(this);
	if( WaveInstance )
	{
		check( AudioDevice );
		AudioDevice->FreeSources.AddUnique( this );
		AudioDevice->WaveInstanceSourceMap.Remove( WaveInstance );
		WaveInstance->NotifyFinished(true);
		WaveInstance = NULL;
	}
	else
	{
		check( AudioDevice->FreeSources.Find( this ) != INDEX_NONE );
	}
}

bool FSoundSource::IsGameOnly( void )
{
	return (WaveInstance && !WaveInstance->bIsUISound);
}

bool FSoundSource::SetReverbApplied( bool bHardwareAvailable )
{
	// Do not apply reverb if it is explicitly disallowed
	bReverbApplied = WaveInstance->bReverb && bHardwareAvailable;

	// Do not apply reverb to music
	if( WaveInstance->bIsMusic )
	{
		bReverbApplied = false;
	}

	// Do not apply reverb to multichannel sounds
	if( WaveInstance->WaveData->NumChannels > 2 )
	{
		bReverbApplied = false;
	}

	return( bReverbApplied );
}

float FSoundSource::SetStereoBleed( void )
{
	StereoBleed = 0.0f;

	// All stereo sounds bleed by default
	if( WaveInstance->WaveData->NumChannels == 2 )
	{
		StereoBleed = WaveInstance->StereoBleed;

		if( AudioDevice->GetMixDebugState() == DEBUGSTATE_TestStereoBleed )
		{
			StereoBleed = 1.0f;
		}
	}

	return( StereoBleed );
}

float FSoundSource::SetLFEBleed( void )
{
	LFEBleed = WaveInstance->LFEBleed;

	if( AudioDevice->GetMixDebugState() == DEBUGSTATE_TestLFEBleed )
	{
		LFEBleed = 10.0f;
	}

	return( LFEBleed );
}

void FSoundSource::SetFilterFrequency(void)
{
	LPFFrequency = MAX_FILTER_FREQUENCY;

	if (AudioDevice->GetMixDebugState() == DEBUGSTATE_TestLPF)
	{
		// If in debug mode, lets set all sounds to a LPF of MIN_FILTER_FREQUENCY
		LPFFrequency = MIN_FILTER_FREQUENCY;
	}
	else if (AudioDevice->GetMixDebugState() != DEBUGSTATE_DisableLPF)
	{
		// If so, override the frequency with the occluded filter frequency
		LPFFrequency = WaveInstance->OcclusionFilterFrequency;

		// Set the LPFFrequency to the manual LowPassFilterFrequency if it's lower
		if (WaveInstance->bEnableLowPassFilter && WaveInstance->LowPassFilterFrequency < LPFFrequency)
		{
			LPFFrequency = WaveInstance->LowPassFilterFrequency;
		}

		// Set the LPFFrequency to the ambient filter frequency if it's lower
		if (WaveInstance->AmbientZoneFilterFrequency < LPFFrequency)
		{
			LPFFrequency = WaveInstance->AmbientZoneFilterFrequency;
		}

		if (WaveInstance->AttenuationFilterFrequency < LPFFrequency)
		{
			LPFFrequency = WaveInstance->AttenuationFilterFrequency;
		}
	}
}

void FSoundSource::UpdateStereoEmitterPositions()
{
	// Only call this function if we're told to use spatialization
	check(WaveInstance->bUseSpatialization);
	check(Buffer->NumChannels == 2);

	if (WaveInstance->StereoSpread > 0.0f)
	{
		// We need to compute the stereo left/right channel positions using the audio component position and the spread 
		FVector ListenerPosition = AudioDevice->Listeners[0].Transform.GetLocation();
		FVector ListenerToSourceDir = (WaveInstance->Location - ListenerPosition).GetSafeNormal();

		float HalfSpread = 0.5f * WaveInstance->StereoSpread;

		// Get direction of left emitter from true emitter position (left hand rule)
		FVector LeftEmitterDir = FVector::CrossProduct(ListenerToSourceDir, FVector::UpVector);
		FVector LeftEmitterOffset = LeftEmitterDir * HalfSpread;

		// Get position vector of left emitter by adding to true emitter the dir scaled by half the spread
		LeftChannelSourceLocation = WaveInstance->Location + LeftEmitterOffset;

		// Right emitter position is same as right but opposite direction
		RightChannelSourceLocation = WaveInstance->Location - LeftEmitterOffset;
	}
	else
	{
		LeftChannelSourceLocation = WaveInstance->Location;
		RightChannelSourceLocation = WaveInstance->Location;
	}
}

void FSoundSource::DrawDebugInfo()
{
	// Draw 3d Debug information about this source, if enabled
	FAudioDeviceManager* DeviceManager = GEngine->GetAudioDeviceManager();

	if (DeviceManager && DeviceManager->IsVisualizeDebug3dEnabled())
	{
		const uint32 AudioComponentID = WaveInstance->ActiveSound->GetAudioComponentID();

		if (AudioComponentID > 0)
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.DrawSourceDebugInfo"), STAT_AudioDrawSourceDebugInfo, STATGROUP_TaskGraphTasks);

			USoundBase* Sound = WaveInstance->ActiveSound->GetSound();
			const FVector Location = WaveInstance->Location;

			const bool bSpatialized = Buffer->NumChannels == 2 && WaveInstance->bUseSpatialization;
			const FVector LeftChannelSourceLoc = LeftChannelSourceLocation;
			const FVector RightChannelSourceLoc = RightChannelSourceLocation;

			FAudioThread::RunCommandOnGameThread([AudioComponentID, Sound, bSpatialized, Location, LeftChannelSourceLoc, RightChannelSourceLoc]()
			{
				UAudioComponent* AudioComponent = UAudioComponent::GetAudioComponentFromID(AudioComponentID);
				if (AudioComponent)
				{
					UWorld* SoundWorld = AudioComponent->GetWorld();
					if (SoundWorld)
					{
						FRotator SoundRotation = AudioComponent->GetComponentRotation();
						DrawDebugCrosshairs(SoundWorld, Location, SoundRotation, 20.0f, FColor::White, false, -1.0f, SDPG_Foreground);

						if (bSpatialized)
						{
							DrawDebugCrosshairs(SoundWorld, LeftChannelSourceLoc, SoundRotation, 20.0f, FColor::Red, false, -1.0f, SDPG_Foreground);
							DrawDebugCrosshairs(SoundWorld, RightChannelSourceLoc, SoundRotation, 20.0f, FColor::Green, false, -1.0f, SDPG_Foreground);
						}

						const FString Name = Sound->GetName();
						DrawDebugString(SoundWorld, AudioComponent->GetComponentLocation() + FVector(0, 0, 32), *Name, nullptr, FColor::White, 0.033, false);
					}
				}
			}, GET_STATID(STAT_AudioDrawSourceDebugInfo));
		}
	}
}
 
float FSoundSource::GetDebugVolume(const float InVolume)
{
	float OutVolume = InVolume;

#if !UE_BUILD_SHIPPING

	if (OutVolume != 0.0f)
	{

		// Check for solo sound class debuging. Mute all sounds that don't substring match their sound class name to the debug solo'd sound class
		const FString& DebugSoloSoundName = GEngine->GetAudioDeviceManager()->GetDebugSoloSoundWave();
		if (DebugSoloSoundName != TEXT(""))
		{
			bool bMute = true;
			FString WaveInstanceName = WaveInstance->GetName();
			if (WaveInstanceName.Contains(DebugSoloSoundName))
			{
				bMute = false;
			}
			if (bMute)
			{
				OutVolume = 0.0f;
			}
		}
	}

	if (OutVolume != 0.0f)
	{
		// Check for solo sound class debuging. Mute all sounds that don't substring match their sound class name to the debug solo'd sound class
		const FString& DebugSoloSoundCue = GEngine->GetAudioDeviceManager()->GetDebugSoloSoundCue();
		if (DebugSoloSoundCue != TEXT(""))
		{
			bool bMute = true;
			USoundBase* Sound = WaveInstance->ActiveSound->GetSound();
			if (Sound->IsA<USoundCue>())
			{
				FString SoundCueName = Sound->GetName();
				if (SoundCueName.Contains(DebugSoloSoundCue))
				{
					bMute = false;
				}
			}

			if (bMute)
			{
				OutVolume = 0.0f;
			}
		}
	}

	if (OutVolume != 0.0f)
	{
		const FString& DebugSoloSoundClassName = GEngine->GetAudioDeviceManager()->GetDebugSoloSoundClass();
		if (DebugSoloSoundClassName != TEXT(""))
		{
			bool bMute = true;
			if (WaveInstance->SoundClass)
			{
				FString SoundClassName;
				WaveInstance->SoundClass->GetName(SoundClassName);
				if (SoundClassName.Contains(DebugSoloSoundClassName))
				{
					bMute = false;
				}
			}
			if (bMute)
			{
				OutVolume = 0.0f;
			}
		}
	}
#endif

	return OutVolume;
}

FSpatializationParams FSoundSource::GetSpatializationParams()
{
	FSpatializationParams Params;

		// Calculate direction from listener to sound, where the sound is at the origin if unspatialized.
	if (WaveInstance->bUseSpatialization)
	{
		FVector EmitterPosition = AudioDevice->GetListenerTransformedDirection(WaveInstance->Location, &Params.Distance);
		
		// If we are using the OmniRadius feature
		if (WaveInstance->OmniRadius > 0.0f)
		{
			static const float MaxNormalizedRadius = 1000000.0f;
			Params.NormalizedOmniRadius = MaxNormalizedRadius;

			if (Params.Distance > 0)
			{
				Params.NormalizedOmniRadius = FMath::Clamp(WaveInstance->OmniRadius / Params.Distance, 0.0f, MaxNormalizedRadius);
			}
		}
		else
		{
			Params.NormalizedOmniRadius = 0.0f;
		}

		if (Buffer->NumChannels == 2)
		{
			Params.LeftChannelPosition = AudioDevice->GetListenerTransformedDirection(LeftChannelSourceLocation, nullptr);
			Params.RightChannelPosition = AudioDevice->GetListenerTransformedDirection(RightChannelSourceLocation, nullptr);
			Params.EmitterPosition = FVector::ZeroVector;
		}
		else
		{
			Params.EmitterPosition = EmitterPosition;
		}
	}
	else
	{
		Params.NormalizedOmniRadius = 0.0f;
		Params.Distance = 0.0f;
		Params.EmitterPosition = FVector::ZeroVector;
	}

	// We are currently always computing spatialization for XAudio2 relative to the listener!
	Params.ListenerOrientation = FVector::UpVector;
	Params.ListenerPosition = FVector::ZeroVector;

	return Params;
}


/*-----------------------------------------------------------------------------
	FNotifyBufferFinishedHooks implementation.
-----------------------------------------------------------------------------*/

void FNotifyBufferFinishedHooks::AddNotify(USoundNode* NotifyNode, UPTRINT WaveInstanceHash)
{
	Notifies.Add(FNotifyBufferDetails(NotifyNode, WaveInstanceHash));
}

UPTRINT FNotifyBufferFinishedHooks::GetHashForNode(USoundNode* NotifyNode) const
{
	for (const FNotifyBufferDetails& NotifyDetails : Notifies)
	{
		if (NotifyDetails.NotifyNode == NotifyNode)
		{
			return NotifyDetails.NotifyNodeWaveInstanceHash;
		}
	}

	return 0;
}

void FNotifyBufferFinishedHooks::DispatchNotifies(FWaveInstance* WaveInstance, const bool bStopped)
{
	for (int32 NotifyIndex = Notifies.Num() - 1; NotifyIndex >= 0; --NotifyIndex)
	{
		// All nodes get an opportunity to handle the notify if we're forcefully stopping the sound
		if (Notifies[NotifyIndex].NotifyNode->NotifyWaveInstanceFinished(WaveInstance) && !bStopped)
		{
			break;
		}
	}

}

void FNotifyBufferFinishedHooks::AddReferencedObjects( FReferenceCollector& Collector )
{
	for (FNotifyBufferDetails& NotifyDetails : Notifies)
	{
		Collector.AddReferencedObject( NotifyDetails.NotifyNode );
	}
}

FArchive& operator<<( FArchive& Ar, FNotifyBufferFinishedHooks& NotifyHook )
{
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		for (FNotifyBufferFinishedHooks::FNotifyBufferDetails& NotifyDetails : NotifyHook.Notifies)
		{
			Ar << NotifyDetails.NotifyNode;
		}
	}
	return( Ar );
}


/*-----------------------------------------------------------------------------
	FWaveInstance implementation.
-----------------------------------------------------------------------------*/

/** Helper to create good unique type hashs for FWaveInstance instances */
uint32 FWaveInstance::TypeHashCounter = 0;

/**
 * Constructor, initializing all member variables.
 *
 * @param InActiveSound		ActiveSound this wave instance belongs to.
 */
FWaveInstance::FWaveInstance( FActiveSound* InActiveSound )
:	WaveData( NULL )
,   SoundClass( NULL )
,	ActiveSound( InActiveSound )
,	Volume( 0.0f )
,	VolumeMultiplier( 1.0f )
,	Priority( 1.0f )
,	VoiceCenterChannelVolume( 0.0f )
,	RadioFilterVolume( 0.0f )
,	RadioFilterVolumeThreshold( 0.0f )
,	StereoBleed( 0.0f )
,	LFEBleed( 0.0f )
,	LoopingMode( LOOP_Never )
,	StartTime( -1.f )
,	bApplyRadioFilter( false )
,	bIsStarted(false)
,	bIsFinished( false )
,	bAlreadyNotifiedHook( false )
,	bUseSpatialization( false )
,	bEnableLowPassFilter(false)
,	bIsOccluded(false)
,	bEQFilterApplied(false)
,	bIsUISound( false )
,	bIsMusic( false )
,	bReverb( true )
,	bCenterChannelOnly( false )
,	bReportedSpatializationWarning( false )
,	SpatializationAlgorithm(SPATIALIZATION_Default)
,	OutputTarget(EAudioOutputTarget::Speaker)
,	LowPassFilterFrequency(MAX_FILTER_FREQUENCY)
,	OcclusionFilterFrequency(MAX_FILTER_FREQUENCY)
, AmbientZoneFilterFrequency(MAX_FILTER_FREQUENCY)
,	AttenuationFilterFrequency(MAX_FILTER_FREQUENCY), Pitch(0.0f)
,	Velocity( FVector::ZeroVector )
,	Location( FVector::ZeroVector )
,	OmniRadius(0.0f)
,	StereoSpread(0.0f)
,	UserIndex( 0 )
{
	TypeHash = ++TypeHashCounter;
}

/**
 * Notifies the wave instance that it has finished.
 */
void FWaveInstance::NotifyFinished( const bool bStopped )
{
	if( !bAlreadyNotifiedHook )
	{
		// Can't have a source finishing that hasn't started
		if( !bIsStarted )
		{
			UE_LOG(LogAudio, Warning, TEXT( "Received finished notification from waveinstance that hasn't started!" ) );
		}

		// We are finished.
		bIsFinished = true;

		// Avoid double notifications.
		bAlreadyNotifiedHook = true;

		NotifyBufferFinishedHooks.DispatchNotifies(this, bStopped);
	}
}

/**
 * Stops the wave instance without notifying NotifyWaveInstanceFinishedHook. This will NOT stop wave instance
 * if it is set up to loop indefinitely or set to remain active.
 */
void FWaveInstance::StopWithoutNotification( void )
{
	if( LoopingMode == LOOP_Forever || ActiveSound->bShouldRemainActiveIfDropped )
	{
		// We don't finish if we're either indefinitely looping or the audio component explicitly mandates that we should
		// remain active which is e.g. used for engine sounds and such.
		bIsFinished = false;
	}
	else
	{
		// We're finished.
		bIsFinished = true;
	}
}

FArchive& operator<<( FArchive& Ar, FWaveInstance* WaveInstance )
{
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << WaveInstance->WaveData;
		Ar << WaveInstance->SoundClass;
		Ar << WaveInstance->NotifyBufferFinishedHooks;
	}
	return( Ar );
}

void FWaveInstance::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( WaveData );
	Collector.AddReferencedObject( SoundClass );
	NotifyBufferFinishedHooks.AddReferencedObjects( Collector );
}

float FWaveInstance::GetActualVolume() const
{
	return Volume * VolumeMultiplier;
}


bool FWaveInstance::ShouldStopDueToMaxConcurrency() const
{
	return ActiveSound->bShouldStopDueToMaxConcurrency;
}

float FWaveInstance::GetVolumeWeightedPriority() const
{
	// This will result in zero-volume sounds still able to be sorted due to priority but give non-zero volumes higher priority than 0 volumes
	float ActualVolume = GetActualVolume();
	if (ActualVolume > 0.0f)
	{
		return ActualVolume * Priority;
	}
	else
	{
		return Priority - MAX_SOUND_PRIORITY - 1;
	}
}

bool FWaveInstance::IsStreaming() const
{
	return FPlatformProperties::SupportsAudioStreaming() && WaveData != nullptr && WaveData->IsStreaming();
}

FString FWaveInstance::GetName() const
{
	if (WaveData)
	{
		return WaveData->GetName();
	}
	return TEXT("Null");
}


/*-----------------------------------------------------------------------------
	WaveModInfo implementation - downsampling of wave files.
-----------------------------------------------------------------------------*/

//  Macros to convert 4 bytes to a Riff-style ID uint32.
//  Todo: make these endian independent !!!

#define UE_MAKEFOURCC(ch0, ch1, ch2, ch3)\
	((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) |\
	((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))

#define UE_mmioFOURCC(ch0, ch1, ch2, ch3)\
	UE_MAKEFOURCC(ch0, ch1, ch2, ch3)

// Main Riff-Wave header.
struct FRiffWaveHeader
{
	uint32	rID;			// Contains 'RIFF'
	uint32	ChunkLen;		// Remaining length of the entire riff chunk (= file).
	uint32	wID;			// Form type. Contains 'WAVE' for .wav files.
};

// General chunk header format.
struct FRiffChunkOld
{
	uint32	ChunkID;		  // General data chunk ID like 'data', or 'fmt '
	uint32	ChunkLen;		  // Length of the rest of this chunk in bytes.
};

// ChunkID: 'fmt ' ("WaveFormatEx" structure )
struct FFormatChunk
{
	uint16   wFormatTag;        // Format type: 1 = PCM
	uint16   nChannels;         // Number of channels (i.e. mono, stereo...).
	uint32   nSamplesPerSec;    // Sample rate. 44100 or 22050 or 11025  Hz.
	uint32   nAvgBytesPerSec;   // For buffer estimation  = sample rate * BlockAlign.
	uint16   nBlockAlign;       // Block size of data = Channels times BYTES per sample.
	uint16   wBitsPerSample;    // Number of bits per sample of mono data.
	uint16   cbSize;            // The count in bytes of the size of extra information (after cbSize).
};

// ChunkID: 'smpl'
struct FSampleChunk
{
	uint32   dwManufacturer;
	uint32   dwProduct;
	uint32   dwSamplePeriod;
	uint32   dwMIDIUnityNote;
	uint32   dwMIDIPitchFraction;
	uint32	dwSMPTEFormat;
	uint32   dwSMPTEOffset;		//
	uint32   cSampleLoops;		// Number of tSampleLoop structures following this chunk
	uint32   cbSamplerData;		//
};

//
//	Figure out the WAVE file layout.
//
bool FWaveModInfo::ReadWaveInfo( uint8* WaveData, int32 WaveDataSize, FString* ErrorReason )
{
	FFormatChunk* FmtChunk;
	FRiffWaveHeader* RiffHdr = ( FRiffWaveHeader* )WaveData;
	WaveDataEnd = WaveData + WaveDataSize;

	if( WaveDataSize == 0 )
	{
		return( false );
	}

	// Verify we've got a real 'WAVE' header.
#if PLATFORM_LITTLE_ENDIAN
	if( RiffHdr->wID != UE_mmioFOURCC( 'W','A','V','E' ) )
	{
		if (ErrorReason) *ErrorReason = TEXT("Invalid WAVE file.");
		return( false );
	}
#else
	if( ( RiffHdr->wID != ( UE_mmioFOURCC( 'W','A','V','E' ) ) ) &&
	     ( RiffHdr->wID != ( UE_mmioFOURCC( 'E','V','A','W' ) ) ) )
	{
		ErrorReason = TEXT("Invalid WAVE file.")
		return( false );
	}

	bool AlreadySwapped = ( RiffHdr->wID == ( UE_mmioFOURCC('W','A','V','E' ) ) );
	if( !AlreadySwapped )
	{
		RiffHdr->rID = INTEL_ORDER32( RiffHdr->rID );
		RiffHdr->ChunkLen = INTEL_ORDER32( RiffHdr->ChunkLen );
		RiffHdr->wID = INTEL_ORDER32( RiffHdr->wID );
	}
#endif

	FRiffChunkOld* RiffChunk = ( FRiffChunkOld* )&WaveData[3 * 4];
	pMasterSize = &RiffHdr->ChunkLen;

	// Look for the 'fmt ' chunk.
	while( ( ( ( uint8* )RiffChunk + 8 ) < WaveDataEnd ) && ( INTEL_ORDER32( RiffChunk->ChunkID ) != UE_mmioFOURCC( 'f','m','t',' ' ) ) )
	{
		RiffChunk = ( FRiffChunkOld* )( ( uint8* )RiffChunk + Pad16Bit( INTEL_ORDER32( RiffChunk->ChunkLen ) ) + 8 );
	}

	if( INTEL_ORDER32( RiffChunk->ChunkID ) != UE_mmioFOURCC( 'f','m','t',' ' ) )
	{
		#if !PLATFORM_LITTLE_ENDIAN  // swap them back just in case.
			if( !AlreadySwapped )
			{
				RiffHdr->rID = INTEL_ORDER32( RiffHdr->rID );
				RiffHdr->ChunkLen = INTEL_ORDER32( RiffHdr->ChunkLen );
				RiffHdr->wID = INTEL_ORDER32( RiffHdr->wID );
			}
		#endif
		if (ErrorReason) *ErrorReason = TEXT("Invalid WAVE file.");
		return( false );
	}

	FmtChunk = ( FFormatChunk* )( ( uint8* )RiffChunk + 8 );
#if !PLATFORM_LITTLE_ENDIAN
	if( !AlreadySwapped )
	{
		FmtChunk->wFormatTag = INTEL_ORDER16( FmtChunk->wFormatTag );
		FmtChunk->nChannels = INTEL_ORDER16( FmtChunk->nChannels );
		FmtChunk->nSamplesPerSec = INTEL_ORDER32( FmtChunk->nSamplesPerSec );
		FmtChunk->nAvgBytesPerSec = INTEL_ORDER32( FmtChunk->nAvgBytesPerSec );
		FmtChunk->nBlockAlign = INTEL_ORDER16( FmtChunk->nBlockAlign );
		FmtChunk->wBitsPerSample = INTEL_ORDER16( FmtChunk->wBitsPerSample );
	}
#endif
	pBitsPerSample = &FmtChunk->wBitsPerSample;
	pSamplesPerSec = &FmtChunk->nSamplesPerSec;
	pAvgBytesPerSec = &FmtChunk->nAvgBytesPerSec;
	pBlockAlign = &FmtChunk->nBlockAlign;
	pChannels = &FmtChunk->nChannels;
	pFormatTag = &FmtChunk->wFormatTag;

	// re-initalize the RiffChunk pointer
	RiffChunk = ( FRiffChunkOld* )&WaveData[3 * 4];

	// Look for the 'data' chunk.
	while( ( ( ( uint8* )RiffChunk + 8 ) < WaveDataEnd ) && ( INTEL_ORDER32( RiffChunk->ChunkID ) != UE_mmioFOURCC( 'd','a','t','a' ) ) )
	{
		RiffChunk = ( FRiffChunkOld* )( ( uint8* )RiffChunk + Pad16Bit( INTEL_ORDER32( RiffChunk->ChunkLen ) ) + 8 );
	}

	if( INTEL_ORDER32( RiffChunk->ChunkID ) != UE_mmioFOURCC( 'd','a','t','a' ) )
	{
		#if !PLATFORM_LITTLE_ENDIAN  // swap them back just in case.
			if( !AlreadySwapped )
			{
				RiffHdr->rID = INTEL_ORDER32( RiffHdr->rID );
				RiffHdr->ChunkLen = INTEL_ORDER32( RiffHdr->ChunkLen );
				RiffHdr->wID = INTEL_ORDER32( RiffHdr->wID );
				FmtChunk->wFormatTag = INTEL_ORDER16( FmtChunk->wFormatTag );
				FmtChunk->nChannels = INTEL_ORDER16( FmtChunk->nChannels );
				FmtChunk->nSamplesPerSec = INTEL_ORDER32( FmtChunk->nSamplesPerSec );
				FmtChunk->nAvgBytesPerSec = INTEL_ORDER32( FmtChunk->nAvgBytesPerSec );
				FmtChunk->nBlockAlign = INTEL_ORDER16( FmtChunk->nBlockAlign );
				FmtChunk->wBitsPerSample = INTEL_ORDER16( FmtChunk->wBitsPerSample );
			}
		#endif
		if (ErrorReason) *ErrorReason = TEXT("Invalid WAVE file.");
		return( false );
	}

#if !PLATFORM_LITTLE_ENDIAN  // swap them back just in case.
	if( AlreadySwapped ) // swap back into Intel order for chunk search...
	{
		RiffChunk->ChunkLen = INTEL_ORDER32( RiffChunk->ChunkLen );
	}
#endif

	SampleDataStart = ( uint8* )RiffChunk + 8;
	pWaveDataSize = &RiffChunk->ChunkLen;
	SampleDataSize = INTEL_ORDER32( RiffChunk->ChunkLen );
	OldBitsPerSample = FmtChunk->wBitsPerSample;
	SampleDataEnd = SampleDataStart + SampleDataSize;

	if( ( uint8* )SampleDataEnd > ( uint8* )WaveDataEnd )
	{
		UE_LOG(LogAudio, Warning, TEXT( "Wave data chunk is too big!" ) );

		// Fix it up by clamping data chunk.
		SampleDataEnd = ( uint8* )WaveDataEnd;
		SampleDataSize = SampleDataEnd - SampleDataStart;
		RiffChunk->ChunkLen = INTEL_ORDER32( SampleDataSize );
	}

	NewDataSize = SampleDataSize;

	if(    FmtChunk->wFormatTag != 0x0001 // WAVE_FORMAT_PCM  
		&& FmtChunk->wFormatTag != 0x0002 // WAVE_FORMAT_ADPCM
		&& FmtChunk->wFormatTag != 0x0011) // WAVE_FORMAT_DVI_ADPCM
	{
		ReportImportFailure();
		if (ErrorReason) *ErrorReason = TEXT("Unsupported wave file format.  Only PCM, ADPCM, and DVI ADPCM can be imported.");
		return( false );
	}

#if !PLATFORM_LITTLE_ENDIAN
	if( !AlreadySwapped )
	{
		if( FmtChunk->wBitsPerSample == 16 )
		{
			for( uint16* i = ( uint16* )SampleDataStart; i < ( uint16* )SampleDataEnd; i++ )
			{
				*i = INTEL_ORDER16( *i );
			}
		}
		else if( FmtChunk->wBitsPerSample == 32 )
		{
			for( uint32* i = ( uint32* )SampleDataStart; i < ( uint32* )SampleDataEnd; i++ )
			{
				*i = INTEL_ORDER32( *i );
			}
		}
	}
#endif

	// Couldn't byte swap this before, since it'd throw off the chunk search.
#if !PLATFORM_LITTLE_ENDIAN
	*pWaveDataSize = INTEL_ORDER32( *pWaveDataSize );
#endif

	return( true );
}

bool FWaveModInfo::ReadWaveHeader(uint8* RawWaveData, int32 Size, int32 Offset )
{
	if( Size == 0 )
	{
		return( false );
	}

	// Parse wave info.
	if( !ReadWaveInfo( RawWaveData + Offset, Size ) )
	{
		return( false );
	}

	// Validate the info
	if( ( *pChannels != 1 && *pChannels != 2 ) || *pBitsPerSample != 16 )
	{
		return( false );
	}

	return( true );
}

void FWaveModInfo::ReportImportFailure() const
{
	if (FEngineAnalytics::IsAvailable())
	{
		TArray< FAnalyticsEventAttribute > WaveImportFailureAttributes;
		WaveImportFailureAttributes.Add(FAnalyticsEventAttribute(TEXT("Format"), *pFormatTag));
		WaveImportFailureAttributes.Add(FAnalyticsEventAttribute(TEXT("Channels"), *pChannels));
		WaveImportFailureAttributes.Add(FAnalyticsEventAttribute(TEXT("BitsPerSample"), *pBitsPerSample));

		FEngineAnalytics::GetProvider().RecordEvent(FString("Editor.Usage.WaveImportFailure"), WaveImportFailureAttributes);
	}
}
