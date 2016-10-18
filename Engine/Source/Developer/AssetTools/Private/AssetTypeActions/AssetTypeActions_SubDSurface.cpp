// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "Engine/SubDSurface.h"

UClass* FAssetTypeActions_SubDSurface::GetSupportedClass() const
{
	return USubDSurface::StaticClass();
}
