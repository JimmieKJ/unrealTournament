// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "SoundDefinitions.h"

UClass* FAssetTypeActions_SoundConcurrency::GetSupportedClass() const
{
	return USoundConcurrency::StaticClass();
}