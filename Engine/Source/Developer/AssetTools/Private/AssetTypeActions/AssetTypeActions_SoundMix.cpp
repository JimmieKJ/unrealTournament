// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "SoundDefinitions.h"
#include "Sound/SoundMix.h"

UClass* FAssetTypeActions_SoundMix::GetSupportedClass() const
{
	return USoundMix::StaticClass();
}