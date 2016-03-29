// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshRendering.h: Simple mesh rendering implementation.
=============================================================================*/

#pragma once

struct FMaterialMergeData;
class FMaterialRenderProxy;
class UTextureRenderTarget2D;
struct FColor;

/** Class used as an interface for baking materials to textures using mesh/vertex-data */
class FMeshRenderer
{
public:
	/** Renders out textures for each material property for the given material, using the given mesh data or by using a simple tile rendering approach */
	MATERIALUTILITIES_API static bool RenderMaterial(FMaterialMergeData& InMaterialData, FMaterialRenderProxy* InMaterialProxy,
		EMaterialProperty InMaterialProperty, UTextureRenderTarget2D* InRenderTarget, TArray<FColor>& OutBMP);	
private:
	/** This class never needs to be instantiated. */
	FMeshRenderer() {}
};
