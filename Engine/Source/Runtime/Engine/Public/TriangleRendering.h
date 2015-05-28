// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TriangleRendering.h: Simple tile rendering implementation.
=============================================================================*/

#ifndef _INC_TRIANGLERENDERING
#define _INC_TRIANGLERENDERING

class FTriangleRenderer
{
public:

	/**
	 * Draw a tile at the given location and size, using the given UVs
	 * (UV = [0..1]
	 */
	ENGINE_API static void DrawTriangle(FRHICommandListImmediate& RHICmdList, const class FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, bool bNeedsToSwitchVerticalAxis, const FCanvasUVTri& Tri, bool bIsHitTesting = false, const FHitProxyId HitProxyId = FHitProxyId(), const FColor InVertexColor = FColor(255, 255, 255, 255));

private:

	/** This class never needs to be instantiated. */
	FTriangleRenderer() {}
};

#endif
