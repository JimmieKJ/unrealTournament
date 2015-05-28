// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ActiveSound.h"
#include "Sound/DialogueWave.h"
#include "Sound/DialogueSoundWaveProxy.h"
#include "Sound/SoundWave.h"
#include "SubtitleManager.h"

bool operator==(const FDialogueContextMapping& LHS, const FDialogueContextMapping& RHS)
{
	return	LHS.Context == RHS.Context &&
		LHS.SoundWave == RHS.SoundWave;
}

bool operator!=(const FDialogueContextMapping& LHS, const FDialogueContextMapping& RHS)
{
	return	LHS.Context != RHS.Context ||
		LHS.SoundWave != RHS.SoundWave;
}

FDialogueContextMapping::FDialogueContextMapping() : SoundWave(NULL), Proxy(NULL)
{

}

UDialogueSoundWaveProxy::UDialogueSoundWaveProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UDialogueSoundWaveProxy::IsPlayable() const
{
	return SoundWave->IsPlayable();
}

const FAttenuationSettings* UDialogueSoundWaveProxy::GetAttenuationSettingsToApply() const
{
	return SoundWave->GetAttenuationSettingsToApply();
}

float UDialogueSoundWaveProxy::GetMaxAudibleDistance()
{
	return SoundWave->GetMaxAudibleDistance();
}

float UDialogueSoundWaveProxy::GetDuration()
{
	return SoundWave->GetDuration();
}

float UDialogueSoundWaveProxy::GetVolumeMultiplier()
{
	return SoundWave->GetVolumeMultiplier();
}

float UDialogueSoundWaveProxy::GetPitchMultiplier()
{
	return SoundWave->GetPitchMultiplier();
}

void UDialogueSoundWaveProxy::Parse( class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	int OldWaveInstanceCount = WaveInstances.Num();
	SoundWave->Parse(AudioDevice, NodeWaveInstanceHash, ActiveSound, ParseParams, WaveInstances);
	int NewWaveInstanceCount = WaveInstances.Num();

	FWaveInstance* WaveInstance = NULL;
	if(NewWaveInstanceCount == OldWaveInstanceCount + 1)
	{
		WaveInstance = WaveInstances[WaveInstances.Num() - 1];
	}

	// Add in the subtitle if they exist
	if (ActiveSound.bHandleSubtitles && Subtitles.Num() > 0)
	{
		// TODO - Audio Threading. This would need to be a call back to the main thread.
		if (ActiveSound.AudioComponent.IsValid() && ActiveSound.AudioComponent->OnQueueSubtitles.IsBound())
		{
			// intercept the subtitles if the delegate is set
			ActiveSound.AudioComponent->OnQueueSubtitles.ExecuteIfBound( Subtitles, GetDuration() );
		}
		else
		{
			// otherwise, pass them on to the subtitle manager for display
			// Subtitles are hashed based on the associated sound (wave instance).
			if( ActiveSound.World.IsValid() )
			{
				FSubtitleManager::GetSubtitleManager()->QueueSubtitles( ( PTRINT )WaveInstance, ActiveSound.SubtitlePriority, false, false, GetDuration(), Subtitles, 0.0f );
			}
		}
	}
}

USoundClass* UDialogueSoundWaveProxy::GetSoundClass() const
{
	return SoundWave->GetSoundClass();
}

UDialogueWave::UDialogueWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LocalizationGUID( FGuid::NewGuid() )
{
	ContextMappings.Add( FDialogueContextMapping() );
}

// Begin UObject interface. 
void UDialogueWave::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar.ThisRequiresLocalizationGather();

	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
	{
		UE_LOG(LogAudio, Fatal, TEXT("This platform requires cooked packages, and audio data was not cooked into %s."), *GetFullName());
	}
}

bool UDialogueWave::IsReadyForFinishDestroy()
{
	return true;
}

FString UDialogueWave::GetDesc()
{
	return FString::Printf( TEXT( "Dialogue Wave Description" ) );
}

void UDialogueWave::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
}

void UDialogueWave::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	if( !bDuplicateForPIE )
	{
		LocalizationGUID = FGuid::NewGuid();
	}
}

void UDialogueWave::PostLoad()
{
	Super::PostLoad();

	for(FDialogueContextMapping& ContextMapping : ContextMappings)
	{
		UpdateMappingProxy(ContextMapping);
	}
}

#if WITH_EDITOR
void UDialogueWave::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	int32 NewArrayIndex = PropertyChangedEvent.GetArrayIndex(TEXT("ContextMappings"));

	if (ContextMappings.IsValidIndex(NewArrayIndex))
	{
		UpdateMappingProxy(ContextMappings[NewArrayIndex]);
	}
	else if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UDialogueWave, SpokenText))
	{
		for(FDialogueContextMapping& ContextMapping : ContextMappings)
		{
			UpdateMappingProxy(ContextMapping);
		}
	}
}
#endif

// End UObject interface. 

// Begin UDialogueWave interface.
bool UDialogueWave::SupportsContext(const FDialogueContext& Context) const
{
	for(int32 i = 0; i < ContextMappings.Num(); ++i)
	{
		const FDialogueContextMapping& ContextMapping = ContextMappings[i];
		if(ContextMapping.Context == Context)
		{
			return true;
		}
	}

	return false;
}

USoundBase* UDialogueWave::GetWaveFromContext(const FDialogueContext& Context) const
{
	if(Context.Speaker == NULL)
	{
		UE_LOG(LogAudio, Warning, TEXT("UDialogueWave::GetWaveFromContext requires a Context.Speaker (%s)."), *GetPathName());
		return NULL;
	}

	UDialogueSoundWaveProxy* Proxy = NULL;
	for(int32 i = 0; i < ContextMappings.Num(); ++i)
	{
		const FDialogueContextMapping& ContextMapping = ContextMappings[i];
		if(ContextMapping.Context == Context)
		{
			Proxy = ContextMapping.Proxy;
			break;
		}
	}
	
	return Proxy;
}

FString UDialogueWave::GetContextLocalizationKey( const FDialogueContext& Context ) const
{
	FString Key;
	if( SupportsContext(Context) )
	{
		Key = LocalizationGUID.ToString() + TEXT(" ") + Context.GetLocalizationKey();
	}
	return Key;
}


// End UDialogueWave interface.

void UDialogueWave::UpdateMappingProxy(FDialogueContextMapping& ContextMapping)
{
	if (ContextMapping.SoundWave)
	{
		if (!ContextMapping.Proxy)
		{
			ContextMapping.Proxy = NewObject<UDialogueSoundWaveProxy>();
		}
	}
	else
	{
		ContextMapping.Proxy = NULL;
	}

	if(ContextMapping.Proxy)
	{
		// Copy the properties that the proxy shares with the sound in case it's used as a SoundBase
		ContextMapping.Proxy->SoundWave = ContextMapping.SoundWave;
		UEngine::CopyPropertiesForUnrelatedObjects(ContextMapping.SoundWave, ContextMapping.Proxy);

		FSubtitleCue NewSubtitleCue;
		FString Key = GetContextLocalizationKey( ContextMapping.Context );

		if( !( FText::FindText( FString(), Key, NewSubtitleCue.Text )) )
		{
			Key = LocalizationGUID.ToString();

			if ( !FText::FindText( FString(), Key, /*OUT*/NewSubtitleCue.Text ) )
			{
				NewSubtitleCue.Text = FText::FromString( SpokenText );
			}
		}
		NewSubtitleCue.Time = 0.0f;
		ContextMapping.Proxy->Subtitles.Empty();
		ContextMapping.Proxy->Subtitles.Add(NewSubtitleCue);
	}
}
