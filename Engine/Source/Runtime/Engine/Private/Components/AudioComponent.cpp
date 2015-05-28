// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "SoundDefinitions.h"
#include "Sound/SoundNodeAttenuation.h"
#include "Sound/SoundCue.h"
#include "SubtitleManager.h"

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
#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif
	VolumeMultiplier = 1.f;
	PitchMultiplier = 1.f;
	VolumeModulationMin = 1.f;
	VolumeModulationMax = 1.f;
	PitchModulationMin = 1.f;
	PitchModulationMax = 1.f;
	HighFrequencyGainMultiplier = 1.0f;
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

void UAudioComponent::OnUpdateTransform(bool bSkipPhysicsMove)
{
	Super::OnUpdateTransform(bSkipPhysicsMove);

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
		Stop();
		bAutoDestroy = bCurrentAutoDestroy;
	}

	if (Sound && (World == nullptr || World->bAllowAudioPlayback))
	{
		if (FAudioDevice* AudioDevice = GetAudioDevice())
		{
			FActiveSound NewActiveSound;
			NewActiveSound.AudioComponent = this;
			NewActiveSound.World = GetWorld();
			NewActiveSound.Sound = Sound;
			NewActiveSound.SoundClassOverride = SoundClassOverride;

			NewActiveSound.VolumeMultiplier = (VolumeModulationMax + ((VolumeModulationMin - VolumeModulationMax) * FMath::SRand())) * VolumeMultiplier;
			NewActiveSound.PitchMultiplier = (PitchModulationMax + ((PitchModulationMin - PitchModulationMax) * FMath::SRand())) * PitchMultiplier;
			NewActiveSound.HighFrequencyGainMultiplier = HighFrequencyGainMultiplier;

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
	// Mark inactive before calling destroy to avoid recursion
	bIsActive = false;

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
