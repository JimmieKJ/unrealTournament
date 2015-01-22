// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "GameFramework/ForceFeedbackEffect.h"

UClass* FAssetTypeActions_ForceFeedbackEffect::GetSupportedClass() const
{
	return UForceFeedbackEffect::StaticClass();
}