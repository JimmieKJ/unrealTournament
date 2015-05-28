// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "FoliageType_InstancedStaticMesh.h"

UClass* FAssetTypeActions_InstancedFoliageSettings::GetSupportedClass() const
{
	return UFoliageType_InstancedStaticMesh::StaticClass();
}