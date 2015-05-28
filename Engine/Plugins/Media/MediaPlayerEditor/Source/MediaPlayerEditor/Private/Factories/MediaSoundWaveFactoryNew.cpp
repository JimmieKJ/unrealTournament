// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPrivatePCH.h"


/* UMediaSoundWaveFactoryNew structors
 *****************************************************************************/

UMediaSoundWaveFactoryNew::UMediaSoundWaveFactoryNew( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	SupportedClass = UMediaSoundWave::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory overrides
 *****************************************************************************/

UObject* UMediaSoundWaveFactoryNew::FactoryCreateNew( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn )
{
	UMediaSoundWave* MediaSoundWave = NewObject<UMediaSoundWave>(InParent, InClass, InName, Flags);

	if ((MediaSoundWave != nullptr) && (InitialMediaPlayer != nullptr))
	{
		MediaSoundWave->SetMediaPlayer(InitialMediaPlayer);
	}

	return MediaSoundWave;
}


bool UMediaSoundWaveFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
