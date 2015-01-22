// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TessellationRendering.h: Tessellation rendering definitions.
=============================================================================*/

#ifndef __TESSELLATIONRENDERING_H__
#define __TESSELLATIONRENDERING_H__

/** Returns true if the Material and Vertex Factory combination require adjacency information. */
extern ENGINE_API bool RequiresAdjacencyInformation(UMaterialInterface* Material, const FVertexFactoryType* VertexFactoryType, ERHIFeatureLevel::Type InFeatureLevel);

#endif
