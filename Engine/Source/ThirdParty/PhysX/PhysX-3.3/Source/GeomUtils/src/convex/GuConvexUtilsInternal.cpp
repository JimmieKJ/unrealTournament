/*
 * Copyright (c) 2008-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.  

#include "GuConvexUtilsInternal.h"
#include "PxBounds3.h"
#include "CmScaling.h"
#include "GuBoxConversion.h"
#include "PxConvexMeshGeometry.h"
#include "GuConvexMesh.h"

using namespace physx;
using namespace Gu;

void Gu::computeHullOBB(
	Box& hullOBB, const PxBounds3& hullAABB, float offset, const PxTransform& transformCvx, const Cm::Matrix34& worldCvx, const Cm::Matrix34& worldMesh,
	const Cm::FastVertex2ShapeScaling& meshScaling, bool idtScaleMesh)
{
	//query OBB in world space
	PxVec3 center = transformCvx.transform((hullAABB.minimum + hullAABB.maximum) * 0.5f);
	PxVec3 extents = (hullAABB.maximum - hullAABB.minimum) * 0.5f;
	extents += PxVec3(offset);	//fatten query region for dist based.
	//orientation is worldCvx.M

	//transform to mesh shape space:
	center = worldMesh.transformTranspose(center);
	PxMat33 obbBasis = PxMat33(worldMesh.base0, worldMesh.base1, worldMesh.base2).getTranspose() * PxMat33(worldCvx.base0, worldCvx.base1, worldCvx.base2);
	//OBB orientation is worldCvx

	if(!idtScaleMesh)
		meshScaling.transformQueryBounds(center, extents, obbBasis);

	// Setup an OBB of the convex hull in vertex space
	hullOBB.center	= center;
	hullOBB.extents	= extents;
	hullOBB.rot		= obbBasis;
}

void Gu::computeVertexSpaceOBB(Box& dst, const Box& src, const PxTransform& meshPose, const PxMeshScale& meshScale)
{
	// AP scaffold failure in x64 debug in GuConvexUtilsInternal.cpp
	//PX_ASSERT("Performance warning - this path shouldn't execute for identity mesh scale." && !meshScale.isIdentity());

	// to go from vertex to world we have to apply scale first, then rotation, then translation
	const PxMat33 vertexToWorldSkewRot = PxMat33(meshPose.q) * meshScale.toMat33();
	const PxVec3& vertexToWorldTrans = meshPose.p;

	// invert the combined transform (vertex to skew to world)
	PxMat33 worldToVertexSkewRot;
	PxVec3 worldToVertexTrans;
	getInverse(worldToVertexSkewRot, worldToVertexTrans, vertexToWorldSkewRot, vertexToWorldTrans);

	// make vertex space OBB
	const Cm::Matrix34 _worldToVertexSkew(worldToVertexSkewRot, worldToVertexTrans);
	dst = transform(_worldToVertexSkew, src);
}

void Gu::computeOBBAroundConvex(
	Box& obb, const PxConvexMeshGeometry& convexGeom, const PxConvexMesh* cm, const PxTransform& convexPose)
{
	PxBounds3 localAABB = ((Gu::ConvexMesh*)cm)->getLocalBoundsFast();

	Box localOBB;
	localOBB.center		= localAABB.getCenter();
	localOBB.extents	= localAABB.getExtents();
	localOBB.rot		= PxMat33(PxIdentity);

	Cm::Matrix34 M;
	if(convexGeom.scale.isIdentity())
	{
		M = Cm::Matrix34(convexPose);

		rotate(localOBB, M, obb);
	}
	else
	{
		const PxMat33 rot = PxMat33(convexPose.q) * convexGeom.scale.toMat33();
		M = Cm::Matrix34(rot, convexPose.p);

		obb = transform(M, localOBB);
	}
}
