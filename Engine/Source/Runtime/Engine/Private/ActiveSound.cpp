// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ActiveSound.h"
#include "AudioDevice.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundNodeAttenuation.h"
#include "SubtitleManager.h"

FActiveSound::FActiveSound()
	: Sound(nullptr)
	, World(nullptr)
	, AudioComponent(nullptr)
	, AudioDevice(nullptr)
	, ConcurrencyGroupID(0)
	, ConcurrencySettings(nullptr)
	, SoundClassOverride(nullptr)
	, bIsOccluded(false)
	, bAllowSpatialization(true)
	, bHasAttenuationSettings(false)
	, bShouldRemainActiveIfDropped(false)
	, bFadingOut(false)
	, bFinished(false)
	, bShouldStopDueToMaxConcurrency(false)
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
#if !(NO_LOGGING || UE_BUILD_SHIPPING || UE_BUILD_TEST)
	, bWarnedAboutOrphanedLooping(false)
#endif
	, bEnableLowPassFilter(false)
	, bEnableOcclusionChecks(false)
	, bUseComplexOcclusionChecks(false)
	, bOcclusionAsyncTrace(false)
	, UserIndex(0)
	, PlaybackTime(0.f)
	, RequestedStartTime(0.f)
	, CurrentAdjustVolumeMultiplier(1.f)
	, TargetAdjustVolumeMultiplier(1.f)
	, TargetAdjustVolumeStopTime(-1.f)
	, VolumeMultiplier(1.f)
	, PitchMultiplier(1.f)
	, LowPassFilterFrequency(MAX_FILTER_FREQUENCY)
	, OcclusionLowPassFilterFrequency(MAX_FILTER_FREQUENCY)
	, OcclusionInterpolationTime(0.1f)
	, OcclusionVolumeAttenuation(1.0f)
	, CurrentOcclusionFilterFrequency(MAX_FILTER_FREQUENCY)
	, CurrentOcclusionVolumeAttenuation(1.0f)
	, ConcurrencyVolumeScale(1.f)
	, SubtitlePriority(0.f)
	, Priority(1.0f)
	, VolumeConcurrency(0.0f)
	, OcclusionCheckInterval(0.f)
	, LastOcclusionCheckTime(0.f)
	, MaxDistance(WORLD_MAX)
	, LastLocation(FVector::ZeroVector)
	, LastAudioVolume(nullptr)
	, LastUpdateTime(0.f)
	, SourceInteriorVolume(1.f)
	, SourceInteriorLPF(1.f)
	, CurrentInteriorVolume(1.f)
	, CurrentInteriorLPF(1.f)
	, bIsAudible(true)
{
	// Bind our async occlusion trace delegate
	OcclusionTraceDelegate.BindRaw(this, &FActiveSound::OcclusionTraceDone);
}

FActiveSound::~FActiveSound()
{
	ensureMsgf(WaveInstances.Num() == 0, TEXT("Destroyed an active sound that had active wave instances."));
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

uint32 FActiveSound::TryGetOwnerID() const
{
	// Only have an owner of the active sound if the audio component is valid and if the audio component has an owner.
	if (UAudioComponent* AudioComponentPtr = AudioComponent.Get())
	{
		AActor* Owner = AudioComponentPtr->GetOwner();
		if (Owner)
		{
			return Owner->GetUniqueID();
		}
	}

	return 0;
}

const FSoundConcurrencySettings* FActiveSound::GetSoundConcurrencySettingsToApply() const
{
	if (ConcurrencySettings)
	{
		return &ConcurrencySettings->Concurrency;
	}
	else if (Sound)
	{
		return Sound->GetSoundConcurrencySettingsToApply();
	}
	return nullptr;
}

uint32 FActiveSound::GetSoundConcurrencyObjectID() const
{
	if (ConcurrencySettings)
	{
		return ConcurrencySettings->GetUniqueID();
	}
	else
	{
		return Sound->GetSoundConcurrencyObjectID();
	}
}


void FActiveSound::UpdateWaveInstances( TArray<FWaveInstance*> &InWaveInstances, const float DeltaTime )
{
	check(AudioDevice);

	// Early outs.
	if (Sound == nullptr || !Sound->IsPlayable())
	{
		return;
	}

	//@todo audio: Need to handle pausing and not getting out of sync by using the mixer's time.
	//@todo audio: Fading in and out is also dependent on the DeltaTime
	PlaybackTime += DeltaTime;

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

	if (AudioDevice->Listeners.Num() > 1)
	{
		SCOPE_CYCLE_COUNTER( STAT_AudioFindNearestLocation );
		ClosestListenerIndex = FindClosestListener(AudioDevice->Listeners);
	}

	const FListener& ClosestListener = AudioDevice->Listeners[ ClosestListenerIndex ];

	// Update whether or not his sound is out of range
	bIsAudible = AudioDevice->LocationIsAudible(Transform.GetTranslation(), ClosestListener, MaxDistance);

	FSoundParseParameters ParseParams;
	ParseParams.Transform = Transform;
	ParseParams.StartTime = RequestedStartTime;

	// Set volume multiplier to 1.0f before scaling with occlusion attenuation
	float OcclusionVolumeMultiplier = 1.0f;

	// Process occlusion only if there's a chance of it being heard and if its in range (to save on doing raycasting)
	if (bEnableOcclusionChecks && bIsAudible && FApp::GetVolumeMultiplier() > 0.0f && !AudioDevice->IsAudioDeviceMuted())
	{
		CheckOcclusion(ClosestListener.Transform.GetTranslation(), ParseParams.Transform.GetTranslation());

		// Apply the volume attenuation due to occlusion (using the interpolating dynamic parameter)
		OcclusionVolumeMultiplier = CurrentOcclusionVolumeAttenuation.GetValue();
	}

	// Default values.
	// It's all Multiplicative!  So now people are all modifying the multiplier values via various means
	// (even after the Sound has started playing, and this line takes them all into account and gives us
	// final value that is correct
	UpdateAdjustVolumeMultiplier(DeltaTime);
	ParseParams.VolumeMultiplier = VolumeMultiplier * Sound->GetVolumeMultiplier() * CurrentAdjustVolumeMultiplier * AudioDevice->TransientMasterVolume * OcclusionVolumeMultiplier * ConcurrencyVolumeScale;
	ParseParams.Priority = Priority;
	ParseParams.Pitch *= PitchMultiplier * Sound->GetPitchMultiplier();
	ParseParams.bEnableLowPassFilter = bEnableLowPassFilter;
	ParseParams.bIsOccluded = bIsOccluded;
	ParseParams.LowPassFilterFrequency = LowPassFilterFrequency;
	ParseParams.OcclusionFilterFrequency = CurrentOcclusionFilterFrequency.GetValue();
	ParseParams.SoundClass = GetSoundClass();

	if (ParseParams.SoundClass && ParseParams.SoundClass->Properties.bApplyAmbientVolumes)
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
	if (ClosestListenerIndex != 0)
	{
		ParseParams.Transform = ParseParams.Transform * ClosestListener.Transform.Inverse() * Listener.Transform;
	}

	TArray<FWaveInstance*> ThisSoundsWaveInstances;

	// Recurse nodes, have SoundWave's create new wave instances and update bFinished unless we finished fading out.
	bFinished = true;
	if (!bFadingOut || (PlaybackTime <= TargetAdjustVolumeStopTime))
	{
		if (bHasAttenuationSettings)
		{
			ApplyAttenuation(ParseParams, ClosestListener);
		}


		Sound->Parse(AudioDevice, 0, *this, ParseParams, ThisSoundsWaveInstances);
	}

	if (bFinished)
	{
		AudioDevice->StopActiveSound(this);
	}
	else if (ThisSoundsWaveInstances.Num() > 0)
	{
		// If this active sound is told to limit concurrency by the quietest sound
		const FSoundConcurrencySettings* ConcurrencySettingsToApply = GetSoundConcurrencySettingsToApply();
		if (ConcurrencySettingsToApply && ConcurrencySettingsToApply->ResolutionRule == EMaxConcurrentResolutionRule::StopQuietist)
		{
			check(ConcurrencyGroupID != 0);
			// Now that we have this sound's active wave instances, lets find the loudest active wave instance to represent the "volume" of this active sound
			VolumeConcurrency = 0.0f;
			for (const FWaveInstance* WaveInstance : ThisSoundsWaveInstances)
			{
				const float WaveInstanceVolume = WaveInstance->GetActualVolume();
				if (WaveInstanceVolume > VolumeConcurrency)
				{
					VolumeConcurrency = WaveInstanceVolume;
				}
			}
		}
	}

	InWaveInstances.Append(ThisSoundsWaveInstances);
}

void FActiveSound::Stop()
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

	AudioDevice->RemoveActiveSound(this);

	delete this;
}

FWaveInstance* FActiveSound::FindWaveInstance( const UPTRINT WaveInstanceHash )
{
	FWaveInstance** WaveInstance = WaveInstances.Find(WaveInstanceHash);
	return (WaveInstance ? *WaveInstance : NULL);
}

void FActiveSound::UpdateAdjustVolumeMultiplier(const float DeltaTime)
{
	// keep stepping towards our target until we hit our stop time
	if (PlaybackTime < TargetAdjustVolumeStopTime)
	{
		CurrentAdjustVolumeMultiplier += (TargetAdjustVolumeMultiplier - CurrentAdjustVolumeMultiplier) * DeltaTime / (TargetAdjustVolumeStopTime - PlaybackTime);
	}
	else
	{
		CurrentAdjustVolumeMultiplier = TargetAdjustVolumeMultiplier;
	}
}

void FActiveSound::SetIsOccluded(const bool bInOccluded)
{
	if (bInOccluded != bIsOccluded)
	{
		bIsOccluded = bInOccluded;
		if (bIsOccluded)
		{
			CurrentOcclusionFilterFrequency.Set(OcclusionLowPassFilterFrequency, OcclusionInterpolationTime);
			CurrentOcclusionVolumeAttenuation.Set(OcclusionVolumeAttenuation, OcclusionInterpolationTime);
		}
		else
		{
			CurrentOcclusionFilterFrequency.Set(MAX_FILTER_FREQUENCY, OcclusionInterpolationTime);
			CurrentOcclusionVolumeAttenuation.Set(1.0f, OcclusionInterpolationTime);
		}
	}
}

void FActiveSound::OcclusionTraceDone(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum)
{
	SetIsOccluded(TraceDatum.OutHits.Num() > 0);
}

void FActiveSound::CheckOcclusion(const FVector ListenerLocation, const FVector SoundLocation)
{
	check(bEnableOcclusionChecks);
	UWorld* WorldPtr = World.Get();
	float WorldTime = WorldPtr->GetTimeSeconds();
	if ((WorldTime - LastOcclusionCheckTime) > OcclusionCheckInterval)
	{
		LastOcclusionCheckTime = WorldTime;
		static FName NAME_SoundOcclusion = FName(TEXT("SoundOcclusion"));

		// Make sure we ignore the sound's actor geometry
		AActor* IgnoreActor = nullptr;
		if (UAudioComponent* AudioComponentPtr = GetAudioComponent())
		{
			IgnoreActor = AudioComponentPtr->GetOwner();
		}

		FCollisionQueryParams Params(NAME_SoundOcclusion, bUseComplexOcclusionChecks, IgnoreActor);

		if (bOcclusionAsyncTrace)
		{
			WorldPtr->AsyncLineTraceByChannel(SoundLocation, ListenerLocation, ECC_Visibility, Params, FCollisionResponseParams::DefaultResponseParam, &OcclusionTraceDelegate);
		}
		else
		{
			bool bTraceResult = WorldPtr->LineTraceTestByChannel(SoundLocation, ListenerLocation, ECC_Visibility, Params);
			SetIsOccluded(bTraceResult);
		}
	}

	CurrentOcclusionFilterFrequency.Update(WorldPtr->DeltaTimeSeconds);
	CurrentOcclusionVolumeAttenuation.Update(WorldPtr->DeltaTimeSeconds);
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
	if (LastUpdateTime < Listener.InteriorStartTime)
	{
		SourceInteriorVolume = CurrentInteriorVolume;
		SourceInteriorLPF = CurrentInteriorLPF;
		LastUpdateTime = FApp::GetCurrentTime();
	}

	if (Listener.Volume == AudioVolume || !bAllowSpatialization)
	{
		// Ambient and listener in same ambient zone
		CurrentInteriorVolume = FMath::Lerp(SourceInteriorVolume, 1.0f, Listener.InteriorVolumeInterp);
		ParseParams.VolumeMultiplier *= CurrentInteriorVolume;

		CurrentInteriorLPF = FMath::Lerp(SourceInteriorLPF, MAX_FILTER_FREQUENCY, Listener.InteriorLPFInterp);
		ParseParams.AmbientZoneFilterFrequency = CurrentInteriorLPF;
	}
	else
	{
		// Ambient and listener in different ambient zone
		if( Ambient.bIsWorldSettings )
		{
			// The ambient sound is 'outside' - use the listener's exterior volume
			CurrentInteriorVolume = FMath::Lerp(SourceInteriorVolume, Listener.InteriorSettings.ExteriorVolume, Listener.ExteriorVolumeInterp);
			ParseParams.VolumeMultiplier *= CurrentInteriorVolume;

			CurrentInteriorLPF = FMath::Lerp(SourceInteriorLPF, Listener.InteriorSettings.ExteriorLPF, Listener.ExteriorLPFInterp);
			ParseParams.AmbientZoneFilterFrequency = CurrentInteriorLPF;

			UE_LOG(LogAudio, Verbose, TEXT( "Ambient in diff volume, ambient outside. Volume *= %g LPF *= %g (%s)" ),
				CurrentInteriorVolume, CurrentInteriorLPF, ( WaveInstances.Num() > 0 ) ? GetAWaveName(WaveInstances) : TEXT( "NULL" ) );
		}
		else
		{
			// The ambient sound is 'inside' - use the ambient sound's interior volume multiplied with the listeners exterior volume
			CurrentInteriorVolume = FMath::Lerp(SourceInteriorVolume, Ambient.InteriorVolume, Listener.InteriorVolumeInterp);
			CurrentInteriorVolume *= FMath::Lerp(SourceInteriorVolume, Listener.InteriorSettings.ExteriorVolume, Listener.ExteriorVolumeInterp);
			ParseParams.VolumeMultiplier *= CurrentInteriorVolume;

			float AmbientLPFValue = FMath::Lerp(SourceInteriorLPF, Ambient.InteriorLPF, Listener.InteriorLPFInterp);
			float ListenerLPFValue = FMath::Lerp(SourceInteriorLPF, Listener.InteriorSettings.ExteriorLPF, Listener.ExteriorLPFInterp);

			// The current interior LPF value is the less of the LPF due to ambient zone and LPF due to listener settings
			if (AmbientLPFValue < ListenerLPFValue)
			{
				CurrentInteriorLPF = AmbientLPFValue;
				ParseParams.AmbientZoneFilterFrequency = AmbientLPFValue;
			}
			else
			{
				CurrentInteriorLPF = ListenerLPFValue;
				ParseParams.AmbientZoneFilterFrequency = ListenerLPFValue;
			}

			UE_LOG(LogAudio, Verbose, TEXT( "Ambient in diff volume, ambient inside. Volume *= %g LPF *= %g (%s)" ),
				CurrentInteriorVolume, CurrentInteriorLPF, ( WaveInstances.Num() > 0 ) ? GetAWaveName(WaveInstances) : TEXT( "NULL" ) );
		}
	}
}

void FActiveSound::ApplyRadioFilter(const FSoundParseParameters& ParseParams )
{
	check(AudioDevice);
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

void FActiveSound::GetAttenuationListenerData(FAttenuationListenerData& ListenerData, const FSoundParseParameters& ParseParams, const FListener& Listener) const
{
	// Only perform this calculation once and explicitly require a reset.
	if (ListenerData.bDataComputed)
	{
		return;
	}

	const FTransform& SoundTransform = ParseParams.Transform;
	const FVector& ListenerLocation = Listener.Transform.GetTranslation();

	FVector ListenerToSound = SoundTransform.GetTranslation() - ListenerLocation;
	ListenerToSound.ToDirectionAndLength(ListenerData.ListenerToSoundDir, ListenerData.ListenerToSoundDistance);

	ListenerData.AttenuationDistance = 0.0f;

	if ((AttenuationSettings.bAttenuate && AttenuationSettings.AttenuationShape == EAttenuationShape::Sphere) || AttenuationSettings.bAttenuateWithLPF)
	{
		ListenerData.AttenuationDistance = FMath::Max(ListenerData.ListenerToSoundDistance - AttenuationSettings.AttenuationShapeExtents.X, 0.f);
	}

	ListenerData.bDataComputed = true;
}

void FActiveSound::ApplyAttenuation(FSoundParseParameters& ParseParams, const FListener& Listener)
{
	float& Volume = ParseParams.Volume;
	const FTransform& SoundTransform = ParseParams.Transform;
	const FVector& ListenerLocation = Listener.Transform.GetTranslation();

	FAttenuationListenerData ListenerData;

	// Init focus factor to 1.0f in case this sound is ignoring focus
	float FocusFactor = 1.0f;

	// How much we want to scale the listener-emitter distance based on focus
	float DistanceScale = 1.0f;

	// Computer the focus factor if needed
	check(Sound);
	if (AttenuationSettings.bSpatialize && AttenuationSettings.bEnableListenerFocus && !Sound->bIgnoreFocus)
	{
		// Get the attenuation listener data
		GetAttenuationListenerData(ListenerData, ParseParams, Listener);

		// Compute the focus factor
		const FVector& ListenerForwardDir = Listener.Transform.GetUnitAxis(EAxis::X);
		const FVector& ListenerToSoundDir = ListenerData.ListenerToSoundDir;
		const float FocusDotProduct = FVector::DotProduct(ListenerForwardDir, ListenerToSoundDir);
		const float FocusAngle = FMath::RadiansToDegrees(FMath::Acos(FocusDotProduct));

		const float FocusAzimuth = FMath::Clamp(AttenuationSettings.FocusAzimuth, 0.0f, 360.0f);
		const float NonFocusAzimuth = FMath::Clamp(AttenuationSettings.NonFocusAzimuth, 0.0f, 360.0f);

		if (FocusAzimuth != NonFocusAzimuth)
		{
			FocusFactor = (FocusAngle - FocusAzimuth) / (NonFocusAzimuth - FocusAzimuth);
			FocusFactor = FMath::Clamp(FocusFactor, 0.0f, 1.0f);
		}
		else
		{
			FocusFactor = 0.0f;
			if (FocusAngle < FocusAzimuth)
			{
				FocusFactor = 1.0f;
			}
		}

		// Get the volume scale to apply the volume calculation based on the focus factor
		const float NonFocusVolumeAttenuation = FMath::Clamp(AttenuationSettings.NonFocusVolumeAttenuation, 0.0f, 1.0f);
		const float FocusVolumeAttenuation = FMath::Lerp(1.0f, NonFocusVolumeAttenuation, FocusFactor);
		Volume *= FocusVolumeAttenuation;

		// Scale the volume-weighted priority scale value we use for sorting this sound for voice-stealing
		const float NonFocusPriorityScale = FMath::Max(AttenuationSettings.NonFocusPriorityScale, 0.0f);
		const float FocusPriorityScale = FMath::Max(AttenuationSettings.FocusPriorityScale, 0.0f);
		const float PriorityScale = FMath::Lerp(FocusPriorityScale, NonFocusPriorityScale, FocusFactor);
		ParseParams.Priority *= PriorityScale;

		// Get the distance scale to use when computing distance-calculations for 3d atttenuation
		const float FocusDistanceScale = FMath::Max(AttenuationSettings.FocusDistanceScale, 0.0f);
		const float NonFocusDistanceScale = FMath::Max(AttenuationSettings.NonFocusDistanceScale, 0.0f);
		DistanceScale = FMath::Lerp(FocusDistanceScale, NonFocusDistanceScale, FocusFactor);

		// Update the sound's distance scale value for checks based on max distance
		Sound->SetFocusDistanceScale(DistanceScale);
	}

	// Attenuate the volume based on the model
	if (AttenuationSettings.bAttenuate)
	{
		switch (AttenuationSettings.AttenuationShape)
		{
			case EAttenuationShape::Sphere:
			{
				// Update attenuation data in-case it hasn't been updated
				GetAttenuationListenerData(ListenerData, ParseParams, Listener);
				Volume *= AttenuationSettings.AttenuationEval(ListenerData.AttenuationDistance, AttenuationSettings.FalloffDistance, DistanceScale);
				break;
			}

			case EAttenuationShape::Box:
			Volume *= AttenuationSettings.AttenuationEvalBox(SoundTransform, ListenerLocation, DistanceScale);
			break;

			case EAttenuationShape::Capsule:
			Volume *= AttenuationSettings.AttenuationEvalCapsule(SoundTransform, ListenerLocation, DistanceScale);
			break;

			case EAttenuationShape::Cone:
			Volume *= AttenuationSettings.AttenuationEvalCone(SoundTransform, ListenerLocation, DistanceScale);
			break;

			default:
			check(false);
		}
	}

	// Attenuate with the low pass filter if necessary
	if (AttenuationSettings.bAttenuateWithLPF)
	{
		GetAttenuationListenerData(ListenerData, ParseParams, Listener);

		// Attenuate with the low pass filter if necessary
		FVector2D InputRange(AttenuationSettings.LPFRadiusMin, AttenuationSettings.LPFRadiusMax);
		FVector2D OutputRange(AttenuationSettings.LPFFrequencyAtMin, AttenuationSettings.LPFFrequencyAtMax);
		float AttenuationFilterFrequency = FMath::GetMappedRangeValueClamped(InputRange, OutputRange, ListenerData.AttenuationDistance);

		// Only apply the attenuation filter frequency if it results in a lower attenuation filter frequency than is already being used by ParseParams (the struct pass into the sound cue node tree)
		// This way, subsequently chained attenuation nodes in a sound cue will only result in the lowest frequency of the set.
		if (AttenuationFilterFrequency < ParseParams.AttenuationFilterFrequency)
		{
			ParseParams.AttenuationFilterFrequency = AttenuationFilterFrequency;
		}
	}

	ParseParams.OmniRadius = AttenuationSettings.OmniRadius;
	ParseParams.StereoSpread = AttenuationSettings.StereoSpread;
	ParseParams.bUseSpatialization |= AttenuationSettings.bSpatialize;

	if (AttenuationSettings.SpatializationAlgorithm == SPATIALIZATION_Default && AudioDevice->IsHRTFEnabledForAll())
	{
		ParseParams.SpatializationAlgorithm = SPATIALIZATION_HRTF;
	}
	else
	{
		ParseParams.SpatializationAlgorithm = AttenuationSettings.SpatializationAlgorithm;
	}
}