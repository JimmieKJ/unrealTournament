// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_ReverbEffect.h"
#include "Sound/ReverbEffect.h"

UClass* FAssetTypeActions_ReverbEffect::GetSupportedClass() const
{
	return UReverbEffect::StaticClass();
}
