// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ConvexDecompTool.h: Utility for turning graphics mesh into convex hulls.
=============================================================================*/

#ifndef __CONVEXDECOMPTOOL_H__
#define __CONVEXDECOMPTOOL_H__

/** 
 *	Utlity for turning arbitary mesh into convex hulls.
 *	@output		InBodySetup			BodySetup that will have its existing hulls removed and replaced with results of decomposition.
 *	@param		InVertices			Array of vertex positions of input mesh
 *	@param		InIndices			Array of triangle indices for input mesh
 *	@param		InAccuracy			Value between 0 and 1, controls how accurate hull generation is
 *	@param		InMaxHullVerts		Number of verts allowed in a hull
 */
UNREALED_API void DecomposeMeshToHulls(UBodySetup* InBodySetup, const TArray<FVector>& InVertices, const TArray<uint32>& InIndices, float InAccuracy, int32 InMaxHullVerts);

#endif // __CONVEXDECOMPTOOL_H__
