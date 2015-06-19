// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LightmassRender.h"

class FMaterialRenderProxy;
class FLandscapeStaticLightingMesh;

extern void RenderLandscapeMaterialForLightmass(const FLandscapeStaticLightingMesh* LandscapeMesh, FMaterialRenderProxy* MaterialProxy, const FRenderTarget* RenderTarget);
extern void GetLandscapeOpacityData(const FLandscapeStaticLightingMesh* LandscapeMesh, int32& InOutSizeX, int32& InOutSizeY, TArray<FFloat16Color>& OutMaterialSamples);
