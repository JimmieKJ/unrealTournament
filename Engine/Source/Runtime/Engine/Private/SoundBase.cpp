// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Sound/SoundBase.h"
#include "Sound/AudioSettings.h"
#include "AudioDevice.h"

USoundBase::USoundBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaxConcurrentPlayCount = 16;
}

void USoundBase::PostInitProperties()
{
	Super::PostInitProperties();

	const FStringAssetReference DefaultSoundClassName = GetDefault<UAudioSettings>()->DefaultSoundClassName;
	if (DefaultSoundClassName.IsValid())
	{
		SoundClassObject = LoadObject<USoundClass>(NULL, *DefaultSoundClassName.ToString());
	}
}

bool USoundBase::IsPlayable() const
{
	return false;
}

const FAttenuationSettings* USoundBase::GetAttenuationSettingsToApply() const
{
	if (AttenuationSettings)
	{
		return &AttenuationSettings->Attenuation;
	}
	return NULL;
}

float USoundBase::GetMaxAudibleDistance()
{
	return 0.f;
}

bool USoundBase::IsAudibleSimple( const FVector Location, USoundAttenuation* AttenuationSettings )
{
	// No audio device means no listeners to check against
	if( !GEngine || !GEngine->GetAudioDevice() )
	{
		return true ;
	}

	// Listener position could change before long sounds finish
	if( GetDuration() > 1.0f )
	{
		return true ;
	}

	// Is this SourceActor within the MaxAudibleDistance of any of the listeners?
	float MaxAudibleDistance = AttenuationSettings != nullptr ? AttenuationSettings->Attenuation.GetMaxDimension() : GetMaxAudibleDistance();
	return GEngine->GetAudioDevice()->LocationIsAudible( Location, MaxAudibleDistance );
}

bool USoundBase::IsAudible( const FVector &SourceLocation, const FVector &ListenerLocation, AActor* SourceActor, bool& bIsOccluded, bool bCheckOcclusion )
{
	//@fixme - naive implementation, needs to be optimized
	// for now, check max audible distance, and also if looping
	check( SourceActor );
	// Account for any portals
	const FVector ModifiedSourceLocation = SourceLocation;

	const float MaxDist = GetMaxAudibleDistance();
	if( MaxDist * MaxDist >= ( ListenerLocation - ModifiedSourceLocation ).SizeSquared() )
	{
		// Can't line check through portals
		if( bCheckOcclusion && ( MaxDist != WORLD_MAX ) && ( ModifiedSourceLocation == SourceLocation ) )
		{
			static FName NAME_IsAudible(TEXT("IsAudible"));

			// simple trace occlusion check - reduce max audible distance if occluded
			bIsOccluded = SourceActor->GetWorld()->LineTraceTest(ModifiedSourceLocation, ListenerLocation, ECC_Visibility, FCollisionQueryParams(NAME_IsAudible, true, SourceActor));
		}
		return true;
	}
	else
	{
		return false;
	}
}

float USoundBase::GetDuration()
{
	return Duration;
}

float USoundBase::GetVolumeMultiplier()
{
	return 1.f;
}

float USoundBase::GetPitchMultiplier()
{
	return 1.f;
}