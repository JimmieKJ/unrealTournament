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
 *	@param		InMaxHullCount		Max number of hulls to create
 *	@param		InMaxHullVerts		Number of verts allowed in a hull
 */
UNREALED_API void DecomposeMeshToHulls(UBodySetup* InBodySetup, const TArray<FVector>& InVertices, const TArray<uint32>& InIndices, int32 InMaxHullCount, int32 InMaxHullVerts);

#endif // __CONVEXDECOMPTOOL_H__
