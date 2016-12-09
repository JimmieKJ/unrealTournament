// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SubDSurface.h"
#include "Engine/SubDSurface.h"

UClass* FAssetTypeActions_SubDSurface::GetSupportedClass() const
{
	return USubDSurface::StaticClass();
}
