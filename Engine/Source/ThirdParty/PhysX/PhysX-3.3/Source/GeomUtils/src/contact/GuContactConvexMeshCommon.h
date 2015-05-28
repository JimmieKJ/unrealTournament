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

#ifndef GU_CONTACT_CONVEX_MESH_COMMON_H
#define GU_CONTACT_CONVEX_MESH_COMMON_H

#include "GuTriangleVertexPointers.h"
#include "CmScaling.h"
#include "GuTriangleCache.h"

namespace physx
{
	namespace Gu
	{
		class Container;
	}

namespace Gu
{

	struct PolygonalData;
	class ContactBuffer;

	PX_FORCE_INLINE bool edgeCulling(const PxPlane& plane, const PxVec3& p0, const PxVec3& p1, PxReal contactDistance)
	{
		return plane.distance(p0)<=contactDistance || plane.distance(p1)<=contactDistance;
	}

	PX_FORCE_INLINE void getVertices(
		PxVec3* PX_RESTRICT localPoints, const Gu::InternalTriangleMeshData* meshDataLS,
		const Cm::FastVertex2ShapeScaling& meshScaling, PxU32 triangleIndex, bool idtMeshScale)
	{
		PxVec3 v0, v1, v2;
		Gu::TriangleVertexPointers::getTriangleVerts(meshDataLS, triangleIndex, v0, v1, v2);

		if(idtMeshScale)
		{
			localPoints[0] = v0;
			localPoints[1] = v1;
			localPoints[2] = v2;
		}
		else
		{
			localPoints[0] = meshScaling * v0;
			localPoints[1] = meshScaling * v1;
			localPoints[2] = meshScaling * v2;
		}
	}

	//Shared convex mesh contact generation object for use with HF and mesh objects
	struct ConvexVsMeshContactGeneration
	{
		Gu::Container&						mDelayedContacts;
		Gu::CacheMap<Gu::CachedEdge, 128>	mEdgeCache;
		Gu::CacheMap<Gu::CachedVertex, 128>	mVertCache;

		const Cm::Matrix34					m0to1;
		const Cm::Matrix34					m1to0;

		PxVec3								mHullCenterMesh;
		PxVec3								mHullCenterWorld;

		const PolygonalData&				mPolyData0;
		const Cm::Matrix34&					mWorld0;
		const Cm::Matrix34&					mWorld1;

		const Cm::FastVertex2ShapeScaling&	mConvexScaling;

		PxReal								mContactDistance;
		bool								mIdtMeshScale, mIdtConvexScale;
		PxReal								mCCDEpsilon;
		const PxTransform&					mTransform0;
		const PxTransform&					mTransform1;
		ContactBuffer&						mContactBuffer;
		bool								mAnyHits;

		ConvexVsMeshContactGeneration(
			Gu::Container& delayedContacts,

			const PxTransform& t0to1, const PxTransform& t1to0,

			const PolygonalData& polyData0, const Cm::Matrix34& world0, const Cm::Matrix34& world1,

			const Cm::FastVertex2ShapeScaling& convexScaling,

			PxReal contactDistance,
			bool idtConvexScale,
			PxReal cCCDEpsilon,
			const PxTransform& transform0, const PxTransform& transform1,
			ContactBuffer& contactBuffer
		);

		template <PxU32 TriangleCount>
		bool processTriangleCache(Gu::TriangleCache<TriangleCount>& cache)
		{
			PxU32 count = cache.mNumTriangles;
			PxVec3* verts = cache.mVertices;
			PxU32* vertInds = cache.mIndices;
			PxU32* triInds = cache.mTriangleIndex;
			PxU8* edgeFlags = cache.mEdgeFlags;
			while(count--)
			{
				processTriangle(verts, *triInds, *edgeFlags, vertInds);
				verts += 3;
				vertInds += 3;
				triInds++;
				edgeFlags++;
			}
			return true;
		}
		bool processTriangle(const PxVec3* verts, PxU32 triangleIndex, PxU8 triFlags, const PxU32* vertInds);
		void generateLastContacts();

		bool	generateContacts(
			const PxPlane& localPlane,
			const PxVec3* PX_RESTRICT localPoints,
			const PxVec3& triCenter, PxVec3& groupAxis,
			PxReal groupMinDepth, PxU32 index)	const;

	private:
		ConvexVsMeshContactGeneration& operator=(const ConvexVsMeshContactGeneration&);
	};

}
}

#endif
