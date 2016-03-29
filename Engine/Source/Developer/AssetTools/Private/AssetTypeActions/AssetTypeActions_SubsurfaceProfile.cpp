// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "Engine/SubsurfaceProfile.h"

UClass* FAssetTypeActions_SubsurfaceProfile::GetSupportedClass() const
{
	return USubsurfaceProfile::StaticClass();
}
