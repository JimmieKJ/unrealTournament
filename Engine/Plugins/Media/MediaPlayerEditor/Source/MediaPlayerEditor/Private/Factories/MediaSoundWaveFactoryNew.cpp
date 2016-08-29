// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPCH.h"
#include "MediaSoundWaveFactoryNew.h"


/* UMediaSoundWaveFactoryNew structors
 *****************************************************************************/

UMediaSoundWaveFactoryNew::UMediaSoundWaveFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UMediaSoundWave::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory interface
 *****************************************************************************/

UObject* UMediaSoundWaveFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UMediaSoundWave>(InParent, InClass, InName, Flags);
}


uint32 UMediaSoundWaveFactoryNew::GetMenuCategories() const
{
	return EAssetTypeCategories::Media | EAssetTypeCategories::Sounds;
}


bool UMediaSoundWaveFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
