// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTAudioSettings.h"
#include "AudioThread.h"

UUTAudioSettings::UUTAudioSettings(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UUTAudioSettings::SetSoundClassVolume(EUTSoundClass::Type SoundClassType, float NewVolume)
{
	const UEnum* EUTSoundClassPtr = FindObject<UEnum>(ANY_PACKAGE, TEXT("EUTSoundClass"), true);
	checkSlow(EUTSoundClassPtr != NULL);
	if (EUTSoundClassPtr == NULL)
	{
		// add logging?
		return;
	}
	FString EnumValueDisplayName = EUTSoundClassPtr->GetEnumName(SoundClassType);
	SetSoundClassVolumeByName(EnumValueDisplayName, NewVolume);
}

/** iterate over all registered sound classes and update all matching sound classes' volumes */
void UUTAudioSettings::SetSoundClassVolumeByName(const FString& SoundClassName, float NewVolume)
{
	// no effect in editor because we're altering a savable object
	// also just unintuitive when editing sounds
	if (!GIsEditor)
	{
		FAudioDevice* AudioDevice = GEngine->GetMainAudioDevice();
		if (AudioDevice != NULL)
		{
			// This is a hack until UT-1201 is resolved to switch to using the correct SoundMixClassOverride system
			FAudioThreadSuspendContext AudioThreadSuspend;

			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			for (TMap<USoundClass*, FSoundClassProperties>::TConstIterator It(AudioDevice->GetSoundClassPropertyMap()); It; ++It)
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			{
				USoundClass* ThisSoundClass = It.Key();
				if (ThisSoundClass != NULL && (ThisSoundClass->GetName() == SoundClassName))
				{
					// the audiodevice function logspams for some reason
					ThisSoundClass->Properties.Volume = NewVolume;
				}
			}
		}
	}
}
