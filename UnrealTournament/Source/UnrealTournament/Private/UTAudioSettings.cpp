// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTAudioSettings.h"

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
		FAudioDevice* AudioDevice = GEngine->GetAudioDevice();
		if (AudioDevice != NULL)
		{
			for (TMap<USoundClass*, FSoundClassProperties>::TIterator It(AudioDevice->SoundClasses); It; ++It)
			{
				USoundClass* ThisSoundClass = It.Key();
				if (ThisSoundClass != NULL && ThisSoundClass->GetFullName().Find(SoundClassName) != INDEX_NONE)
				{
					// the audiodevice function logspams for some reason
					//AudioDevice->SetClassVolume(ThisSoundClass, NewVolume);
					ThisSoundClass->Properties.Volume = NewVolume;
				}
			}
		}
	}
}
