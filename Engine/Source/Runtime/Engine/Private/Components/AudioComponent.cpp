// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "SoundDefinitions.h"
#include "Sound/SoundNodeAttenuation.h"
#include "Sound/SoundCue.h"
#include "SubtitleManager.h"
#include "Audio.h"

/*-----------------------------------------------------------------------------
UAudioComponent implementation.
-----------------------------------------------------------------------------*/
UAudioComponent::UAudioComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoDestroy = false;
	bAutoActivate = true;
	bAllowSpatialization = true;
	bStopWhenOwnerDestroyed = true;
	bNeverNeedsRenderUpdate = true;
	bWantsOnUpdateTransform = true;
#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif
	VolumeMultiplier = 1.f;
	bOverridePriority = false;
	Priority = 1.f;
	PitchMultiplier = 1.f;
	VolumeModulationMin = 1.f;
	VolumeModulationMax = 1.f;
	PitchModulationMin = 1.f;
	PitchModulationMax = 1.f;
	bEnableLowPassFilter = false;
	OcclusionVolumeAttenuation = 1.0f;
	LowPassFilterFrequency = MAX_FILTER_FREQUENCY;
	bEnableOcclusionChecks = false;
	bUseComplexCollisionChecks = false;
	OcclusionLowPassFilterFrequency = MAX_FILTER_FREQUENCY;
	OcclusionInterpolationTime = 0.1f;
	OcclusionCheckInterval = 0.1f;
	ActiveCount = 0;
}

FString UAudioComponent::GetDetailedInfoInternal( void ) const
{
	FString Result;

	if (Sound != nullptr)
	{
		Result = Sound->GetPathName(nullptr);
	}
	else
	{
		Result = TEXT( "No_Sound" );
	}

	return Result;
}

void UAudioComponent::PostLoad()
{
	const int32 LinkerUE4Version = GetLinkerUE4Version();

	// Translate the old HighFrequencyGainMultiplier value to the new LowPassFilterFrequency value
	if (LinkerUE4Version < VER_UE4_USE_LOW_PASS_FILTER_FREQ)
	{
		if (HighFrequencyGainMultiplier_DEPRECATED > 0.0f &&  HighFrequencyGainMultiplier_DEPRECATED < 1.0f)
		{
			bEnableLowPassFilter = true;

			// This seems like it wouldn't make sense, but the original implementation for HighFrequencyGainMultiplier (a number between 0.0 and 1.0).
			// In earlier versions, this was *not* used as a high frequency gain, but instead converted to a frequency value between 0.0 and 6000.0
			// then "converted" to a radian frequency value using an equation taken from XAudio2 documentation. To recover
			// the original intended frequency (approximately), we'll run it through that equation, then scale radian value by the max filter frequency.

			float FilterConstant = 2.0f * FMath::Sin(PI * 6000.0f * HighFrequencyGainMultiplier_DEPRECATED / 48000);
			LowPassFilterFrequency = FilterConstant * MAX_FILTER_FREQUENCY;
		}
	}

	Super::PostLoad();
}

#if WITH_EDITORONLY_DATA
void UAudioComponent::OnRegister()
{
	Super::OnRegister();

	UpdateSpriteTexture();
}
#endif

void UAudioComponent::OnUnregister()
{
	// Route OnUnregister event.
	Super::OnUnregister();

	// Don't stop audio and clean up component if owner has been destroyed (default behaviour). This function gets
	// called from AActor::ClearComponents when an actor gets destroyed which is not usually what we want for one-
	// shot sounds.
	AActor* Owner = GetOwner();
	if (!Owner || bStopWhenOwnerDestroyed)
	{
		Stop();
	}
}

const UObject* UAudioComponent::AdditionalStatObject() const
{
	return Sound;
}

void UAudioComponent::SetSound( USoundBase* NewSound )
{
	const bool bPlay = IsPlaying();

	// If this is an auto destroy component we need to prevent it from being auto-destroyed since we're really just restarting it
	const bool bWasAutoDestroy = bAutoDestroy;
	bAutoDestroy = false;
	Stop();
	bAutoDestroy = bWasAutoDestroy;

	Sound = NewSound;

	if (bPlay)
	{
		Play();
	}
}

void UAudioComponent::OnUpdateTransform(bool bSkipPhysicsMove, ETeleportType Teleport)
{
	Super::OnUpdateTransform(bSkipPhysicsMove, Teleport);

	if (bIsActive && !bPreviewComponent)
	{
		if (FAudioDevice * AudioDevice = GetAudioDevice())
		{
			FActiveSound* ActiveSound = AudioDevice->FindActiveSound(this);
			if (ActiveSound)
			{
				FScopeCycleCounterUObject ComponentScope(ActiveSound->Sound);
				ActiveSound->Transform = ComponentToWorld;
			}
		}
	}
};

void UAudioComponent::Play(float StartTime)
{
	PlayInternal(StartTime);
}

void UAudioComponent::PlayInternal(const float StartTime, const float FadeInDuration, const float FadeVolumeLevel)
{
	UWorld* World = GetWorld();

	UE_LOG(LogAudio, Verbose, TEXT("%g: Playing AudioComponent : '%s' with Sound: '%s'"), World ? World->GetAudioTimeSeconds() : 0.0f, *GetFullName(), Sound ? *Sound->GetName() : TEXT("nullptr"));

	if (bIsActive)
	{
		// If this is an auto destroy component we need to prevent it from being auto-destroyed since we're really just restarting it
		bool bCurrentAutoDestroy = bAutoDestroy;
		bAutoDestroy = false;
		if (!bShouldRemainActiveIfDropped)
		{
			Stop();
		}
		bAutoDestroy = bCurrentAutoDestroy;
	}

	// Bump ActiveCount... this is used to determine if an audio component is still active after "finishing"
	++ActiveCount;

	if (Sound && (World == nullptr || World->bAllowAudioPlayback))
	{
		if (FAudioDevice* AudioDevice = GetAudioDevice())
		{
			FActiveSound NewActiveSound;
			NewActiveSound.SetAudioComponent(this);
			NewActiveSound.World = GetWorld();
			NewActiveSound.Sound = Sound;
			NewActiveSound.SoundClassOverride = SoundClassOverride;
			NewActiveSound.ConcurrencySettings = ConcurrencySettings;

			NewActiveSound.VolumeMultiplier = (VolumeModulationMax + ((VolumeModulationMin - VolumeModulationMax) * FMath::SRand())) * VolumeMultiplier;
			// The priority used for the active sound is the audio component's priority scaled with the sound's priority
			if (bOverridePriority)
			{
				NewActiveSound.Priority = Priority;
			}
			else
			{
				NewActiveSound.Priority = Sound->Priority;
			}
			NewActiveSound.PitchMultiplier = (PitchModulationMax + ((PitchModulationMin - PitchModulationMax) * FMath::SRand())) * PitchMultiplier;
			NewActiveSound.bEnableLowPassFilter = bEnableLowPassFilter;
			NewActiveSound.LowPassFilterFrequency = LowPassFilterFrequency;
			NewActiveSound.bEnableOcclusionChecks = bEnableOcclusionChecks;
			NewActiveSound.bUseComplexOcclusionChecks = bUseComplexCollisionChecks;
			NewActiveSound.OcclusionLowPassFilterFrequency = FMath::Clamp(OcclusionLowPassFilterFrequency, MIN_FILTER_FREQUENCY, MAX_FILTER_FREQUENCY);
			NewActiveSound.OcclusionVolumeAttenuation = FMath::Clamp(OcclusionVolumeAttenuation, 0.0f, 1.0f);
			NewActiveSound.OcclusionInterpolationTime = FMath::Max(0.0f, OcclusionInterpolationTime);

			NewActiveSound.RequestedStartTime = FMath::Max(0.f, StartTime);
			NewActiveSound.OcclusionCheckInterval = OcclusionCheckInterval;
			NewActiveSound.SubtitlePriority = SubtitlePriority;

			NewActiveSound.bShouldRemainActiveIfDropped = bShouldRemainActiveIfDropped;
			NewActiveSound.bHandleSubtitles = (!bSuppressSubtitles || OnQueueSubtitles.IsBound());
			NewActiveSound.bIgnoreForFlushing = bIgnoreForFlushing;

			NewActiveSound.bIsUISound = bIsUISound;
			NewActiveSound.bIsMusic = bIsMusic;
			NewActiveSound.bAlwaysPlay = bAlwaysPlay;
			NewActiveSound.bReverb = bReverb;
			NewActiveSound.bCenterChannelOnly = bCenterChannelOnly;

			NewActiveSound.bLocationDefined = !bPreviewComponent;
			if (NewActiveSound.bLocationDefined)
			{
				NewActiveSound.Transform = ComponentToWorld;
			}

			const FAttenuationSettings* AttenuationSettingsToApply = (bAllowSpatialization ? GetAttenuationSettingsToApply() : nullptr);
			NewActiveSound.bAllowSpatialization = bAllowSpatialization;
			NewActiveSound.bHasAttenuationSettings = (AttenuationSettingsToApply != nullptr);
			if (NewActiveSound.bHasAttenuationSettings)
			{
				NewActiveSound.AttenuationSettings = *AttenuationSettingsToApply;
				NewActiveSound.MaxDistance = NewActiveSound.AttenuationSettings.GetMaxDimension();
			}
			else
			{
				NewActiveSound.MaxDistance = Sound->GetMaxAudibleDistance();
			}

			NewActiveSound.InstanceParameters = InstanceParameters;

			NewActiveSound.TargetAdjustVolumeMultiplier = FadeVolumeLevel;

			if (FadeInDuration > 0.0f)
			{
				NewActiveSound.CurrentAdjustVolumeMultiplier = 0.f;
				NewActiveSound.TargetAdjustVolumeStopTime = FadeInDuration;
			}
			else
			{
				NewActiveSound.CurrentAdjustVolumeMultiplier = FadeVolumeLevel;
			}

			// TODO - Audio Threading. This call would be a task call to dispatch to the audio thread
			AudioDevice->AddNewActiveSound(NewActiveSound);

			bIsActive = true;
		}
	}
}

FAudioDevice* UAudioComponent::GetAudioDevice() const
{
	FAudioDevice* AudioDevice = nullptr;

	if (GEngine)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			AudioDevice = World->GetAudioDevice();
		}
		else
		{
			AudioDevice = GEngine->GetMainAudioDevice();
		}
	}
	return AudioDevice;
}

void UAudioComponent::FadeIn( float FadeInDuration, float FadeVolumeLevel, float StartTime )
{
	PlayInternal(StartTime, FadeInDuration, FadeVolumeLevel);
}

void UAudioComponent::FadeOut( float FadeOutDuration, float FadeVolumeLevel )
{
	if (bIsActive)
	{
		if (FadeOutDuration > 0.0f)
		{
			// TODO - Audio Threading. This call would be a task
			if (FAudioDevice* AudioDevice = GetAudioDevice())
			{
				FActiveSound* ActiveSound = AudioDevice->FindActiveSound(this);
				if (ActiveSound)
				{
					ActiveSound->TargetAdjustVolumeMultiplier = FadeVolumeLevel;
					ActiveSound->TargetAdjustVolumeStopTime = ActiveSound->PlaybackTime + FadeOutDuration;
					ActiveSound->bFadingOut = true;
				}
			}
		}
		else
		{
			Stop();
		}
	}
}

void UAudioComponent::AdjustVolume( float AdjustVolumeDuration, float AdjustVolumeLevel )
{
	if (bIsActive)
	{
		// TODO - Audio Threading. This call would be a task
		if (FAudioDevice* AudioDevice = GetAudioDevice())
		{
			FActiveSound* ActiveSound = AudioDevice->FindActiveSound(this);
			if (ActiveSound)
			{
				ActiveSound->bFadingOut = false;
				ActiveSound->TargetAdjustVolumeMultiplier = AdjustVolumeLevel;

				if( AdjustVolumeDuration > 0.0f )
				{
					ActiveSound->TargetAdjustVolumeStopTime = ActiveSound->PlaybackTime + AdjustVolumeDuration;
				}
				else
				{
					ActiveSound->CurrentAdjustVolumeMultiplier = AdjustVolumeLevel;
					ActiveSound->TargetAdjustVolumeStopTime = -1.0f;
				}
			}
		}
	}
}

void UAudioComponent::Stop()
{
	if (bIsActive)
	{
		// Set this to immediately be inactive
		bIsActive = false;

		UE_LOG(LogAudio, Verbose, TEXT( "%g: Stopping AudioComponent : '%s' with Sound: '%s'" ), GetWorld() ? GetWorld()->GetAudioTimeSeconds() : 0.0f, *GetFullName(), Sound ? *Sound->GetName() : TEXT( "nullptr" ) );

		// TODO - Audio Threading. This call would be a task
		if (FAudioDevice* AudioDevice = GetAudioDevice())
		{
			AudioDevice->StopActiveSound(this);
		}
	}
}

void UAudioComponent::PlaybackCompleted(bool bFailedToStart)
{
	check(ActiveCount > 0);
	--ActiveCount;

	// Mark inactive before calling destroy to avoid recursion
	bIsActive = (ActiveCount > 0);

	if (!bFailedToStart && GetWorld() != nullptr && (OnAudioFinished.IsBound() || OnAudioFinishedNative.IsBound()))
	{
		INC_DWORD_STAT( STAT_AudioFinishedDelegatesCalled );
		SCOPE_CYCLE_COUNTER( STAT_AudioFinishedDelegates );

		OnAudioFinished.Broadcast();
		OnAudioFinishedNative.Broadcast(this);
	}

	// Auto destruction is handled via marking object for deletion.
	if (bAutoDestroy)
	{
		DestroyComponent();
	}
}

bool UAudioComponent::IsPlaying() const
{
	return bIsActive;
}

#if WITH_EDITORONLY_DATA
void UAudioComponent::UpdateSpriteTexture()
{
	if (SpriteComponent)
	{
		if (bAutoActivate)
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EditorResources/AudioIcons/S_AudioComponent_AutoActivate.S_AudioComponent_AutoActivate")));
		}
		else
		{
			SpriteComponent->SetSprite(LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EditorResources/AudioIcons/S_AudioComponent.S_AudioComponent")));
		}
	}
}
#endif

#if WITH_EDITOR
void UAudioComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (bIsActive)
	{
		// If this is an auto destroy component we need to prevent it from being auto-destroyed since we're really just restarting it
		const bool bWasAutoDestroy = bAutoDestroy;
		bAutoDestroy = false;
		Stop();
		bAutoDestroy = bWasAutoDestroy;
		Play();
	}

#if WITH_EDITORONLY_DATA
	UpdateSpriteTexture();
#endif

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

const FAttenuationSettings* UAudioComponent::GetAttenuationSettingsToApply() const
{
	if (bOverrideAttenuation)
	{
		return &AttenuationOverrides;
	}
	else if (AttenuationSettings)
	{
		return &AttenuationSettings->Attenuation;
	}
	else if (Sound)
	{
		return Sound->GetAttenuationSettingsToApply();
	}
	return nullptr;
}

bool UAudioComponent::BP_GetAttenuationSettingsToApply(FAttenuationSettings& OutAttenuationSettings)
{
	if (const FAttenuationSettings* Settings = GetAttenuationSettingsToApply())
	{
		OutAttenuationSettings = *Settings;
		return true;
	}
	return false;
}

void UAudioComponent::CollectAttenuationShapesForVisualization(TMultiMap<EAttenuationShape::Type, FAttenuationSettings::AttenuationShapeDetails>& ShapeDetailsMap) const
{
	const FAttenuationSettings *AttenuationSettingsToApply = GetAttenuationSettingsToApply();

	if (AttenuationSettingsToApply)
	{
		AttenuationSettingsToApply->CollectAttenuationShapesForVisualization(ShapeDetailsMap);
	}

	// For sound cues we'll dig in and see if we can find any attenuation sound nodes that will affect the settings
	USoundCue* SoundCue = Cast<USoundCue>(Sound);
	if (SoundCue)
	{
		TArray<USoundNodeAttenuation*> AttenuationNodes;
		SoundCue->RecursiveFindAttenuation( SoundCue->FirstNode, AttenuationNodes );
		for (int32 NodeIndex = 0; NodeIndex < AttenuationNodes.Num(); ++NodeIndex)
		{
			AttenuationSettingsToApply = AttenuationNodes[NodeIndex]->GetAttenuationSettingsToApply();
			if (AttenuationSettingsToApply)
			{
				AttenuationSettingsToApply->CollectAttenuationShapesForVisualization(ShapeDetailsMap);
			}
		}
	}
}

void UAudioComponent::Activate(bool bReset)
{
	if (bReset || ShouldActivate() == true)
	{
		Play();
	}
}

void UAudioComponent::Deactivate()
{
	if (ShouldActivate() == false)
	{
		Stop();
	}
}

void UAudioComponent::SetFloatParameter( FName InName, float InFloat )
{
	if (InName != NAME_None)
	{
		bool bFound = false;

		// First see if an entry for this name already exists
		for (int32 i = 0; i < InstanceParameters.Num(); i++)
		{
			FAudioComponentParam& P = InstanceParameters[i];
			if (P.ParamName == InName)
			{
				P.FloatParam = InFloat;
				bFound = true;
				break;
			}
		}

		// We didn't find one, so create a new one.
		if (!bFound)
		{
			const int32 NewParamIndex = InstanceParameters.AddZeroed();
			InstanceParameters[ NewParamIndex ].ParamName = InName;
			InstanceParameters[ NewParamIndex ].FloatParam = InFloat;
		}

		// If we're active we need to push this value to the ActiveSound
		if (bIsActive)
		{
			// TODO - Audio Threading. This call would be a task
			if (FAudioDevice* AudioDevice = GetAudioDevice())
			{
				FActiveSound* ActiveSound = AudioDevice->FindActiveSound(this);
				if (ActiveSound)
				{
					ActiveSound->SetFloatParameter(InName, InFloat);
				}
			}
		}
	}
}

void UAudioComponent::SetWaveParameter( FName InName, USoundWave* InWave )
{
	if (InName != NAME_None)
	{
		bool bFound = false;
		// First see if an entry for this name already exists
		for (int32 i = 0; i < InstanceParameters.Num(); i++)
		{
			FAudioComponentParam& P = InstanceParameters[i];
			if (P.ParamName == InName)
			{
				P.SoundWaveParam = InWave;
				bFound = true;
				break;
			}
		}

		// We didn't find one, so create a new one.
		if (!bFound)
		{
			const int32 NewParamIndex = InstanceParameters.AddZeroed();
			InstanceParameters[NewParamIndex].ParamName = InName;
			InstanceParameters[NewParamIndex].SoundWaveParam = InWave;
		}

		// If we're active we need to push this value to the ActiveSound
		if (bIsActive)
		{
			// TODO - Audio Threading. This call would be a task
			if (FAudioDevice* AudioDevice = GetAudioDevice())
			{
				FActiveSound* ActiveSound = AudioDevice->FindActiveSound(this);
				if (ActiveSound)
				{
					ActiveSound->SetWaveParameter(InName, InWave);
				}
			}
		}
	}
}

void UAudioComponent::SetBoolParameter( FName InName, bool InBool )
{
	if (InName != NAME_None)
	{
		bool bFound = false;
		// First see if an entry for this name already exists
		for (int32 i = 0; i < InstanceParameters.Num(); i++)
		{
			FAudioComponentParam& P = InstanceParameters[i];
			if (P.ParamName == InName)
			{
				P.BoolParam = InBool;
				bFound = true;
				break;
			}
		}

		// We didn't find one, so create a new one.
		if (!bFound)
		{
			const int32 NewParamIndex = InstanceParameters.AddZeroed();
			InstanceParameters[ NewParamIndex ].ParamName = InName;
			InstanceParameters[ NewParamIndex ].BoolParam = InBool;
		}

		// If we're active we need to push this value to the ActiveSound
		if (bIsActive)
		{
			// TODO - Audio Threading. This call would be a task
			if (FAudioDevice* AudioDevice = GetAudioDevice())
			{
				FActiveSound* ActiveSound = AudioDevice->FindActiveSound(this);
				if (ActiveSound)
				{
					ActiveSound->SetBoolParameter(InName, InBool);
				}
			}
		}
	}
}


void UAudioComponent::SetIntParameter( FName InName, int32 InInt )
{
	if (InName != NAME_None)
	{
		bool bFound = false;
		// First see if an entry for this name already exists
		for (int32 i = 0; i < InstanceParameters.Num(); i++)
		{
			FAudioComponentParam& P = InstanceParameters[i];
			if (P.ParamName == InName)
			{
				P.IntParam = InInt;
				bFound = true;
				break;
			}
		}

		// We didn't find one, so create a new one.
		if (!bFound)
		{
			const int32 NewParamIndex = InstanceParameters.AddZeroed();
			InstanceParameters[NewParamIndex].ParamName = InName;
			InstanceParameters[NewParamIndex].IntParam = InInt;
		}

		// If we're active we need to push this value to the ActiveSound
		if (bIsActive)
		{
			// TODO - Audio Threading. This call would be a task
			if (FAudioDevice* AudioDevice = GetAudioDevice())
			{
				FActiveSound* ActiveSound = AudioDevice->FindActiveSound(this);
				if (ActiveSound)
				{
					ActiveSound->SetIntParameter(InName, InInt);
				}
			}
		}
	}
}

void UAudioComponent::SetVolumeMultiplier(float NewVolumeMultiplier)
{
	VolumeMultiplier = NewVolumeMultiplier;
	VolumeModulationMin = VolumeModulationMax = 1.f;

	// TODO - Audio Threading. This call would be a task
	if (FAudioDevice* AudioDevice = GetAudioDevice())
	{
		FActiveSound* ActiveSound = AudioDevice->FindActiveSound(this);
		if (ActiveSound)
		{
			ActiveSound->VolumeMultiplier = NewVolumeMultiplier;
		}
	}
}

void UAudioComponent::SetPitchMultiplier(float NewPitchMultiplier)
{
	PitchMultiplier = NewPitchMultiplier;
	PitchModulationMin = PitchModulationMax = 1.f;

	// TODO - Audio Threading. This call would be a task
	if (FAudioDevice* AudioDevice = GetAudioDevice())
	{
		FActiveSound* ActiveSound = AudioDevice->FindActiveSound(this);
		if (ActiveSound)
		{
			ActiveSound->PitchMultiplier = NewPitchMultiplier;
		}
	}
}

void UAudioComponent::SetUISound(bool bInIsUISound)
{
	bIsUISound = bInIsUISound;

	// TODO - Audio Threading. This call would be a task
	if (FAudioDevice* AudioDevice = GetAudioDevice())
	{
		FActiveSound* ActiveSound = AudioDevice->FindActiveSound(this);
		if (ActiveSound)
		{
			ActiveSound->bIsUISound = bIsUISound;
		}
	}
}

void UAudioComponent::AdjustAttenuation(const FAttenuationSettings& InAttenuationSettings)
{
	bOverrideAttenuation = true;
	AttenuationOverrides = InAttenuationSettings;

	// TODO - Audio Threading. This call would be a task
	if (FAudioDevice* AudioDevice = GetAudioDevice())
	{
		FActiveSound* ActiveSound = AudioDevice->FindActiveSound(this);
		if (ActiveSound)
		{
			ActiveSound->AttenuationSettings = AttenuationOverrides;
		}
	}
}
