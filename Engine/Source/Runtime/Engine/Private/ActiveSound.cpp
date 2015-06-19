// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ActiveSound.h"
#include "AudioDevice.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundNodeAttenuation.h"
#include "SubtitleManager.h"

FActiveSound::FActiveSound()
	: Sound(NULL)
	, World(NULL)
	, AudioComponent(NULL)
	, SoundClassOverride(NULL)
	, bOccluded(false)
	, bAllowSpatialization(true)
	, bHasAttenuationSettings(false)
	, bShouldRemainActiveIfDropped(false)
	, bFadingOut(false)
	, bFinished(false)
	, bRadioFilterSelected(false)
	, bApplyRadioFilter(false)
	, bHandleSubtitles(true)
	, bLocationDefined(false)
	, bIgnoreForFlushing(false)
	, bEQFilterApplied(false)
	, bAlwaysPlay(false)
	, bIsUISound(false)
	, bIsMusic(false)
	, bReverb(false)
	, bCenterChannelOnly(false)
	, bGotInteriorSettings(false)
#if !NO_LOGGING
	, bWarnedAboutOrphanedLooping(false)
#endif
	, UserIndex(0)
	, PlaybackTime(0.f)
	, RequestedStartTime(0.f)
	, CurrentAdjustVolumeMultiplier(1.f)
	, TargetAdjustVolumeMultiplier(1.f)
	, TargetAdjustVolumeStopTime(-1.f)
	, VolumeMultiplier(1.f)
	, PitchMultiplier(1.f)
	, HighFrequencyGainMultiplier(1.f)
	, SubtitlePriority(0.f)
	, OcclusionCheckInterval(0.f)
	, LastOcclusionCheckTime(0.f)
	, LastLocation(FVector::ZeroVector)
	, LastAudioVolume(NULL)
	, LastUpdateTime(0.f)
	, SourceInteriorVolume(1.f)
	, SourceInteriorLPF(1.f)
	, CurrentInteriorVolume(1.f)
	, CurrentInteriorLPF(1.f)
{
}

FActiveSound::~FActiveSound()
{
	ensureMsg(WaveInstances.Num() == 0, TEXT("Destroyed an active sound that had active wave instances."));
}

FArchive& operator<<( FArchive& Ar, FActiveSound* ActiveSound )
{
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << ActiveSound->Sound;
		Ar << ActiveSound->LastAudioVolume;
		Ar << ActiveSound->WaveInstances;
		Ar << ActiveSound->SoundNodeOffsetMap;
	}
	return( Ar );
}

void FActiveSound::AddReferencedObjects( FReferenceCollector& Collector)
{
	for (auto WaveInstanceIt(WaveInstances.CreateConstIterator()); WaveInstanceIt; ++WaveInstanceIt)
	{
		FWaveInstance* WaveInstance = WaveInstanceIt.Value();
		// Avoid recursing back to the wave instance that sourced this active sound
		if( WaveInstance )
		{
			WaveInstance->AddReferencedObjects( Collector );
		}
	}

	Collector.AddReferencedObject(Sound);
	Collector.AddReferencedObject(LastAudioVolume);
	Collector.AddReferencedObject(SoundClassOverride);
}

USoundClass* FActiveSound::GetSoundClass() const
{
	if (SoundClassOverride)
	{
		return SoundClassOverride;
	}
	else if (Sound)
	{
		return Sound->GetSoundClass();
	}

	return NULL;
}

int32 FActiveSound::FindClosestListener( const TArray<FListener>& InListeners ) const
{
	int32 ClosestListenerIndex = 0;
	float ClosestDistSq = FVector::DistSquared(Transform.GetTranslation(), InListeners[0].Transform.GetTranslation() );

	for( int32 i = 1; i < InListeners.Num(); i++ )
	{
		const float DistSq = FVector::DistSquared(Transform.GetTranslation(), InListeners[i].Transform.GetTranslation());
		if( DistSq < ClosestDistSq )
		{
			ClosestListenerIndex = i;
			ClosestDistSq = DistSq;
		}
	}

	return ClosestListenerIndex;
}


void FActiveSound::UpdateWaveInstances( FAudioDevice* AudioDevice, TArray<FWaveInstance*> &InWaveInstances, const float DeltaTime )
{
	check( AudioDevice );

	// Early outs.
	if( Sound == NULL || !Sound->IsPlayable() )
	{
		return;
	}

	//@todo audio: Need to handle pausing and not getting out of sync by using the mixer's time.
	//@todo audio: Fading in and out is also dependent on the DeltaTime
	PlaybackTime += DeltaTime;

	FSoundParseParameters ParseParams;
	ParseParams.Transform = Transform;
	ParseParams.StartTime = RequestedStartTime;

	// Don't clear the seeking unless the sound is actually playing
	if (DeltaTime > 0.f)
	{
		RequestedStartTime = 0.f;
	}

	// splitscreen support:
	// we always pass the 'primary' listener (viewport 0) to the sound nodes and the underlying audio system
	// then move the AudioComponent's CurrentLocation so that its position relative to that Listener is the same as its real position is relative to the closest Listener
	const FListener& Listener = AudioDevice->Listeners[ 0 ];

	int32 ClosestListenerIndex = 0;

	if (AudioDevice->Listeners.Num() > 0)
	{
		SCOPE_CYCLE_COUNTER( STAT_AudioFindNearestLocation );
		ClosestListenerIndex = FindClosestListener(AudioDevice->Listeners);
	}

	const FListener& ClosestListener = AudioDevice->Listeners[ ClosestListenerIndex ];

	// Process occlusion before shifting the sounds position
	if (OcclusionCheckInterval > 0.f)
	{
		CheckOcclusion( ClosestListener.Transform.GetTranslation(), ParseParams.Transform.GetTranslation() );
	}

	// Default values.
	// It's all Multiplicative!  So now people are all modifying the multiplier values via various means
	// (even after the Sound has started playing, and this line takes them all into account and gives us
	// final value that is correct
	UpdateAdjustVolumeMultiplier(DeltaTime);
	ParseParams.VolumeMultiplier = VolumeMultiplier * Sound->GetVolumeMultiplier() * CurrentAdjustVolumeMultiplier * AudioDevice->TransientMasterVolume * (bOccluded ? 0.5f : 1.0f);
	ParseParams.Pitch *= PitchMultiplier * Sound->GetPitchMultiplier();
	ParseParams.HighFrequencyGain *= HighFrequencyGainMultiplier;

	ParseParams.SoundClass = GetSoundClass();
	if( ParseParams.SoundClass && ParseParams.SoundClass->Properties.bApplyAmbientVolumes)
	{
		// Additional inside/outside processing for ambient sounds
		// If we aren't in a world there is no interior volumes to be handled.
		HandleInteriorVolumes( ClosestListener, ParseParams );
	}

	// for velocity-based effects like doppler
	if (DeltaTime > 0.f)
	{
		ParseParams.Velocity = (ParseParams.Transform.GetTranslation() - LastLocation) / DeltaTime;
		LastLocation = ParseParams.Transform.GetTranslation();
	}

	// if the closest listener is not the primary one, transform CurrentLocation
	if( ClosestListenerIndex != 0 )
	{
		ParseParams.Transform = ParseParams.Transform * ClosestListener.Transform.Inverse() * Listener.Transform;
	}

	// Recurse nodes, have SoundWave's create new wave instances and update bFinished unless we finished fading out.
	bFinished = true;
	if( !bFadingOut || ( PlaybackTime <= TargetAdjustVolumeStopTime ) )
	{
		if (bHasAttenuationSettings)
		{
			AttenuationSettings.ApplyAttenuation(ParseParams.Transform, Listener.Transform.GetTranslation(), ParseParams.Volume, ParseParams.HighFrequencyGain );
			ParseParams.OmniRadius = AttenuationSettings.OmniRadius;
			ParseParams.bUseSpatialization = AttenuationSettings.bSpatialize;
			if (AttenuationSettings.SpatializationAlgorithm == SPATIALIZATION_Default && AudioDevice->IsHRTFEnabledForAll())
			{
				ParseParams.SpatializationAlgorithm = SPATIALIZATION_HRTF;
			}
			else
			{
				ParseParams.SpatializationAlgorithm = AttenuationSettings.SpatializationAlgorithm;
			}
		}

		Sound->Parse( AudioDevice, 0, *this, ParseParams, InWaveInstances );
	}

	if( bFinished )
	{
		Stop(AudioDevice);
	}
}

void FActiveSound::Stop(FAudioDevice* AudioDevice)
{
	check(AudioDevice);

	if (Sound)
	{
		Sound->CurrentPlayCount = FMath::Max( Sound->CurrentPlayCount - 1, 0 );
	}

	for (auto WaveInstanceIt(WaveInstances.CreateIterator()); WaveInstanceIt; ++WaveInstanceIt)
	{
		FWaveInstance*& WaveInstance = WaveInstanceIt.Value();

		// Stop the owning sound source
		FSoundSource* Source = AudioDevice->WaveInstanceSourceMap.FindRef( WaveInstance );
		if( Source )
		{
			Source->Stop();
		}

		// TODO - Audio Threading. This call would be a task back to game thread
		// Dequeue subtitles for this sounds
		FSubtitleManager::GetSubtitleManager()->KillSubtitles( ( PTRINT )WaveInstance );

		delete WaveInstance;

		// Null the entry out temporarily as later Stop calls could try to access this structure
		WaveInstance = NULL;
	}
	WaveInstances.Empty();

	// TODO - Audio Threading. This call would be a task back to game thread
	if (AudioComponent.IsValid())
	{
		AudioComponent->PlaybackCompleted(false);
	}

	AudioDevice->RemoveActiveSound(this);
}

FWaveInstance* FActiveSound::FindWaveInstance( const UPTRINT WaveInstanceHash )
{
	FWaveInstance** WaveInstance = WaveInstances.Find(WaveInstanceHash);
	return (WaveInstance ? *WaveInstance : NULL);
}

void FActiveSound::UpdateAdjustVolumeMultiplier( const float DeltaTime )
{
	// keep stepping towards our target until we hit our stop time
	if( PlaybackTime <= TargetAdjustVolumeStopTime )
	{
		CurrentAdjustVolumeMultiplier += (TargetAdjustVolumeMultiplier - CurrentAdjustVolumeMultiplier) * DeltaTime / (TargetAdjustVolumeStopTime - PlaybackTime);
	}
	else
	{
		CurrentAdjustVolumeMultiplier = TargetAdjustVolumeMultiplier;
	}
}

void FActiveSound::CheckOcclusion( const FVector ListenerLocation, const FVector SoundLocation )
{
	// @optimization: dont do this if listener and current locations haven't changed much?
	// also, should this use getaudiotimeseconds?
	UWorld* WorldPtr = World.Get();
	if (WorldPtr->GetTimeSeconds() - LastOcclusionCheckTime > OcclusionCheckInterval && Sound->GetMaxAudibleDistance() != WORLD_MAX )
	{
		LastOcclusionCheckTime = WorldPtr->GetTimeSeconds();
		static FName NAME_SoundOcclusion = FName(TEXT("SoundOcclusion"));
		const bool bNowOccluded = WorldPtr->LineTraceTestByChannel(SoundLocation, ListenerLocation, ECC_Visibility, FCollisionQueryParams(NAME_SoundOcclusion, true));
		if( bNowOccluded != bOccluded )
		{
			bOccluded = bNowOccluded;
		}
	}
}

const TCHAR* GetAWaveName(TMap<UPTRINT, struct FWaveInstance*> WaveInstances)
{
	TArray<FWaveInstance*> WaveInstanceArray;
	WaveInstances.GenerateValueArray(WaveInstanceArray);
	return *WaveInstanceArray[0]->WaveData->GetName();
}

void FActiveSound::HandleInteriorVolumes( const FListener& Listener, FSoundParseParameters& ParseParams )
{
	UWorld* WorldPtr = World.Get();
	if (WorldPtr == nullptr)
	{
		return;
	}

	// Get the settings of the ambient sound
	FInteriorSettings Ambient;
	class AAudioVolume* AudioVolume;

	if (!bGotInteriorSettings || (GIsEditor && !WorldPtr->IsGameWorld()) || (ParseParams.Transform.GetTranslation() - LastLocation).SizeSquared() > KINDA_SMALL_NUMBER)
	{
		AudioVolume = WorldPtr->GetAudioSettings(ParseParams.Transform.GetTranslation(), NULL, &Ambient);
		LastInteriorSettings = Ambient;
		LastAudioVolume = AudioVolume;
		bGotInteriorSettings = true;
	}
	else
	{
		// use previous settings as we haven't moved
		Ambient = LastInteriorSettings;
		AudioVolume = LastAudioVolume;
	}

	// Check to see if we've moved to a new audio volume
	if( LastUpdateTime < Listener.InteriorStartTime )
	{
		SourceInteriorVolume = CurrentInteriorVolume;
		SourceInteriorLPF = CurrentInteriorLPF;
		LastUpdateTime = FApp::GetCurrentTime();
	}

	if( Listener.Volume == AudioVolume || !bAllowSpatialization )
	{
		// Ambient and listener in same ambient zone
		CurrentInteriorVolume = ( SourceInteriorVolume * ( 1.0f - Listener.InteriorVolumeInterp ) ) + Listener.InteriorVolumeInterp;
		ParseParams.VolumeMultiplier *= CurrentInteriorVolume;

		CurrentInteriorLPF = ( SourceInteriorLPF * ( 1.0f - Listener.InteriorLPFInterp ) ) + Listener.InteriorLPFInterp;
		ParseParams.HighFrequencyGain *= CurrentInteriorLPF;

		UE_LOG(LogAudio, Verbose, TEXT( "Ambient in same volume. Volume *= %g LPF *= %g (%s)" ),
			CurrentInteriorVolume, CurrentInteriorLPF, ( WaveInstances.Num() > 0 ) ? GetAWaveName(WaveInstances) : TEXT( "NULL" ) );
	}
	else
	{
		// Ambient and listener in different ambient zone
		if( Ambient.bIsWorldSettings )
		{
			// The ambient sound is 'outside' - use the listener's exterior volume
			CurrentInteriorVolume = ( SourceInteriorVolume * ( 1.0f - Listener.ExteriorVolumeInterp ) ) + ( Listener.InteriorSettings.ExteriorVolume * Listener.ExteriorVolumeInterp );
			ParseParams.VolumeMultiplier *= CurrentInteriorVolume;

			CurrentInteriorLPF = ( SourceInteriorLPF * ( 1.0f - Listener.ExteriorLPFInterp ) ) + ( Listener.InteriorSettings.ExteriorLPF * Listener.ExteriorLPFInterp );
			ParseParams.HighFrequencyGain *= CurrentInteriorLPF;

			UE_LOG(LogAudio, Verbose, TEXT( "Ambient in diff volume, ambient outside. Volume *= %g LPF *= %g (%s)" ),
				CurrentInteriorVolume, CurrentInteriorLPF, ( WaveInstances.Num() > 0 ) ? GetAWaveName(WaveInstances) : TEXT( "NULL" ) );
		}
		else
		{
			// The ambient sound is 'inside' - use the ambient sound's interior volume multiplied with the listeners exterior volume
			CurrentInteriorVolume = (( SourceInteriorVolume * ( 1.0f - Listener.InteriorVolumeInterp ) ) + ( Ambient.InteriorVolume * Listener.InteriorVolumeInterp ))
										* (( SourceInteriorVolume * ( 1.0f - Listener.ExteriorVolumeInterp ) ) + ( Listener.InteriorSettings.ExteriorVolume * Listener.ExteriorVolumeInterp ));
			ParseParams.VolumeMultiplier *= CurrentInteriorVolume;

			CurrentInteriorLPF = (( SourceInteriorLPF * ( 1.0f - Listener.InteriorLPFInterp ) ) + ( Ambient.InteriorLPF * Listener.InteriorLPFInterp ))
										* (( SourceInteriorLPF * ( 1.0f - Listener.ExteriorLPFInterp ) ) + ( Listener.InteriorSettings.ExteriorLPF * Listener.ExteriorLPFInterp ));
			ParseParams.HighFrequencyGain *= CurrentInteriorLPF;

			UE_LOG(LogAudio, Verbose, TEXT( "Ambient in diff volume, ambient inside. Volume *= %g LPF *= %g (%s)" ),
				CurrentInteriorVolume, CurrentInteriorLPF, ( WaveInstances.Num() > 0 ) ? GetAWaveName(WaveInstances) : TEXT( "NULL" ) );
		}
	}
}

void FActiveSound::ApplyRadioFilter( FAudioDevice* AudioDevice, const FSoundParseParameters& ParseParams )
{
	if( AudioDevice->GetMixDebugState() != DEBUGSTATE_DisableRadio )
	{
		// Make sure the radio filter is requested
		if( ParseParams.SoundClass)
		{
			const float RadioFilterVolumeThreshold = ParseParams.VolumeMultiplier * ParseParams.SoundClass->Properties.RadioFilterVolumeThreshold;
			if (RadioFilterVolumeThreshold > KINDA_SMALL_NUMBER )
			{
				bApplyRadioFilter = ( ParseParams.Volume < RadioFilterVolumeThreshold );
			}
		}
	}
	else
	{
		bApplyRadioFilter = false;
	}

	bRadioFilterSelected = true;
}

bool FActiveSound::GetFloatParameter( const FName InName, float& OutFloat ) const
{
	// Always fail if we pass in no name.
	if( InName != NAME_None )
	{
		for( int32 Index = 0; Index < InstanceParameters.Num(); ++Index )
		{
			const FAudioComponentParam& P = InstanceParameters[Index];
			if( P.ParamName == InName )
			{
				OutFloat = P.FloatParam;
				return true;
			}
		}
	}

	return false;
}

void FActiveSound::SetFloatParameter( const FName InName, const float InFloat )
{
	if( InName != NAME_None )
	{
		// First see if an entry for this name already exists
		for( int32 Index = 0; Index < InstanceParameters.Num(); ++Index )
		{
			FAudioComponentParam& P = InstanceParameters[Index];
			if( P.ParamName == InName )
			{
				P.FloatParam = InFloat;
				return;
			}
		}

		// We didn't find one, so create a new one.
		const int32 NewParamIndex = InstanceParameters.AddZeroed();
		InstanceParameters[ NewParamIndex ].ParamName = InName;
		InstanceParameters[ NewParamIndex ].FloatParam = InFloat;
	}
}

bool FActiveSound::GetWaveParameter( const FName InName, USoundWave*& OutWave ) const
{
	// Always fail if we pass in no name.
	if( InName != NAME_None )
	{
		for( int32 Index = 0; Index < InstanceParameters.Num(); ++Index )
		{
			const FAudioComponentParam& P = InstanceParameters[Index];
			if( P.ParamName == InName )
			{
				OutWave = P.SoundWaveParam;
				return true;
			}
		}
	}

	return false;
}

void FActiveSound::SetWaveParameter( FName InName, USoundWave* InWave )
{
	if( InName != NAME_None )
	{
		// First see if an entry for this name already exists
		for( int32 Index = 0; Index < InstanceParameters.Num(); ++Index )
		{
			FAudioComponentParam& P = InstanceParameters[Index];
			if( P.ParamName == InName )
			{
				P.SoundWaveParam = InWave;
				return;
			}
		}

		// We didn't find one, so create a new one.
		const int32 NewParamIndex = InstanceParameters.AddZeroed();
		InstanceParameters[ NewParamIndex ].ParamName = InName;
		InstanceParameters[ NewParamIndex ].SoundWaveParam = InWave;
	}
}

bool FActiveSound::GetBoolParameter( const FName InName, bool& OutBool ) const
{
	// Always fail if we pass in no name.
	if( InName != NAME_None )
	{
		for( int32 Index = 0; Index < InstanceParameters.Num(); ++Index )
		{
			const FAudioComponentParam& P = InstanceParameters[Index];
			if( P.ParamName == InName )
			{
				OutBool = P.BoolParam;
				return true;
			}
		}
	}

	return false;
}

void FActiveSound::SetBoolParameter( FName InName, const bool InBool )
{
	if( InName != NAME_None )
	{
		// First see if an entry for this name already exists
		for( int32 Index = 0; Index < InstanceParameters.Num(); ++Index )
		{
			FAudioComponentParam& P = InstanceParameters[Index];
			if( P.ParamName == InName )
			{
				P.BoolParam = InBool;
				return;
			}
		}

		// We didn't find one, so create a new one.
		const int32 NewParamIndex = InstanceParameters.AddZeroed();
		InstanceParameters[ NewParamIndex ].ParamName = InName;
		InstanceParameters[ NewParamIndex ].BoolParam = InBool;
	}
}

int32 FActiveSound::GetIntParameter( const FName InName, int32& OutInt ) const
{
	// Always fail if we pass in no name.
	if( InName != NAME_None )
	{
		for( int32 Index = 0; Index < InstanceParameters.Num(); ++Index )
		{
			const FAudioComponentParam& P = InstanceParameters[Index];
			if( P.ParamName == InName )
			{
				OutInt = P.IntParam;
				return true;
			}
		}
	}

	return false;
}

void FActiveSound::SetIntParameter( FName InName, const int32 InInt )
{
	if( InName != NAME_None )
	{
		// First see if an entry for this name already exists
		for( int32 Index = 0; Index < InstanceParameters.Num(); ++Index )
		{
			FAudioComponentParam& P = InstanceParameters[Index];
			if( P.ParamName == InName )
			{
				P.IntParam = InInt;
				return;
			}
		}

		// We didn't find one, so create a new one.
		const int32 NewParamIndex = InstanceParameters.AddZeroed();
		InstanceParameters[ NewParamIndex ].ParamName = InName;
		InstanceParameters[ NewParamIndex ].IntParam = InInt;
	}
}

void FActiveSound::CollectAttenuationShapesForVisualization(TMultiMap<EAttenuationShape::Type, FAttenuationSettings::AttenuationShapeDetails>& ShapeDetailsMap) const
{
	bool bFoundAttenuationSettings = false;

	if (bHasAttenuationSettings)
	{
		AttenuationSettings.CollectAttenuationShapesForVisualization(ShapeDetailsMap);
	}

	// For sound cues we'll dig in and see if we can find any attenuation sound nodes that will affect the settings
	USoundCue* SoundCue = Cast<USoundCue>(Sound);
	if (SoundCue)
	{
		TArray<USoundNodeAttenuation*> AttenuationNodes;
		SoundCue->RecursiveFindAttenuation( SoundCue->FirstNode, AttenuationNodes );
		for (int32 NodeIndex = 0; NodeIndex < AttenuationNodes.Num(); ++NodeIndex)
		{
			FAttenuationSettings* AttenuationSettingsToApply = AttenuationNodes[NodeIndex]->GetAttenuationSettingsToApply();
			if (AttenuationSettingsToApply)
			{
				AttenuationSettingsToApply->CollectAttenuationShapesForVisualization(ShapeDetailsMap);
			}
		}
	}
}
