// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ActiveSound.h"
#include "AudioDevice.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundNodeAttenuation.h"
#include "SubtitleManager.h"
#include "AudioThread.h"


FActiveSound::FActiveSound()
	: World(nullptr)
	, WorldID(0)
	, Sound(nullptr)
	, AudioComponentID(0)
	, OwnerID(0)
	, AudioDevice(nullptr)
	, ConcurrencyGroupID(0)
	, ConcurrencyGeneration(0)
	, ConcurrencySettings(nullptr)
	, SoundClassOverride(nullptr)
	, bHasCheckedOcclusion(false)
	, bIsTraceDelegateBound(false)
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
	, bIsPreviewSound(false)
	, bGotInteriorSettings(false)
	, bApplyInteriorVolumes(false)
#if !(NO_LOGGING || UE_BUILD_SHIPPING || UE_BUILD_TEST)
	, bWarnedAboutOrphanedLooping(false)
#endif
	, bEnableLowPassFilter(false)
	, bOcclusionAsyncTrace(true)
	, bIsAudible(true)
	, UserIndex(0)
	, bIsOccluded(false)
	, bAsyncOcclusionPending(false)
	, PlaybackTime(0.f)
	, RequestedStartTime(0.f)
	, CurrentAdjustVolumeMultiplier(1.f)
	, TargetAdjustVolumeMultiplier(1.f)
	, TargetAdjustVolumeStopTime(-1.f)
	, VolumeMultiplier(1.f)
	, PitchMultiplier(1.f)
	, LowPassFilterFrequency(MAX_FILTER_FREQUENCY)
	, CurrentOcclusionFilterFrequency(MAX_FILTER_FREQUENCY)
	, CurrentOcclusionVolumeAttenuation(1.0f)
	, ConcurrencyVolumeScale(1.f)
	, ConcurrencyDuckingVolumeScale(1.f)
	, SubtitlePriority(DEFAULT_SUBTITLE_PRIORITY)
	, Priority(1.0f)
	, FocusPriorityScale(1.0f)
	, FocusDistanceScale(1.0f)
	, VolumeConcurrency(0.0f)
	, OcclusionCheckInterval(0.f)
	, LastOcclusionCheckTime(0.f)
	, MaxDistance(WORLD_MAX)
	, LastLocation(FVector::ZeroVector)
	, AudioVolumeID(0)
	, LastUpdateTime(0.f)
	, SourceInteriorVolume(1.f)
	, SourceInteriorLPF(MAX_FILTER_FREQUENCY)
	, CurrentInteriorVolume(1.f)
	, CurrentInteriorLPF(MAX_FILTER_FREQUENCY)
	, ClosestListenerPtr(nullptr)
{
}

FActiveSound::~FActiveSound()
{
	ensureMsgf(WaveInstances.Num() == 0, TEXT("Destroyed an active sound that had active wave instances."));
	check(CanDelete());
}

FArchive& operator<<( FArchive& Ar, FActiveSound* ActiveSound )
{
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << ActiveSound->Sound;
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
	Collector.AddReferencedObject(SoundClassOverride);
	Collector.AddReferencedObject(ConcurrencySettings);

	for (FAudioComponentParam& Param : InstanceParameters)
	{
		if (Param.SoundWaveParam)
		{
			Collector.AddReferencedObject(Param.SoundWaveParam);
		}
	}
}

void FActiveSound::SetWorld(UWorld* InWorld)
{
	check(IsInGameThread());

	World = InWorld;
	WorldID = (InWorld ? InWorld->GetUniqueID() : 0);
}

void FActiveSound::SetSound(USoundBase* InSound)
{
	check(IsInGameThread());

	Sound = InSound;
	bApplyInteriorVolumes = (SoundClassOverride && SoundClassOverride->Properties.bApplyAmbientVolumes)
							|| (Sound && Sound->ShouldApplyInteriorVolumes());
}

void FActiveSound::SetSoundClass(USoundClass* SoundClass)
{
	check(IsInGameThread());

	SoundClassOverride = SoundClass;
	bApplyInteriorVolumes = (SoundClassOverride && SoundClassOverride->Properties.bApplyAmbientVolumes)
							|| (Sound && Sound->ShouldApplyInteriorVolumes());
}

void FActiveSound::SetAudioComponent(UAudioComponent* Component)
{
	check(IsInGameThread());

	AActor* Owner = Component->GetOwner();

	AudioComponentID = Component->GetAudioComponentID();
	AudioComponentName = Component->GetFName();

	if (Owner)
	{
		OwnerID = Owner->GetUniqueID();
		OwnerName = Owner->GetFName();
	}
	else
	{
		OwnerID = 0;
		OwnerName = NAME_None;
	}

	}

FString FActiveSound::GetAudioComponentName() const
{
	return (AudioComponentID > 0 ? AudioComponentName.ToString() : TEXT("NO COMPONENT"));
}

FString FActiveSound::GetOwnerName() const
{
	return (OwnerID > 0 ? OwnerName.ToString() : TEXT("None"));
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

	return nullptr;
	}

int32 FActiveSound::FindClosestListener( const TArray<FListener>& InListeners ) const
{
	return FAudioDevice::FindClosestListenerIndex(Transform, InListeners);
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

	// splitscreen support:
	// we always pass the 'primary' listener (viewport 0) to the sound nodes and the underlying audio system
	// then move the AudioComponent's CurrentLocation so that its position relative to that Listener is the same as its real position is relative to the closest Listener

	int32 ClosestListenerIndex = 0;

	const TArray<FListener>& Listeners = AudioDevice->GetListeners();

	if (Listeners.Num() > 1)
	{
		SCOPE_CYCLE_COUNTER( STAT_AudioFindNearestLocation );
		ClosestListenerIndex = FindClosestListener(Listeners);
	}

	// Cache the closest listener ptr 
	ClosestListenerPtr = &Listeners[ClosestListenerIndex];

	// The apparent max distance factors the actual max distance of the sound scaled with the distance scale due to focus effects
	float ApparentMaxDistance = MaxDistance * FocusDistanceScale;

	// Update whether or not his sound is out of range
	bIsAudible = AudioDevice->LocationIsAudible(Transform.GetTranslation(), ClosestListenerPtr->Transform, ApparentMaxDistance);

	FSoundParseParameters ParseParams;
	ParseParams.Transform = Transform;
	ParseParams.StartTime = RequestedStartTime;

	// Default values.
	// It's all Multiplicative!  So now people are all modifying the multiplier values via various means
	// (even after the Sound has started playing, and this line takes them all into account and gives us
	// final value that is correct
	UpdateAdjustVolumeMultiplier(DeltaTime);

	// If the sound is a preview sound, then ignore the transient master volume and application volume
	float MasterVolume = AudioDevice->GetTransientMasterVolume(); 
	float ApplicationVolume = FApp::GetVolumeMultiplier();
	if (bIsPreviewSound)
	{
		MasterVolume = 1.0f;
		ApplicationVolume = 1.0f;
	}

	ParseParams.VolumeMultiplier = VolumeMultiplier * Sound->GetVolumeMultiplier() * CurrentAdjustVolumeMultiplier * MasterVolume * ApplicationVolume * ConcurrencyVolumeScale;

	ParseParams.Priority = Priority;
	ParseParams.Pitch *= PitchMultiplier * Sound->GetPitchMultiplier();
	ParseParams.bEnableLowPassFilter = bEnableLowPassFilter;
	ParseParams.LowPassFilterFrequency = LowPassFilterFrequency;
	ParseParams.SoundClass = GetSoundClass();

	if (bApplyInteriorVolumes)
	{
		// Additional inside/outside processing for ambient sounds
		// If we aren't in a world there is no interior volumes to be handled.
		HandleInteriorVolumes(*ClosestListenerPtr, ParseParams);
	}

	// for velocity-based effects like doppler
	if (DeltaTime > 0.f)
	{
		ParseParams.Velocity = (ParseParams.Transform.GetTranslation() - LastLocation) / DeltaTime;
		LastLocation = ParseParams.Transform.GetTranslation();
	}

	TArray<FWaveInstance*> ThisSoundsWaveInstances;

	// Recurse nodes, have SoundWave's create new wave instances and update bFinished unless we finished fading out.
	bFinished = true;
	if (!bFadingOut || (PlaybackTime <= TargetAdjustVolumeStopTime))
	{
		if (bHasAttenuationSettings)
		{
			ApplyAttenuation(ParseParams, *ClosestListenerPtr);
		}

		// if the closest listener is not the primary one, transform the sound transform so it's panned relative to primary listener position
		if (ClosestListenerIndex != 0)
		{
			const FListener& Listener = Listeners[0];
			ParseParams.Transform = ParseParams.Transform * ClosestListenerPtr->Transform.Inverse() * Listener.Transform;
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
		if (ConcurrencySettingsToApply && ConcurrencySettingsToApply->ResolutionRule == EMaxConcurrentResolutionRule::StopQuietest)
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
		FSoundSource* Source = AudioDevice->GetSoundSource(WaveInstance);
		if( Source )
		{
			Source->Stop();
		}

		// Dequeue subtitles for this sounds on the game thread
		DECLARE_CYCLE_STAT(TEXT("FGameThreadAudioTask.KillSubtitles"), STAT_AudioKillSubtitles, STATGROUP_TaskGraphTasks);
		const PTRINT WaveInstanceID = (PTRINT)WaveInstance;
		FAudioThread::RunCommandOnGameThread([WaveInstanceID]()
		{
			FSubtitleManager::GetSubtitleManager()->KillSubtitles(WaveInstanceID);
		}, GET_STATID(STAT_AudioKillSubtitles));

		delete WaveInstance;

		// Null the entry out temporarily as later Stop calls could try to access this structure
		WaveInstance = nullptr;
	}
	WaveInstances.Empty();

	AudioDevice->RemoveActiveSound(this);
}

FWaveInstance* FActiveSound::FindWaveInstance( const UPTRINT WaveInstanceHash )
{
	FWaveInstance** WaveInstance = WaveInstances.Find(WaveInstanceHash);
	return (WaveInstance ? *WaveInstance : nullptr);
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

void FActiveSound::UpdateOcclusion(const FAttenuationSettings* AttenuationSettingsPtr)
{
	float InterpolationTime = bHasCheckedOcclusion ? AttenuationSettingsPtr->OcclusionInterpolationTime : 0.0f;
	bHasCheckedOcclusion = true;

	if (bIsOccluded)
	{
		if (CurrentOcclusionFilterFrequency.GetTargetValue() > AttenuationSettingsPtr->OcclusionLowPassFilterFrequency)
		{
			CurrentOcclusionFilterFrequency.Set(AttenuationSettingsPtr->OcclusionLowPassFilterFrequency, InterpolationTime);
		}

		if (CurrentOcclusionVolumeAttenuation.GetTargetValue() > AttenuationSettingsPtr->OcclusionVolumeAttenuation)
		{
			CurrentOcclusionVolumeAttenuation.Set(AttenuationSettingsPtr->OcclusionVolumeAttenuation, InterpolationTime);
		}
	}
	else
	{
		CurrentOcclusionFilterFrequency.Set(MAX_FILTER_FREQUENCY, InterpolationTime);
		CurrentOcclusionVolumeAttenuation.Set(1.0f, InterpolationTime);
	}

	UWorld* WorldPtr = World.Get();
	CurrentOcclusionFilterFrequency.Update(WorldPtr->DeltaTimeSeconds);
	CurrentOcclusionVolumeAttenuation.Update(WorldPtr->DeltaTimeSeconds);
}

void FActiveSound::OcclusionTraceDone(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum)
{
	// Look for any results that resulted in a blocking hit
	bIsOccluded = false;
	for (const FHitResult& HitResult : TraceDatum.OutHits)
	{
		if (HitResult.bBlockingHit)
		{
			bIsOccluded = true;
			break;
		}
	}

	bAsyncOcclusionPending = false;
}

void FActiveSound::CheckOcclusion(const FVector ListenerLocation, const FVector SoundLocation, const FAttenuationSettings* AttenuationSettingsPtr)
{
	check(AttenuationSettingsPtr);
	check(AttenuationSettingsPtr->bEnableOcclusion);

	UWorld* WorldPtr = World.Get();
	float WorldTime = WorldPtr->GetTimeSeconds();

	if ((WorldTime - LastOcclusionCheckTime) > OcclusionCheckInterval)
	{
		LastOcclusionCheckTime = WorldTime;
		static FName NAME_SoundOcclusion = FName(TEXT("SoundOcclusion"));

		FCollisionQueryParams Params(NAME_SoundOcclusion, AttenuationSettingsPtr->bUseComplexCollisionForOcclusion);
		if (OwnerID > 0)
		{
			Params.AddIgnoredActor(OwnerID);
		}

		if (bOcclusionAsyncTrace)
		{
			ECollisionChannel OcclusionTraceChannel = AttenuationSettingsPtr->OcclusionTraceChannel;

			// If the audio thread is running, then only do a sync trace
			if (FAudioThread::IsAudioThreadRunning())
			{
				bIsOccluded = WorldPtr->LineTraceTestByChannel(SoundLocation, ListenerLocation, OcclusionTraceChannel, Params);
			}
			else
			{
				// Check if we've not already bound our trace delegate
				if (!bIsTraceDelegateBound)
				{
					bIsTraceDelegateBound = true;

					// Bind our async occlusion trace delegate (so next update we'll have it bound)
					OcclusionTraceDelegate.BindRaw(this, &FActiveSound::OcclusionTraceDone);

					// Only do async occlusion trace if we've already made one. The first trace must be synchronous to avoid issues with sounds starting playing as occluded
					bIsOccluded = WorldPtr->LineTraceTestByChannel(SoundLocation, ListenerLocation, OcclusionTraceChannel, Params);
				}
				// don't need to do another async trace if we've already got one pending
				else if (!bAsyncOcclusionPending)
				{
					bAsyncOcclusionPending = true;
					WorldPtr->AsyncLineTraceByChannel(EAsyncTraceType::Test, SoundLocation, ListenerLocation, OcclusionTraceChannel, Params, FCollisionResponseParams::DefaultResponseParam, &OcclusionTraceDelegate);
				}
			}
		}
	}

	UpdateOcclusion(AttenuationSettingsPtr);
}

const TCHAR* GetAWaveName(TMap<UPTRINT, struct FWaveInstance*> WaveInstances)
{
	TArray<FWaveInstance*> WaveInstanceArray;
	WaveInstances.GenerateValueArray(WaveInstanceArray);
	return *WaveInstanceArray[0]->WaveData->GetName();
}

void FActiveSound::HandleInteriorVolumes( const FListener& Listener, FSoundParseParameters& ParseParams )
{
	// Get the settings of the ambient sound
	if (!bGotInteriorSettings || (ParseParams.Transform.GetTranslation() - LastLocation).SizeSquared() > KINDA_SMALL_NUMBER)
	{
		FAudioDevice::FAudioVolumeSettings AudioVolumeSettings;
		AudioDevice->GetAudioVolumeSettings(WorldID, ParseParams.Transform.GetTranslation(), AudioVolumeSettings);

		InteriorSettings = AudioVolumeSettings.InteriorSettings;
		AudioVolumeID = AudioVolumeSettings.AudioVolumeID;
		bGotInteriorSettings = true;
	}

	// Check to see if we've moved to a new audio volume
	if (LastUpdateTime < Listener.InteriorStartTime)
	{
		SourceInteriorVolume = CurrentInteriorVolume;
		SourceInteriorLPF = CurrentInteriorLPF;
		LastUpdateTime = FApp::GetCurrentTime();
	}

	if (Listener.AudioVolumeID == AudioVolumeID || !bAllowSpatialization)
	{
		// Ambient and listener in same ambient zone
		CurrentInteriorVolume = FMath::Lerp(SourceInteriorVolume, 1.0f, Listener.InteriorVolumeInterp);
		ParseParams.InteriorVolumeMultiplier = CurrentInteriorVolume;

		CurrentInteriorLPF = FMath::Lerp(SourceInteriorLPF, MAX_FILTER_FREQUENCY, Listener.InteriorLPFInterp);
		ParseParams.AmbientZoneFilterFrequency = CurrentInteriorLPF;
	}
	else
	{
		// Ambient and listener in different ambient zone
		if( InteriorSettings.bIsWorldSettings )
		{
			// The ambient sound is 'outside' - use the listener's exterior volume
			CurrentInteriorVolume = FMath::Lerp(SourceInteriorVolume, Listener.InteriorSettings.ExteriorVolume, Listener.ExteriorVolumeInterp);
			ParseParams.InteriorVolumeMultiplier = CurrentInteriorVolume;

			CurrentInteriorLPF = FMath::Lerp(SourceInteriorLPF, Listener.InteriorSettings.ExteriorLPF, Listener.ExteriorLPFInterp);
			ParseParams.AmbientZoneFilterFrequency = CurrentInteriorLPF;
		}
		else
		{
			// The ambient sound is 'inside' - use the ambient sound's interior volume multiplied with the listeners exterior volume
			CurrentInteriorVolume = FMath::Lerp(SourceInteriorVolume, InteriorSettings.InteriorVolume, Listener.InteriorVolumeInterp);
			CurrentInteriorVolume *= FMath::Lerp(SourceInteriorVolume, Listener.InteriorSettings.ExteriorVolume, Listener.ExteriorVolumeInterp);
			ParseParams.InteriorVolumeMultiplier = CurrentInteriorVolume;

			float AmbientLPFValue = FMath::Lerp(SourceInteriorLPF, InteriorSettings.InteriorLPF, Listener.InteriorLPFInterp);
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
		for( const FAudioComponentParam& P : InstanceParameters )
		{
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
		for( FAudioComponentParam& P : InstanceParameters )
		{
			if( P.ParamName == InName )
			{
				P.FloatParam = InFloat;
				return;
			}
		}

		// We didn't find one, so create a new one.
		const int32 NewParamIndex = InstanceParameters.AddDefaulted();
		InstanceParameters[ NewParamIndex ].ParamName = InName;
		InstanceParameters[ NewParamIndex ].FloatParam = InFloat;
	}
}

bool FActiveSound::GetWaveParameter( const FName InName, USoundWave*& OutWave ) const
{
	// Always fail if we pass in no name.
	if( InName != NAME_None )
	{
		for( const FAudioComponentParam& P : InstanceParameters )
		{
			if( P.ParamName == InName )
			{
				OutWave = P.SoundWaveParam;
				return true;
			}
		}
	}

	return false;
}

void FActiveSound::SetWaveParameter( const FName InName, USoundWave* InWave )
{
	if( InName != NAME_None )
	{
		// First see if an entry for this name already exists
		for( FAudioComponentParam& P : InstanceParameters )
		{
			if( P.ParamName == InName )
			{
				P.SoundWaveParam = InWave;
				return;
			}
		}

		// We didn't find one, so create a new one.
		const int32 NewParamIndex = InstanceParameters.AddDefaulted();
		InstanceParameters[ NewParamIndex ].ParamName = InName;
		InstanceParameters[ NewParamIndex ].SoundWaveParam = InWave;
	}
}

bool FActiveSound::GetBoolParameter( const FName InName, bool& OutBool ) const
{
	// Always fail if we pass in no name.
	if( InName != NAME_None )
	{
		for( const FAudioComponentParam& P : InstanceParameters )
		{
			if( P.ParamName == InName )
			{
				OutBool = P.BoolParam;
				return true;
			}
		}
	}

	return false;
}

void FActiveSound::SetBoolParameter( const FName InName, const bool InBool )
{
	if( InName != NAME_None )
	{
		// First see if an entry for this name already exists
		for( FAudioComponentParam& P : InstanceParameters )
		{
			if( P.ParamName == InName )
			{
				P.BoolParam = InBool;
				return;
			}
		}

		// We didn't find one, so create a new one.
		const int32 NewParamIndex = InstanceParameters.AddDefaulted();
		InstanceParameters[ NewParamIndex ].ParamName = InName;
		InstanceParameters[ NewParamIndex ].BoolParam = InBool;
	}
}

int32 FActiveSound::GetIntParameter( const FName InName, int32& OutInt ) const
{
	// Always fail if we pass in no name.
	if( InName != NAME_None )
	{
		for( const FAudioComponentParam& P : InstanceParameters )
		{
			if( P.ParamName == InName )
			{
				OutInt = P.IntParam;
				return true;
			}
		}
	}

	return false;
}

void FActiveSound::SetIntParameter( const FName InName, const int32 InInt )
{
	if( InName != NAME_None )
	{
		// First see if an entry for this name already exists
		for( FAudioComponentParam& P : InstanceParameters )
		{
			if( P.ParamName == InName )
			{
				P.IntParam = InInt;
				return;
			}
		}

		// We didn't find one, so create a new one.
		const int32 NewParamIndex = InstanceParameters.AddDefaulted();
		InstanceParameters[ NewParamIndex ].ParamName = InName;
		InstanceParameters[ NewParamIndex ].IntParam = InInt;
	}
}

void FActiveSound::SetSoundParameter(const FAudioComponentParam& Param)
{
	if (Param.ParamName != NAME_None)
	{
		// First see if an entry for this name already exists
		for( FAudioComponentParam& P : InstanceParameters )
		{
			if (P.ParamName == Param.ParamName)
			{
				P = Param;
				return;
			}
		}

		// We didn't find one, so create a new one.
		const int32 NewParamIndex = InstanceParameters.Add(Param);
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

void FActiveSound::ApplyAttenuation(FSoundParseParameters& ParseParams, const FListener& Listener, const FAttenuationSettings* SettingsAttenuationNode)
{
	float& Volume = ParseParams.Volume;
	const FTransform& SoundTransform = ParseParams.Transform;
	const FVector& ListenerLocation = Listener.Transform.GetTranslation();

	// Get the attenuation settings to use for this application to the active sound
	const FAttenuationSettings* Settings = SettingsAttenuationNode ? SettingsAttenuationNode : &AttenuationSettings;

	FAttenuationListenerData ListenerData;

	// Get the current focus factor
	const float FocusFactor = AudioDevice->GetFocusFactor(ListenerData, Sound, SoundTransform, *Settings, &Listener.Transform);

	// Reset distance and priority scale to 1.0 in case changed in editor
	FocusDistanceScale = 1.0f;
	FocusPriorityScale = 1.0f;
	
	// Computer the focus factor if needed
	check(Sound);
	if (Settings->bSpatialize && Settings->bEnableListenerFocus && !Sound->bIgnoreFocus)
	{
		const FGlobalFocusSettings& FocusSettings = AudioDevice->GetGlobalFocusSettings();

		// Get the volume scale to apply the volume calculation based on the focus factor
		const float FocusVolumeAttenuation = Settings->GetFocusAttenuation(FocusSettings, FocusFactor);
		Volume *= FocusVolumeAttenuation;

		// Scale the volume-weighted priority scale value we use for sorting this sound for voice-stealing
		FocusPriorityScale = Settings->GetFocusPriorityScale(FocusSettings, FocusFactor);
		ParseParams.Priority *= FocusPriorityScale;

		// Get the distance scale to use when computing distance-calculations for 3d attenuation
		FocusDistanceScale = Settings->GetFocusDistanceScale(FocusSettings, FocusFactor);
	}

	// Attenuate the volume based on the model
	if (Settings->bAttenuate)
	{
		switch (Settings->AttenuationShape)
		{
			case EAttenuationShape::Sphere:
			{
				// Update attenuation data in-case it hasn't been updated
				AudioDevice->GetAttenuationListenerData(ListenerData, SoundTransform, *Settings, &Listener.Transform);
				Volume *= Settings->AttenuationEval(ListenerData.AttenuationDistance, Settings->FalloffDistance, FocusDistanceScale);
				break;
			}

			case EAttenuationShape::Box:
			Volume *= Settings->AttenuationEvalBox(SoundTransform, ListenerLocation, FocusDistanceScale);
			break;

			case EAttenuationShape::Capsule:
			Volume *= Settings->AttenuationEvalCapsule(SoundTransform, ListenerLocation, FocusDistanceScale);
			break;

			case EAttenuationShape::Cone:
			Volume *= Settings->AttenuationEvalCone(SoundTransform, ListenerLocation, FocusDistanceScale);
			break;

			default:
			check(false);
		}
	}

	// Only do occlusion traces if the sound is audible
	if (Settings->bEnableOcclusion && Volume > 0.0f && !AudioDevice->IsAudioDeviceMuted())
	{
		check(ClosestListenerPtr);
		CheckOcclusion(ClosestListenerPtr->Transform.GetTranslation(), ParseParams.Transform.GetTranslation(), Settings);

		// Apply the volume attenuation due to occlusion (using the interpolating dynamic parameter)
		ParseParams.VolumeMultiplier *= CurrentOcclusionVolumeAttenuation.GetValue();

		ParseParams.bIsOccluded = bIsOccluded;
		ParseParams.OcclusionFilterFrequency = CurrentOcclusionFilterFrequency.GetValue();
	}

	// Attenuate with the low pass filter if necessary
	if (Settings->bAttenuateWithLPF)
	{
		AudioDevice->GetAttenuationListenerData(ListenerData, SoundTransform, *Settings, &Listener.Transform);

		// Attenuate with the low pass filter if necessary
		FVector2D InputRange(Settings->LPFRadiusMin, Settings->LPFRadiusMax);
		FVector2D OutputRange(Settings->LPFFrequencyAtMin, Settings->LPFFrequencyAtMax);
		float AttenuationFilterFrequency = FMath::GetMappedRangeValueClamped(InputRange, OutputRange, ListenerData.AttenuationDistance);

		// Only apply the attenuation filter frequency if it results in a lower attenuation filter frequency than is already being used by ParseParams (the struct pass into the sound cue node tree)
		// This way, subsequently chained attenuation nodes in a sound cue will only result in the lowest frequency of the set.
		if (AttenuationFilterFrequency < ParseParams.AttenuationFilterFrequency)
		{
			ParseParams.AttenuationFilterFrequency = AttenuationFilterFrequency;
		}
	}

	ParseParams.OmniRadius = Settings->OmniRadius;
	ParseParams.StereoSpread = Settings->StereoSpread;
	ParseParams.bUseSpatialization |= Settings->bSpatialize;

	if (Settings->SpatializationAlgorithm == SPATIALIZATION_Default && AudioDevice->IsHRTFEnabledForAll())
	{
		ParseParams.SpatializationAlgorithm = SPATIALIZATION_HRTF;
	}
	else
	{
		ParseParams.SpatializationAlgorithm = Settings->SpatializationAlgorithm;
	}
}