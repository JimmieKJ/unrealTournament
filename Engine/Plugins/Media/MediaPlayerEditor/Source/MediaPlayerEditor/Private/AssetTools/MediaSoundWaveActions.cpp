// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPrivatePCH.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"


/* FAssetTypeActions_SoundBase overrides
 *****************************************************************************/

bool FMediaSoundWaveActions::CanFilter()
{
	return true;
}


uint32 FMediaSoundWaveActions::GetCategories()
{
	return EAssetTypeCategories::Sounds;
}


FText FMediaSoundWaveActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MediaSoundWave", "Media Sound Wave");
}


UClass* FMediaSoundWaveActions::GetSupportedClass() const
{
	return UMediaSoundWave::StaticClass();
}


FColor FMediaSoundWaveActions::GetTypeColor() const
{
	return FColor(77, 93, 239);
}


bool FMediaSoundWaveActions::HasActions ( const TArray<UObject*>& InObjects ) const
{
	return false;
}


#undef LOCTEXT_NAMESPACE
