// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SoundAttenuation.h"
#include "Sound/SoundAttenuation.h"

UClass* FAssetTypeActions_SoundAttenuation::GetSupportedClass() const
{
	return USoundAttenuation::StaticClass();
}
