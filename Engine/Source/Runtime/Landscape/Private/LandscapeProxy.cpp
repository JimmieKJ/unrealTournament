// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LandscapePrivatePCH.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"

#if WITH_EDITOR

LANDSCAPE_API FLandscapeImportLayerInfo::FLandscapeImportLayerInfo(const FLandscapeInfoLayerSettings& InLayerSettings)
	: LayerName(InLayerSettings.GetLayerName())
	, LayerInfo(InLayerSettings.LayerInfoObj)
	, SourceFilePath(InLayerSettings.GetEditorSettings().ReimportLayerFilePath)
{
}

#endif // WITH_EDITOR