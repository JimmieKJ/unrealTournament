// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"

#if WITH_EDITOR

LANDSCAPE_API FLandscapeImportLayerInfo::FLandscapeImportLayerInfo(const struct FLandscapeInfoLayerSettings& InLayerSettings)
:	LayerName(InLayerSettings.GetLayerName())
,	LayerInfo(InLayerSettings.LayerInfoObj)
,	ThumbnailMIC(NULL)
,	SourceFilePath(InLayerSettings.GetEditorSettings().ReimportLayerFilePath)
{ }

#endif // WITH_EDITOR