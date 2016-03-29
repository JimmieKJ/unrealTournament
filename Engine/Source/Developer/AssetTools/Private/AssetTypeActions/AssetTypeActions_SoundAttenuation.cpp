// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "SoundDefinitions.h"

UClass* FAssetTypeActions_SoundAttenuation::GetSupportedClass() const
{
	return USoundAttenuation::StaticClass();
}