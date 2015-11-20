// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Sound/SoundNodeAssetReferencer.h"

#if WITH_EDITOR
void USoundNodeAssetReferencer::PostEditImport()
{
	Super::PostEditImport();

	LoadAsset();
}
#endif
