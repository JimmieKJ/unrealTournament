// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "ProceduralFoliageSpawner.h"

UClass* FAssetTypeActions_ProceduralFoliageSpawner::GetSupportedClass() const
{
	return UProceduralFoliageSpawner::StaticClass();
}

bool FAssetTypeActions_ProceduralFoliageSpawner::CanFilter()
{
	return GetDefault<UEditorExperimentalSettings>()->bProceduralFoliage;
}