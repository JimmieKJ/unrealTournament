/*
 * Copyright (c) 2008-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */


#include "NxApex.h"
#include "ApexSharedUtils.h"
#include "ApexRand.h"
#include "PxMemoryBuffer.h"

#if NX_SDK_VERSION_MAJOR == 2
#include <NxConvexMeshDesc.h>
#include <NxCooking.h>
#include <NxPhysicsSDK.h>
#include <NxShape.h>
#elif NX_SDK_VERSION_MAJOR == 3
#include "PxPhysics.h"
#include "cooking/PxConvexMeshDesc.h"
#include "cooking/PxCooking.h"
#include <PxShape.h>
#endif

#include "PsSort.h"

#include "NiApexSDK.h"

#include "NiCof44.h"
#include "foundation/PxPlane.h"

#include "../../shared/general/stan_hull/include/StanHull.h"


#define REDUCE_CONVEXHULL_INPUT_POINT_SET	0


namespace physx
{
namespace apex
{

// Local utilities

// copied from //sw/physx/PhysXSDK/3.3/trunk/Samples/SampleBase/RenderClothActor.cpp
PxReal det(PxVec4 v0, PxVec4 v1, PxVec4 v2, PxVec4 v3)
{
	const PxVec3& d0 = reinterpret_cast<const PxVec3&>(v0);
	const PxVec3& d1 = reinterpret_cast<const PxVec3&>(v1);
	const PxVec3& d2 = reinterpret_cast<const PxVec3&>(v2);
	const PxVec3& d3 = reinterpret_cast<const PxVec3&>(v3);

	return v0.w * d1.cross(d2).dot(d3)
		- v1.w * d0.cross(d2).dot(d3)
		+ v2.w * d0.cross(d1).dot(d3)
		- v3.w * d0.cross(d1).dot(d2);
}

PX_INLINE PxReal det(const physx::PxMat44& m)
{
	return det(m.column0, m.column1, m.column2, m.column3);
}

PX_INLINE physx::PxF32 extentDistance(physx::PxF32 min0, physx::PxF32 max0, physx::PxF32 min1, physx::PxF32 max1)
{
	return PxMax(min0 - max1, min1 - max0);
}

PX_INLINE physx::PxF32 extentDistanceAndNormalDirection(physx::PxF32 min0, physx::PxF32 max0, physx::PxF32 min1, physx::PxF32 max1, physx::PxF32& midpoint, bool& normalPointsFrom0to1)
{
	const physx::PxF32 d01 = min1 - max0;
	const physx::PxF32 d10 = min0 - max1;

	normalPointsFrom0to1 = d01 > d10;

	if (normalPointsFrom0to1)
	{
		midpoint = 0.5f*(min1 + max0);
		return d01;
	}
	else
	{
		midpoint = 0.5f*(min0 + max1);
		return d10;
	}
}

PX_INLINE void extentOverlapTimeInterval(physx::PxF32& tIn, physx::PxF32& tOut, physx::PxF32 vel0, physx::PxF32 min0, physx::PxF32 max0, physx::PxF32 min1, physx::PxF32 max1)
{
	if (physx::PxAbs(vel0) < PX_EPS_F32)
	{
		// Not moving
		if (extentDistance(min0, max0, min1, max1) < 0.0f)
		{
			// Always overlap
			tIn = -PX_MAX_F32;
			tOut = PX_MAX_F32;
		}
		else
		{
			// Never overlap
			tIn = PX_MAX_F32;
			tOut = -PX_MAX_F32;
		}
		return;
	}

	const physx::PxF32 recipVel0 = 1.0f / vel0;

	if (vel0 > 0.0f)
	{
		tIn = (min1 - max0) * recipVel0;
		tOut = (max1 - min0) * recipVel0;
	}
	else
	{
		tIn = (max1 - min0) * recipVel0;
		tOut = (min1 - max0) * recipVel0;
	}
}

PX_INLINE bool updateTimeIntervalAndNormal(physx::PxF32& in, physx::PxF32& out, physx::PxF32& tNormal, physx::PxVec3* normal, physx::PxF32 tIn, physx::PxF32 tOut, const physx::PxVec3& testNormal)
{
	if (tIn >= out || tOut <= in)
	{
		return false;	// No intersection will occur
	}

	if (normal != NULL && tIn > tNormal)
	{
		tNormal = tIn;
		*normal = testNormal;
	}

	in = physx::PxMax(tIn, in);
	out = physx::PxMin(tOut, out);

	return true;
}

PX_INLINE void getExtent(physx::PxF32& min, physx::PxF32& max, const void* points, physx::PxU32 pointCount, const physx::PxU32 pointByteStride, const physx::PxVec3& normal)
{
	min = PX_MAX_F32;
	max = -PX_MAX_F32;
	const physx::PxVec3* p = (const physx::PxVec3*)points;
	for (physx::PxU32 i = 0; i < pointCount; ++i, p = (const physx::PxVec3*)(((const physx::PxU8*)p) + pointByteStride))
	{
		const physx::PxF32 d = normal.dot(*p);
		if (d < min)
		{
			min = d;
		}
		if (d > max)
		{
			max = d;
		}
	}
}

PX_INLINE bool intersectPlanes(physx::PxVec3& point, const physx::PxPlane& p0, const physx::PxPlane& p1, const physx::PxPlane& p2)
{
	const physx::PxVec3 p1xp2 = p1.n.cross(p2.n);
	const physx::PxF32 det = p0.n.dot(p1xp2);
	if (physx::PxAbs(det) < 1.0e-18)
	{
		return false;	// No intersection
	}
	point = (-p0.d * p1xp2 - p1.d * (p2.n.cross(p0.n)) - p2.d * (p0.n.cross(p1.n))) / det;
	return true;
}

PX_INLINE bool pointInsidePlanes(const physx::PxVec3& point, const physx::PxPlane* planes, physx::PxU32 numPlanes, physx::PxF32 eps)
{
	for (physx::PxU32 i = 0; i < numPlanes; ++i)
	{
		const physx::PxPlane& plane = planes[i];
		if (plane.distance(point) > eps)
		{
			return false;
		}
	}
	return true;
}

static void findPrincipleComponents(physx::PxVec3& mean, physx::PxVec3& variance, physx::PxMat33& axes, const physx::PxVec3* points, physx::PxU32 pointCount)
{
	// Find the average of the points
	mean = physx::PxVec3(0.0f);
	for (physx::PxU32 i = 0; i < pointCount; ++i)
	{
		mean += points[i];
	}

	physx::PxMat33 cov = physx::PxMat33::createZero();

	if (pointCount > 0)
	{
		mean *= 1.0f/pointCount;

		// Now subtract the mean and add to the covariance matrix cov
		for (physx::PxU32 i = 0; i < pointCount; ++i)
		{
			const physx::PxVec3 relativePoint = points[i] - mean;
			for (physx::PxU32 j = 0; j < 3; ++j)
			{
				for (physx::PxU32 k = 0; k < 3; ++k)
				{
					cov(j,k) += relativePoint[j]*relativePoint[k];
				}
			}
		}
		cov *= 1.0f/pointCount;
	}

	// Now diagonalize cov
	variance = diagonalizeSymmetric(axes, cov);
}

#if 0
/* John's k-means clustering function, slightly specialized */
static physx::PxU32 kmeans_cluster(const physx::PxVec3* input,
								   physx::PxU32 inputCount,
								   physx::PxVec3* clusters,
								   physx::PxU32 clumpCount,
								   physx::PxF32 threshold = 0.01f, // controls how long it works to converge towards a least errors solution.
								   physx::PxF32 collapseDistance = 0.0001f) // distance between clumps to consider them to be essentially equal.
{
	physx::PxU32 convergeCount = 64; // maximum number of iterations attempting to converge to a solution..
	physx::Array<physx::PxU32> counts(clumpCount);
	physx::PxF32 error = 0.0f;
	if (inputCount <= clumpCount) // if the number of input points is less than our clumping size, just return the input points.
	{
		clumpCount = inputCount;
		for (physx::PxU32 i = 0; i < inputCount; ++i)
		{
			clusters[i] = input[i];
			counts[i] = 1;
		}
	}
	else
	{
		physx::Array<physx::PxVec3> centroids(clumpCount);

		// Take a sampling of the input points as initial centroid estimates.
		for (physx::PxU32 i = 0; i < clumpCount; ++i)
		{
			physx::PxU32 index = (i*inputCount)/clumpCount;
			PX_ASSERT( index < inputCount );
			clusters[i] = input[index];
		}

		// Here is the main convergence loop
		physx::PxF32 old_error = PX_MAX_F32;	// old and initial error estimates are max physx::PxF32
		error = PX_MAX_F32;
		do
		{
			old_error = error;	// preserve the old error
			// reset the counts and centroids to current cluster location
			for (physx::PxU32 i = 0; i < clumpCount; ++i)
			{
				counts[i] = 0;
				centroids[i] = physx::PxVec3(0.0f);
			}
			error = 0.0f;
			// For each input data point, figure out which cluster it is closest too and add it to that cluster.
			for (physx::PxU32 i = 0; i < inputCount; ++i)
			{
				physx::PxF32 min_distance2 = PX_MAX_F32;
				physx::PxU32 j_nearest = 0;
				// find the nearest clump to this point.
				for (physx::PxU32 j = 0; j < clumpCount; ++j)
				{
					const physx::PxF32 distance2 = (input[i]-clusters[j]).magnitudeSquared();
					if (distance2 < min_distance2)
					{
						min_distance2 = distance2;
						j_nearest = j; // save which clump this point indexes
					}
				}
				centroids[j_nearest] += input[i];
				++counts[j_nearest];	// increment the counter indicating how many points are in this clump.
				error += min_distance2; // save the error accumulation
			}
			// Now, for each clump, compute the mean and store the result.
			for (physx::PxU32 i = 0; i < clumpCount; ++i)
			{
				if (counts[i]) // if this clump got any points added to it...
				{
					centroids[i] /= (physx::PxF32)counts[i];	// compute the average center of the points in this clump.
					clusters[i] = centroids[i]; // store it as the new cluster.
				}
			}
			// decrement the convergence counter and bail if it is taking too long to converge to a solution.
			--convergeCount;
			if (convergeCount == 0)
			{
				break;
			}
			if (error < threshold) // early exit if our first guess is already good enough (if all input points are the same)
			{
				break;
			}
		} while (physx::PxAbs(error - old_error) > threshold); // keep going until the error is reduced by this threshold amount.
	}

	// ok..now we prune the clumps if necessary.
	// The rules are; first, if a clump has no 'counts' then we prune it as it's unused.
	// The second, is if the centroid of this clump is essentially  the same (based on the distance tolerance)
	// as an existing clump, then it is pruned and all indices which used to point to it, now point to the one
	// it is closest too.
	physx::PxU32 outCount = 0; // number of clumps output after pruning performed.
	physx::PxF32 d2 = collapseDistance*collapseDistance; // squared collapse distance.
	for (physx::PxU32 i = 0; i < clumpCount; ++i)
	{
		if (counts[i] == 0) // if no points ended up in this clump, eliminate it.
		{
			continue;
		}
		// see if this clump is too close to any already accepted clump.
		bool add = true;
		for (physx::PxU32 j = 0; j < outCount; ++j)
		{
			if ((clusters[i]-clusters[j]).magnitudeSquared() < d2)
			{
				add = false; // we do not add this clump
				break;
			}
		}
		if (add)
		{
			clusters[outCount++] = clusters[i];
		}
	}

	clumpCount = outCount;

	return clumpCount;
};
#endif

// barycentric utilities
void generateBarycentricCoordinatesTri(const physx::PxVec3& pa, const physx::PxVec3& pb, const physx::PxVec3& pc, const physx::PxVec3& p, physx::PxF32& s, physx::PxF32& t)
{
	// pe = pb-ba, pf = pc-pa
	// Barycentric coordinates: (s,t) -> pa + s*pe + t*pf
	// Minimize: [s*pe + t*pf - (p-pa)]^2
	// Minimize: a s^2 + 2 b s t + c t^2 + 2 d s + 2 e t + f
	// Minimization w.r.t s and t :
	//    as + bt = -d
	//    bs + ct = -e

	physx::PxVec3 pe = pb - pa;
	physx::PxVec3 pf = pc - pa;
	physx::PxVec3 pg = pa - p;

	physx::PxF32 a = pe.dot(pe);
	physx::PxF32 b = pe.dot(pf);
	physx::PxF32 c = pf.dot(pf);
	physx::PxF32 d = pe.dot(pg);
	physx::PxF32 e = pf.dot(pg);

	physx::PxF32 det = a * c - b * b;
	if (det != 0.0f)
	{
		s = (b * e - c * d) / det;
		t = (b * d - a * e) / det;
	}
}

void generateBarycentricCoordinatesTet(const physx::PxVec3& pa, const physx::PxVec3& pb, const physx::PxVec3& pc, const physx::PxVec3& pd, const physx::PxVec3& p, physx::PxVec3& bary)
{
	physx::PxVec3 q  = p - pd;
	physx::PxVec3 q0 = pa - pd;
	physx::PxVec3 q1 = pb - pd;
	physx::PxVec3 q2 = pc - pd;
	physx::PxMat33Legacy m;
	m.setColumn(0, q0);
	m.setColumn(1, q1);
	m.setColumn(2, q2);
	float det = m.determinant();
	m.setColumn(0, q);
	bary.x = m.determinant();
	m.setColumn(0, q0);
	m.setColumn(1, q);
	bary.y = m.determinant();
	m.setColumn(1, q1);
	m.setColumn(2, q);
	bary.z = m.determinant();
	if (det != 0.0f)
	{
		bary /= det;
	}
}

/* TriangleFrame */

size_t TriangleFrame::s_offsets[TriangleFrame::VertexFieldCount];

static TriangleFrameBuilder sTriangleFrameBuilder;

/*
	File-local functions and definitions
 */

struct Marker
{
	physx::PxF32	pos;
	physx::PxU32	id;	// lsb = type (0 = max, 1 = min), other bits used for object index

	void	set(physx::PxF32 _pos, physx::PxI32 _id)
	{
		pos = _pos;
		id = (physx::PxU32)_id;
	}
};

static int compareMarkers(const void* A, const void* B)
{
	// Sorts by value.  If values equal, sorts min types greater than max types, to reduce the # of overlaps
	const physx::PxF32 delta = ((Marker*)A)->pos - ((Marker*)B)->pos;
	return delta != 0 ? (delta < 0 ? -1 : 1) : ((int)(((Marker*)A)->id & 1) - (int)(((Marker*)B)->id & 1));
}

struct EdgeWithFace
{
	EdgeWithFace() {}
	EdgeWithFace(physx::PxU32 v0, physx::PxU32 v1, physx::PxU32 face, physx::PxU32 oppositeFace, bool sort = false) : faceIndex(face), v0Index(v0), v1Index(v1), opposite(oppositeFace)
	{
		if (sort)
		{
			if (v1Index < v0Index)
			{
				physx::swap(v0Index, v1Index);
			}
		}
	}

	bool operator == (const EdgeWithFace& e)
	{
		return v0Index == e.v0Index && v1Index == e.v1Index;
	}

	physx::PxU32	faceIndex;
	physx::PxU32	v0Index;
	physx::PxU32	v1Index;
	physx::PxU32	opposite;
};

/*
	Global utilities
 */

void boundsCalculateOverlaps(physx::Array<IntPair>& overlaps, Bounds3Axes axesToUse, const BoundsRep* bounds, physx::PxU32 boundsCount, physx::PxU32 boundsByteStride,
                             const BoundsInteractions& interactions, bool append)
{
	if (!append)
	{
		overlaps.reset();
	}

	physx::PxU32 D = 0;
	physx::PxU32 axisNums[3];
	for (unsigned i = 0; i < 3; ++i)
	{
		if ((axesToUse >> i) & 1)
		{
			axisNums[D++] = i;
		}
	}

	if (D == 0 || D > 3)
	{
		return;
	}

	physx::Array< physx::Array<Marker> > axes;
	axes.resize(D);
	physx::PxU32 overlapCount[3];

	for (physx::PxU32 n = 0; n < D; ++n)
	{
		const physx::PxU32 axisNum = axisNums[n];
		physx::Array<Marker>& axis = axes[n];
		overlapCount[n] = 0;
		axis.resize(2 * boundsCount);
		physx::PxU8* boundsPtr = (physx::PxU8*)bounds;
		for (physx::PxU32 i = 0; i < boundsCount; ++i, boundsPtr += boundsByteStride)
		{
			const BoundsRep& boundsRep = *(const BoundsRep*)boundsPtr;
			const physx::PxBounds3& box = boundsRep.aabb;
			physx::PxF32 min = box.minimum[axisNum];
			physx::PxF32 max = box.maximum[axisNum];
			if (min >= max)
			{
				const physx::PxF32 mid = 0.5f * (min + max);
				physx::PxF32 pad = 0.000001f * fabs(mid);
				min = mid - pad;
				max = mid + pad;
			}
			axis[i << 1].set(min, (physx::PxI32)i << 1 | 1);
			axis[i << 1 | 1].set(max, (physx::PxI32)i << 1);
		}
		qsort(axis.begin(), axis.size(), sizeof(Marker), compareMarkers);
		physx::PxU32 localOverlapCount = 0;
		for (physx::PxU32 i = 0; i < axis.size(); ++i)
		{
			Marker& marker = axis[i];
			if (marker.id & 1)
			{
				overlapCount[n] += localOverlapCount;
				++localOverlapCount;
			}
			else
			{
				--localOverlapCount;
			}
		}
	}

	int axis0;
	int axis1;
	int axis2;
	int maxBin;
	if (D == 1)
	{
		maxBin = 0;
		axis0 = (physx::PxI32)axisNums[0];
		axis1 = axis0;
		axis2 = axis0;
	}
	else if (D == 2)
	{
		if (overlapCount[0] < overlapCount[1])
		{
			maxBin = 0;
			axis0 = (physx::PxI32)axisNums[0];
			axis1 = (physx::PxI32)axisNums[1];
			axis2 = axis0;
		}
		else
		{
			maxBin = 1;
			axis0 = (physx::PxI32)axisNums[1];
			axis1 = (physx::PxI32)axisNums[0];
			axis2 = axis0;
		}
	}
	else
	{
		maxBin = overlapCount[0] < overlapCount[1] ? (overlapCount[0] < overlapCount[2] ? 0 : 2) : (overlapCount[1] < overlapCount[2] ? 1 : 2);
		axis0 = (physx::PxI32)axisNums[maxBin];
		axis1 = (axis0 + 1) % 3;
		axis2 = (axis0 + 2) % 3;
	}

	const physx::PxU64 interactionBits = interactions.bits;

	NxIndexBank<physx::PxU32> localOverlaps(boundsCount);
	physx::Array<Marker>& axis = axes[(physx::PxU32)maxBin];
	physx::PxF32 boxMin1 = 0.0f;
	physx::PxF32 boxMax1 = 0.0f;
	physx::PxF32 boxMin2 = 0.0f;
	physx::PxF32 boxMax2 = 0.0f;

	for (physx::PxU32 i = 0; i < axis.size(); ++i)
	{
		Marker& marker = axis[i];
		const physx::PxU32 index = marker.id >> 1;
		if (marker.id & 1)
		{
			const BoundsRep& boundsRep = *(const BoundsRep*)((physx::PxU8*)bounds + index*boundsByteStride);
			const physx::PxU8 interaction = (physx::PxU8)((interactionBits >> (boundsRep.type << 3)) & 0xFF);
			const physx::PxBounds3& box = boundsRep.aabb;
			// These conditionals compile out with optimization:
			if (D > 1)
			{
				boxMin1 = box.minimum[axis1];
				boxMax1 = box.maximum[axis1];
				if (D == 3)
				{
					boxMin2 = box.minimum[axis2];
					boxMax2 = box.maximum[axis2];
				}
			}
			const physx::PxU32 localOverlapCount = localOverlaps.usedCount();
			const physx::PxU32* localOverlapIndices = localOverlaps.usedIndices();
			for (physx::PxU32 j = 0; j < localOverlapCount; ++j)
			{
				const physx::PxU32 overlapIndex = localOverlapIndices[j];
				const BoundsRep& overlapBoundsRep = *(const BoundsRep*)((physx::PxU8*)bounds + overlapIndex*boundsByteStride);
				if ((interaction >> overlapBoundsRep.type) & 1)
				{
					const physx::PxBounds3& overlapBox = overlapBoundsRep.aabb;
					// These conditionals compile out with optimization:
					if (D > 1)
					{
						if (boxMin1 >= overlapBox.maximum[axis1] || boxMax1 <= overlapBox.minimum[axis1])
						{
							continue;
						}
						if (D == 3)
						{
							if (boxMin2 >= overlapBox.maximum[axis2] || boxMax2 <= overlapBox.minimum[axis2])
							{
								continue;
							}
						}
					}
					// Add overlap
					IntPair& pair = overlaps.insert();
					pair.i0 = (physx::PxI32)index;
					pair.i1 = (physx::PxI32)overlapIndex;
				}
			}
			PX_ASSERT(localOverlaps.isValid(index));
			PX_ASSERT(!localOverlaps.isUsed(index));
			localOverlaps.use(index);
		}
		else
		{
			// Remove local overlap
			PX_ASSERT(localOverlaps.isValid(index));
			localOverlaps.free(index);
		}
	}
}


/*
	ConvexHull functions
 */

ConvexHull::ConvexHull() : mParams(NULL), mOwnsParams(false)
{
}

ConvexHull::~ConvexHull()
{
	term();
}

// If params == NULL, ConvexHull will build (and own) its own internal parameters
void ConvexHull::init(NxParameterized::Interface* params)
{
	term();

	if (params != NULL)
	{
		mParams = DYNAMIC_CAST(ConvexHullParameters*)(params);
		if (mParams == NULL)
		{
			PX_ASSERT(!"params must be ConvexHullParameters");
		}
	}
	else
	{
		NxParameterized::Traits* traits = NiGetApexSDK()->getParameterizedTraits();
		mParams = DYNAMIC_CAST(ConvexHullParameters*)(traits->createNxParameterized(ConvexHullParameters::staticClassName()));
		PX_ASSERT(mParams != NULL);
		if (mParams != NULL)
		{
			mOwnsParams = true;
			setEmpty();
		}
	}
}


NxParameterized::Interface*	ConvexHull::giveOwnersipOfNxParameters()
{
	if (mOwnsParams)
	{
		mOwnsParams = false;
		return mParams;
	}

	return NULL;
}

// Releases parameters if it owns them
void ConvexHull::term()
{
	if (mParams != NULL && mOwnsParams)
	{
		mParams->destroy();
	}
	mParams = NULL;
	mOwnsParams = false;
}

static int compareEdgesWithFaces(const void* A, const void* B)
{
	const EdgeWithFace& eA = *(const EdgeWithFace*)A;
	const EdgeWithFace& eB = *(const EdgeWithFace*)B;
	if (eA.v0Index != eB.v0Index)
	{
		return (int)eA.v0Index - (int)eB.v0Index;
	}
	if (eA.v1Index != eB.v1Index)
	{
		return (int)eA.v1Index - (int)eB.v1Index;
	}
	return (int)eA.faceIndex - (int)eB.faceIndex;
}

void ConvexHull::buildFromDesc(const ConvexHullDesc& desc)
{
	if (!desc.isValid())
	{
		setEmpty();
		return;
	}

	setEmpty();

	Array<physx::PxPlane>	uniquePlanes;
	Array<PxU32>	edges;
	Array<PxF32>	widths;

	NxParameterized::Handle handle(*mParams);

	// Copy vertices and compute bounds
	mParams->getParameterHandle("vertices", handle);
	mParams->resizeArray(handle, (physx::PxI32)desc.numVertices);
	const physx::PxU8* vertPointer = (const physx::PxU8*)desc.vertices;
	for (physx::PxU32 i = 0; i < desc.numVertices; ++i, vertPointer += desc.vertexStrideBytes)
	{
		mParams->vertices.buf[i] = *(const physx::PxVec3*)vertPointer;
		mParams->bounds.include(mParams->vertices.buf[i]);
	}

	physx::PxVec3 center;
	center = mParams->bounds.getCenter();

	// Find faces and compute volume
	physx::Array<EdgeWithFace> fEdges;
	const physx::PxU32* indices = desc.indices;
	mParams->volume = 0.0f;
	for (physx::PxU32 fI = 0; fI < desc.numFaces; ++fI)
	{
		const physx::PxU32 indexCount = (const physx::PxU32)desc.faceIndexCounts[fI];
		const physx::PxVec3& vI0 = mParams->vertices.buf[indices[0]];
		physx::PxVec3 normal(0.0f);
		for (physx::PxU32 pI = 1; pI < indexCount-1; ++pI)
		{
			const physx::PxU32 i1 = indices[pI];
			const physx::PxU32 i2 = indices[pI+1];
			const physx::PxVec3& vI1 = mParams->vertices.buf[i1];
			const physx::PxVec3& vI2 = mParams->vertices.buf[i2];
			const physx::PxVec3 eI1 = vI1 - vI0;
			const physx::PxVec3 eI2 = vI2 - vI0;
			physx::PxVec3 normalI = eI1.cross(eI2);
			mParams->volume += (normalI.dot(vI0 - center));
			normal += normalI;
		}

		const bool normalValid = normal.normalize() > 0.0f;

		physx::PxF32 d = 0.0f;
		for (physx::PxU32 pI = 0; pI < indexCount; ++pI)
		{
			d -= normal.dot(mParams->vertices.buf[indices[pI]]);
		}
		d /= indexCount;

		physx::PxU32 oppositeFace = 0;
		physx::PxU32 faceN = uniquePlanes.size();	// unless we find an opposite

		if (normalValid)
		{
			// See if this face is opposite an existing one
			for (physx::PxU32 j = 0; j < uniquePlanes.size(); ++j)
			{
				if (normal.dot(uniquePlanes[j].n) < -0.999999f && widths[j] == PX_MAX_F32)
				{
					oppositeFace = 1;
					faceN = j;
					widths[j] = -d - uniquePlanes[j].d;
					break;
				}
			}
		}

		if (!oppositeFace)
		{
			uniquePlanes.pushBack(physx::PxPlane(normal, d));
			widths.pushBack(PX_MAX_F32);
		}

		for (physx::PxU32 pI = 0; pI < indexCount; ++pI)
		{
			fEdges.pushBack(EdgeWithFace(indices[pI], indices[(pI+1)%indexCount], faceN, oppositeFace, true));
		}

		indices += indexCount;
	}

	// Create a map from current plane indices to the arrangement we'll have after the following loop
	// This map will be its inverse
	physx::Array<physx::PxU32> invMap;
	invMap.resize(2*widths.size());
	for (physx::PxU32 i = 0; i < invMap.size(); ++i)
	{
		invMap[i] = i;
	}

	// This ensures that all the slabs are represented first in the planes list
	physx::PxU32 slabCount = widths.size();
	for (physx::PxU32 i = widths.size(); i--;)
	{
		if (widths[i] == PX_MAX_F32)
		{
			--slabCount;
			physx::swap(widths[i], widths[slabCount]);
			physx::swap(uniquePlanes[i], uniquePlanes[slabCount]);
			physx::swap(invMap[i], invMap[slabCount]);
		}
	}

	// Now invert invMap to get the map
	physx::Array<physx::PxU32> map;
	map.resize(invMap.size());
	for (physx::PxU32 i = 0; i < map.size(); ++i)
	{
		map[invMap[i]] = i;
	}

	// Plane indices have been rearranged, need to make corresponding rearrangements to fEdges' faceIndex value.
	for (physx::PxU32 i = 0; i < fEdges.size(); ++i)
	{
		fEdges[i].faceIndex = map[fEdges[i].faceIndex];
		if (fEdges[i].opposite)
		{
			fEdges[i].faceIndex += uniquePlanes.size();
		}
	}

	// Total plane count, with non-unique (back-facing) normals
	mParams->planeCount = uniquePlanes.size() + slabCount;

	// Fill in remaining widths
	for (physx::PxU32 fI = mParams->planeCount - uniquePlanes.size(); fI < widths.size(); ++fI)
	{
		widths[fI] = 0.0f;
		for (physx::PxU32 vI = 0; vI < desc.numVertices; ++vI)
		{
			const physx::PxF32 vDepth = -uniquePlanes[fI].distance(mParams->vertices.buf[vI]);
			if (vDepth > widths[fI])
			{
				widths[fI] = vDepth;
			}
		}
	}

	// Record faceIndex pairs in adjacentFaces
	physx::Array<physx::PxU32> adjacentFaces;

	// Eliminate redundant edges
	qsort(fEdges.begin(), fEdges.size(), sizeof(EdgeWithFace), compareEdgesWithFaces);
	for (physx::PxU32 eI = 0; eI < fEdges.size();)
	{
		physx::PxU32 i0 = fEdges[eI].v0Index;
		physx::PxU32 i1 = fEdges[eI].v1Index;
		PX_ASSERT(i0 < 65536 && i1 < 65536);
		if (eI < fEdges.size() - 1 && fEdges[eI] == fEdges[eI + 1])
		{
			if (fEdges[eI].faceIndex != fEdges[eI + 1].faceIndex)
			{
				edges.pushBack(i0 << 16 | i1);
				adjacentFaces.pushBack(fEdges[eI].faceIndex << 16 | fEdges[eI + 1].faceIndex);
			}
			eI += 2;
		}
		else
		{
			edges.pushBack(i0 << 16 | i1);
			adjacentFaces.pushBack(0xFFFF0000 | fEdges[eI].faceIndex);
			++eI;
		}
	}

	// Find unique edge directions, put them at the front of the edge list
	const physx::PxU32 edgeCount = edges.size();
	if (edgeCount > 0)
	{
		mParams->uniqueEdgeDirectionCount = 1;
		for (physx::PxU32 testEdgeIndex = 1; testEdgeIndex < edgeCount; ++testEdgeIndex)
		{
			const physx::PxU32 testEdgeEndpointIndices = edges[testEdgeIndex];
			const physx::PxVec3 testEdge = mParams->vertices.buf[testEdgeEndpointIndices & 0xFFFF] - mParams->vertices.buf[testEdgeEndpointIndices >> 16];
			const physx::PxF32 testEdge2 = testEdge.magnitudeSquared();
			physx::PxU32 uniqueEdgeIndex = 0;
			for (; uniqueEdgeIndex < mParams->uniqueEdgeDirectionCount; ++uniqueEdgeIndex)
			{
				const physx::PxU32 uniqueEdgeEndpointIndices = edges[uniqueEdgeIndex];
				const physx::PxVec3 uniqueEdge = mParams->vertices.buf[uniqueEdgeEndpointIndices & 0xFFFF] - mParams->vertices.buf[uniqueEdgeEndpointIndices >> 16];
				if (uniqueEdge.cross(testEdge).magnitudeSquared() < testEdge2 * uniqueEdge.magnitudeSquared()*PX_EPS_F32 * PX_EPS_F32)
				{
					break;
				}
			}
			if (uniqueEdgeIndex == mParams->uniqueEdgeDirectionCount)
			{
				physx::swap(edges[mParams->uniqueEdgeDirectionCount], edges[testEdgeIndex]);
				physx::swap(adjacentFaces[mParams->uniqueEdgeDirectionCount], adjacentFaces[testEdgeIndex]);
				++mParams->uniqueEdgeDirectionCount;
			}
		}
	}

	mParams->volume *= 0.1666666667f;

	// Transfer from temporary arrays into mParams

	// uniquePlanes
	mParams->getParameterHandle("uniquePlanes", handle);
	mParams->resizeArray(handle, (physx::PxI32)uniquePlanes.size());
	for (physx::PxU32 i = 0; i < uniquePlanes.size(); ++i)
	{
		physx::PxPlane& plane = uniquePlanes[i];
		ConvexHullParametersNS::Plane_Type& paramPlane = mParams->uniquePlanes.buf[i];
		paramPlane.normal = plane.n;
		paramPlane.d = plane.d;
	}

	// edges
	mParams->getParameterHandle("edges", handle);
	mParams->resizeArray(handle, (physx::PxI32)edges.size());
	mParams->setParamU32Array(handle, edges.begin(), (physx::PxI32)edges.size());

	// adjacentFaces
	mParams->getParameterHandle("adjacentFaces", handle);
	mParams->resizeArray(handle, (physx::PxI32)adjacentFaces.size());
	mParams->setParamU32Array(handle, adjacentFaces.begin(), (physx::PxI32)adjacentFaces.size());

	// widths
	mParams->getParameterHandle("widths", handle);
	mParams->resizeArray(handle, (physx::PxI32)widths.size());
	mParams->setParamF32Array(handle, widths.begin(), (physx::PxI32)widths.size());

}

void ConvexHull::buildFromPoints(const void* points, physx::PxU32 numPoints, physx::PxU32 pointStrideBytes)
{
	if (numPoints < 4)
	{
		setEmpty();
		return;
	}

	// First try stanhull

	// We will transform the points of the hull such that they fit well into a rotated 2x2x2 cube.
	// To find a suitable set of axes for this rotated cube, we will find the  of the point distribution.

	// Create an array of points which we will transform
	physx::Array<physx::PxVec3> transformedPoints(numPoints);
	physx::PxU8* pointPtr = (physx::PxU8*)points;
	for (physx::PxU32 i = 0; i < numPoints; ++i, pointPtr += pointStrideBytes)
	{
		transformedPoints[i] = *(physx::PxVec3*)pointPtr;
	}

	// Optionally reduce the point set
#if REDUCE_CONVEXHULL_INPUT_POINT_SET
	const physx::PxU32 maxSetSize = 400;
	physx::Array<physx::PxVec3> reducedPointSet(numPoints);
	const physx::PxU32 reducedSetSize = kmeans_cluster(&transformedPoints[0], numPoints, &reducedPointSet[0], maxSetSize);
	PX_ASSERT(reducedSetSize <= 400 && reducedSetSize >= 4);
	transformedPoints = reducedPointSet;
#endif // REDUCE_CONVEXHULL_INPUT_POINT_SET

	physx::PxVec3 mean;
	physx::PxVec3 variance;
	physx::PxMat33 axes;
	findPrincipleComponents(mean, variance, axes, &transformedPoints[0], numPoints);

	// Now subtract the mean from the points and rotate the points into the frame of the axes
	for (physx::PxU32 i = 0; i < numPoints; ++i)
	{
		transformedPoints[i] = axes.transformTranspose(transformedPoints[i] - mean);
	}

	// Finally find a scale such that the maximum absolute coordinate on each axis is 1
	physx::PxVec3 scale(0.0f);
	for (physx::PxU32 i = 0; i < numPoints; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			scale[j] = physx::PxMax(scale[j], physx::PxAbs(transformedPoints[i][j]));
		}
	}
	if (scale.x*scale.y*scale.z == 0.0f)
	{
		// We have a degeneracy, planar (or colinear or coincident) points
		setEmpty();
		return;
	}

	// Now scale the points
	const physx::PxVec3 recipScale(1.0f/scale.x, 1.0f/scale.y, 1.0f/scale.z);
	for (physx::PxU32 i = 0; i < numPoints; ++i)
	{
		transformedPoints[i] = transformedPoints[i].multiply(recipScale);
	}

	// The points are transformed, and for completeness let us now build the transform which restores them
	const physx::PxMat44 tm(axes.column0*scale.x, axes.column1*scale.y, axes.column2*scale.z, mean);

#if 0
	// Now find the convex hull of the transformed points
	physx::stanhull::HullDesc hullDesc;
	hullDesc.mVertices = (const physx::PxF32*)&transformedPoints[0];
	hullDesc.mVcount = numPoints;
	hullDesc.mVertexStride = sizeof(physx::PxVec3);
	hullDesc.mSkinWidth = 0.0f;

	physx::stanhull::HullLibrary hulllib;
	physx::stanhull::HullResult hull;
	if (physx::stanhull::QE_OK == hulllib.CreateConvexHull(hullDesc, hull))
	{	// Success
		ConvexHullDesc desc;
		desc.vertices = hull.mOutputVertices;
		desc.numVertices = hull.mNumOutputVertices;
		desc.vertexStrideBytes = 3*sizeof(physx::PxF32);
		desc.indices = hull.mIndices;
		desc.numIndices = hull.mNumIndices;
		desc.faceIndexCounts = hull.mFaces;
		desc.numFaces = hull.mNumFaces;
		buildFromDesc(desc);
		hulllib.ReleaseResult(hull);

		// Transform our hull back before returning
		applyTransformation(tm);

		return;
	}
#endif

	// Stanhull failed, try physx sdk cooker
	NxApexSDK* sdk = NxGetApexSDK();
	if (sdk && sdk->getCookingInterface() && sdk->getPhysXSDK())
	{
#if NX_SDK_VERSION_MAJOR == 2
		NxConvexMeshDesc convexMeshDesc;
		convexMeshDesc.numVertices = numPoints;
		convexMeshDesc.points = &transformedPoints[0];
		convexMeshDesc.pointStrideBytes = pointStrideBytes;
		convexMeshDesc.flags = NX_CF_COMPUTE_CONVEX;

		PxMemoryBuffer stream;
		stream.setEndianMode(physx::PxFileBuf::ENDIAN_NONE);
		NxFromPxStream nxs(stream);
		if (sdk->getCookingInterface()->NxCookConvexMesh(convexMeshDesc, nxs))
		{
			NxConvexMesh* mesh = sdk->getPhysXSDK()->createConvexMesh(nxs);
			if (mesh)
			{
				buildFromConvexMesh(mesh);
				sdk->getPhysXSDK()->releaseConvexMesh(*mesh);
				// Transform our hull back before returning
				applyTransformation(tm);
				return;
			}
		}
#elif NX_SDK_VERSION_MAJOR == 3
		PxConvexMeshDesc convexMeshDesc;
		convexMeshDesc.points.count = numPoints;
		convexMeshDesc.points.data = &transformedPoints[0];
		convexMeshDesc.points.stride = pointStrideBytes;
		convexMeshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

		PxMemoryBuffer stream;
		stream.setEndianMode(physx::PxFileBuf::ENDIAN_NONE);
		NxFromPxStream nxs(stream);
		if (sdk->getCookingInterface()->cookConvexMesh(convexMeshDesc, nxs))
		{
			PxConvexMesh* mesh = sdk->getPhysXSDK()->createConvexMesh(nxs);
			if (mesh)
			{
				buildFromConvexMesh(mesh);
				mesh->release();
				// Transform our hull back before returning
				applyTransformation(tm);
				return;
			}
		}
#endif
	}

	// No luck
	setEmpty();
}

void ConvexHull::buildFromPlanes(const physx::PxPlane* planes, physx::PxU32 numPlanes, physx::PxF32 eps)
{
	physx::Array<physx::PxVec3> points;
	points.reserve((numPlanes * (numPlanes - 1) * (numPlanes - 2)) / 6);
	for (physx::PxU32 i = 0; i < numPlanes; ++i)
	{
		const physx::PxPlane& planeI = planes[i];
		for (physx::PxU32 j = i + 1; j < numPlanes; ++j)
		{
			const physx::PxPlane& planeJ = planes[j];
			for (physx::PxU32 k = j + 1; k < numPlanes; ++k)
			{
				const physx::PxPlane& planeK = planes[k];
				physx::PxVec3 point;
				if (intersectPlanes(point, planeI, planeJ, planeK))
				{
					if (pointInsidePlanes(point, planes, i, eps) &&
					        pointInsidePlanes(point, planes + i + 1, j - (i + 1), eps) &&
					        pointInsidePlanes(point, planes + j + 1, k - (j + 1), eps) &&
					        pointInsidePlanes(point, planes + k + 1, numPlanes - (k + 1), eps))
					{
						physx::PxU32 m = 0;
						for (; m < points.size(); ++m)
						{
							if ((point - points[m]).magnitudeSquared() < eps)
							{
								break;
							}
						}
						if (m == points.size())
						{
							points.pushBack(point);
						}
					}
				}
			}
		}
	}

	buildFromPoints(points.begin(), points.size(), sizeof(physx::PxVec3));
}

#if NX_SDK_VERSION_MAJOR == 2
void ConvexHull::buildFromConvexMesh(const ConvexMesh* mesh)
{
	setEmpty();

	if (!mesh)
	{
		return;
	}

	PX_ASSERT(mesh->getFormat(0, NX_ARRAY_TRIANGLES) == NX_FORMAT_INT);
	PX_ASSERT(mesh->getStride(0, NX_ARRAY_TRIANGLES) == sizeof(physx::PxU32) * 3);
	PX_ASSERT(mesh->getStride(0, NX_ARRAY_VERTICES) == sizeof(physx::PxVec3));

	const physx::PxU32 numVerts = mesh->getCount(0, NX_ARRAY_VERTICES);
	const physx::PxVec3* verts = (const physx::PxVec3*)mesh->getBase(0, NX_ARRAY_VERTICES);
	const physx::PxU32 numTris = mesh->getCount(0, NX_ARRAY_TRIANGLES);
	const physx::PxU32* index = (const physx::PxU32*)mesh->getBase(0, NX_ARRAY_TRIANGLES);

	Array<physx::PxPlane>	uniquePlanes;
	Array<PxU32>	edges;
	Array<PxF32>	widths;

	NxParameterized::Handle handle(*mParams);

	// Copy vertices and compute bounds
	mParams->getParameterHandle("vertices", handle);
	mParams->resizeArray(handle, numVerts);
	for (physx::PxU32 i = 0; i < numVerts; ++i)
	{
		mParams->vertices.buf[i] = verts[i];
		mParams->bounds.include(verts[i]);
	}

	physx::PxVec3 center;
	center = mParams->bounds.getCenter();
	mParams->volume = 0.0f;

	// Find planes and compute volume
	physx::Array<EdgeWithFace> fEdges;
	const physx::PxU32 numIndices = 3 * numTris;
	for (physx::PxU32 vI = 0; vI < numIndices; vI += 3)
	{
		const physx::PxVec3& vI0 = verts[index[vI]];
		const physx::PxVec3& vI1 = verts[index[vI + 1]];
		const physx::PxVec3& vI2 = verts[index[vI + 2]];
		const physx::PxVec3 eI1 = vI1 - vI0;
		const physx::PxVec3 eI2 = vI2 - vI0;
		physx::PxVec3 normalI = eI1.cross(eI2);
		mParams->volume += (normalI.dot(vI0 - center));
		if (normalI.magnitudeSquared() > 0.0f)
		{
			normalI.normalize();
			physx::PxU32 j;
			physx::PxU32 oppositeFace = 0;
			for (j = 0; j < uniquePlanes.size(); ++j)
			{
				const physx::PxF32 cosTheta = normalI.dot(uniquePlanes[j].n);
				oppositeFace = (physx::PxU32)(cosTheta < 0.0f);
				if (cosTheta * cosTheta > 0.999999f)
				{
					break;
				}
			}
			if (j == uniquePlanes.size())
			{
				uniquePlanes.pushBack(physx::PxPlane(vI0, normalI));
				widths.pushBack(PX_MAX_F32);
				oppositeFace = 0;
			}
			else if (oppositeFace && widths[j] == PX_MAX_F32)
			{
				// New slab
				widths[j] = -uniquePlanes[j].distance(vI0);
			}
			fEdges.pushBack(EdgeWithFace(index[vI], index[vI + 1], j, oppositeFace, true));
			fEdges.pushBack(EdgeWithFace(index[vI + 1], index[vI + 2], j, oppositeFace, true));
			fEdges.pushBack(EdgeWithFace(index[vI + 2], index[vI], j, oppositeFace, true));
		}
	}

	// Create a map from current plane indices to the arrangement we'll have after the following loop
	// This map will be its inverse
	physx::Array<physx::PxU32> invMap;
	invMap.resize(2*widths.size());
	for (physx::PxU32 i = 0; i < invMap.size(); ++i)
	{
		invMap[i] = i;
	}

	// This ensures that all the slabs are represented first in the planes list
	physx::PxU32 slabCount = widths.size();
	for (physx::PxU32 i = widths.size(); i--;)
	{
		if (widths[i] == PX_MAX_F32)
		{
			--slabCount;
			physx::swap(widths[i], widths[slabCount]);
			physx::swap(uniquePlanes[i], uniquePlanes[slabCount]);
			physx::swap(invMap[i], invMap[slabCount]);
		}
	}

	// Now invert invMap to get the map
	physx::Array<physx::PxU32> map;
	map.resize(invMap.size());
	for (physx::PxU32 i = 0; i < map.size(); ++i)
	{
		map[invMap[i]] = i;
	}

	// Plane indices have been rearranged, need to make corresponding rearrangements to fEdges' faceIndex value.
	for (physx::PxU32 i = 0; i < fEdges.size(); ++i)
	{
		fEdges[i].faceIndex = map[fEdges[i].faceIndex];
		if (fEdges[i].opposite)
		{
			fEdges[i].faceIndex += uniquePlanes.size();
		}
	}

	// Total plane count, with non-unique (back-facing) normals
	mParams->planeCount = uniquePlanes.size() + slabCount;

	// Fill in remaining widths
	for (physx::PxU32 pI = mParams->planeCount - uniquePlanes.size(); pI < widths.size(); ++pI)
	{
		widths[pI] = 0.0f;
		for (physx::PxU32 vI = 0; vI < numIndices; ++vI)
		{
			const physx::PxF32 vDepth = -uniquePlanes[pI].distance(verts[index[vI]]);
			if (vDepth > widths[pI])
			{
				widths[pI] = vDepth;
			}
		}
	}

	// Record faceIndex pairs in adjacentFaces
	physx::Array<physx::PxU32> adjacentFaces;

	// Eliminate redundant edges
	qsort(fEdges.begin(), fEdges.size(), sizeof(EdgeWithFace), compareEdgesWithFaces);
	for (physx::PxU32 eI = 0; eI < fEdges.size();)
	{
		physx::PxU32 i0 = fEdges[eI].v0Index;
		physx::PxU32 i1 = fEdges[eI].v1Index;
		PX_ASSERT(i0 < 65536 && i1 < 65536);
		if (eI < fEdges.size() - 1 && fEdges[eI] == fEdges[eI + 1])
		{
			if (fEdges[eI].faceIndex != fEdges[eI + 1].faceIndex)
			{
				edges.pushBack(i0 << 16 | i1);
				adjacentFaces.pushBack(fEdges[eI].faceIndex << 16 | fEdges[eI + 1].faceIndex);
			}
			eI += 2;
		}
		else
		{
			edges.pushBack(i0 << 16 | i1);
			adjacentFaces.pushBack(0xFFFF0000 | fEdges[eI].faceIndex);
			++eI;
		}
	}

	// Find unique edge directions, put them at the front of the edge list
	const physx::PxU32 edgeCount = edges.size();
	if (edgeCount > 0)
	{
		mParams->uniqueEdgeDirectionCount = 1;
		for (physx::PxU32 testEdgeIndex = 1; testEdgeIndex < edgeCount; ++testEdgeIndex)
		{
			const physx::PxU32 testEdgeEndpointIndices = edges[testEdgeIndex];
			const physx::PxVec3 testEdge = mParams->vertices.buf[testEdgeEndpointIndices & 0xFFFF] - mParams->vertices.buf[testEdgeEndpointIndices >> 16];
			const physx::PxF32 testEdge2 = testEdge.magnitudeSquared();
			physx::PxU32 uniqueEdgeIndex = 0;
			for (; uniqueEdgeIndex < mParams->uniqueEdgeDirectionCount; ++uniqueEdgeIndex)
			{
				const physx::PxU32 uniqueEdgeEndpointIndices = edges[uniqueEdgeIndex];
				const physx::PxVec3 uniqueEdge = mParams->vertices.buf[uniqueEdgeEndpointIndices & 0xFFFF] - mParams->vertices.buf[uniqueEdgeEndpointIndices >> 16];
				if (uniqueEdge.cross(testEdge).magnitudeSquared() < testEdge2 * uniqueEdge.magnitudeSquared()*PX_EPS_F32 * PX_EPS_F32)
				{
					break;
				}
			}
			if (uniqueEdgeIndex == mParams->uniqueEdgeDirectionCount)
			{
				physx::swap(edges[mParams->uniqueEdgeDirectionCount], edges[testEdgeIndex]);
				physx::swap(adjacentFaces[mParams->uniqueEdgeDirectionCount], adjacentFaces[testEdgeIndex]);
				++mParams->uniqueEdgeDirectionCount;
			}
		}
	}

	mParams->volume *= 0.1666666667f;

	// Transfer from temporary arrays into mParams

	// uniquePlanes
	mParams->getParameterHandle("uniquePlanes", handle);
	mParams->resizeArray(handle, uniquePlanes.size());
	for (physx::PxU32 i = 0; i < uniquePlanes.size(); ++i)
	{
		physx::PxPlane& plane = uniquePlanes[i];
		ConvexHullParametersNS::Plane_Type& paramPlane = mParams->uniquePlanes.buf[i];
		paramPlane.normal = plane.n;
		paramPlane.d = plane.d;
	}

	// edges
	mParams->getParameterHandle("edges", handle);
	mParams->resizeArray(handle, edges.size());
	mParams->setParamU32Array(handle, edges.begin(), edges.size());

	// adjacentFaces
	mParams->getParameterHandle("adjacentFaces", handle);
	mParams->resizeArray(handle, adjacentFaces.size());
	mParams->setParamU32Array(handle, adjacentFaces.begin(), adjacentFaces.size());

	// widths
	mParams->getParameterHandle("widths", handle);
	mParams->resizeArray(handle, widths.size());
	mParams->setParamF32Array(handle, widths.begin(), widths.size());
}
#elif NX_SDK_VERSION_MAJOR == 3


void ConvexHull::buildFromConvexMesh(const ConvexMesh* mesh)
{
	setEmpty();

	if (!mesh)
	{
		return;
	}

	const physx::PxU32 numVerts = mesh->getNbVertices();
	const physx::PxVec3* verts = (const physx::PxVec3*)mesh->getVertices();
	const physx::PxU32 numPolygons = mesh->getNbPolygons();
	const physx::PxU8* index = (const physx::PxU8*)mesh->getIndexBuffer();


	Array<physx::PxPlane>	uniquePlanes;
	Array<PxU32>	edges;
	Array<PxF32>	widths;

	NxParameterized::Handle handle(*mParams);

	// Copy vertices and compute bounds
	mParams->getParameterHandle("vertices", handle);
	mParams->resizeArray(handle, (physx::PxI32)numVerts);
	for (physx::PxU32 i = 0; i < numVerts; ++i)
	{
		mParams->vertices.buf[i] = verts[i];
		mParams->bounds.include(verts[i]);
	}

	physx::PxVec3 center;
	center = mParams->bounds.getCenter();

	PxMat33 localInertia;
	PxVec3 localCenterOfMass;
	mesh->getMassInformation(mParams->volume, localInertia, localCenterOfMass);

	// Find planes and compute volume
	physx::Array<EdgeWithFace> fEdges;

	for (PxU32 i = 0; i < numPolygons; i++)
	{
		PxHullPolygon data;
		bool status = mesh->getPolygonData(i, data);
		PX_ASSERT(status);
		if (!status)
		{
			continue;
		}

		const physx::PxU32 numIndices = data.mNbVerts;
		for (physx::PxU32 vI = 0; vI < numIndices; ++vI)
		{
			const physx::PxU16 i0 = (physx::PxU16)index[data.mIndexBase + vI];
			const physx::PxU16 i1 = (physx::PxU16)index[data.mIndexBase + ((vI+1)%numIndices)];
			const physx::PxVec3& vI0 = verts[i0];
			const physx::PxVec3 normal(data.mPlane[0], data.mPlane[1], data.mPlane[2]);
			PX_ASSERT(physx::PxEquals(normal.magnitudeSquared(), 1.0f, 0.000001f));
			physx::PxU32 faceIndex;
			physx::PxU32 oppositeFace = 0;
			for (faceIndex = 0; faceIndex < uniquePlanes.size(); ++faceIndex)
			{
				const physx::PxF32 cosTheta = normal.dot(uniquePlanes[faceIndex].n);
				oppositeFace = (physx::PxU32)(cosTheta < 0.0f);
				if (cosTheta * cosTheta > 0.999999f)
				{
					break;
				}
			}
			if (faceIndex == uniquePlanes.size())
			{
				uniquePlanes.pushBack(physx::PxPlane(vI0, normal));
				widths.pushBack(PX_MAX_F32);
				oppositeFace = 0;
			}
			else if (oppositeFace && widths[faceIndex] == PX_MAX_F32)
			{
				// New slab
				widths[faceIndex] = -uniquePlanes[faceIndex].distance(vI0);
			}
			fEdges.pushBack(EdgeWithFace(i0, i1, faceIndex, oppositeFace, true));
		}
	}

	// Create a map from current plane indices to the arrangement we'll have after the following loop
	// This map will be its inverse
	physx::Array<physx::PxU32> invMap;
	invMap.resize(2*widths.size());
	for (physx::PxU32 i = 0; i < invMap.size(); ++i)
	{
		invMap[i] = i;
	}

	// This ensures that all the slabs are represented first in the planes list
	physx::PxU32 slabCount = widths.size();
	for (physx::PxU32 i = widths.size(); i--;)
	{
		if (widths[i] == PX_MAX_F32)
		{
			--slabCount;
			physx::swap(widths[i], widths[slabCount]);
			physx::swap(uniquePlanes[i], uniquePlanes[slabCount]);
			physx::swap(invMap[i], invMap[slabCount]);
		}
	}

	// Now invert invMap to get the map
	physx::Array<physx::PxU32> map;
	map.resize(invMap.size());
	for (physx::PxU32 i = 0; i < map.size(); ++i)
	{
		map[invMap[i]] = i;
	}

	// Plane indices have been rearranged, need to make corresponding rearrangements to fEdges' faceIndex value.
	for (physx::PxU32 i = 0; i < fEdges.size(); ++i)
	{
		fEdges[i].faceIndex = map[fEdges[i].faceIndex];
		if (fEdges[i].opposite)
		{
			fEdges[i].faceIndex += uniquePlanes.size();
		}
	}

	// Total plane count, with non-unique (back-facing) normals
	mParams->planeCount = uniquePlanes.size() + slabCount;

	// Fill in remaining widths
	for (physx::PxU32 pI = mParams->planeCount - uniquePlanes.size(); pI < widths.size(); ++pI)
	{
		widths[pI] = 0.0f;
		for (PxU32 i = 0; i < numPolygons; i++)
		{
			PxHullPolygon data;
			bool status = mesh->getPolygonData(i, data);
			PX_ASSERT(status);
			if (!status)
			{
				continue;
			}

			const physx::PxU32 numIndices = (physx::PxU32)data.mNbVerts - 2;
			for (physx::PxU32 vI = 0; vI < numIndices; ++vI)
			{
				const physx::PxF32 vDepth = -uniquePlanes[pI].distance(verts[index[data.mIndexBase + vI]]);
				if (vDepth > widths[pI])
				{
					widths[pI] = vDepth;
				}
			}
		}
	}

	// Record faceIndex pairs in adjacentFaces
	physx::Array<physx::PxU32> adjacentFaces;

	// Eliminate redundant edges
	qsort(fEdges.begin(), fEdges.size(), sizeof(EdgeWithFace), compareEdgesWithFaces);
	for (physx::PxU32 eI = 0; eI < fEdges.size();)
	{
		physx::PxU32 i0 = fEdges[eI].v0Index;
		physx::PxU32 i1 = fEdges[eI].v1Index;
		PX_ASSERT(i0 < 65536 && i1 < 65536);
		if (eI < fEdges.size() - 1 && fEdges[eI] == fEdges[eI + 1])
		{
			if (fEdges[eI].faceIndex != fEdges[eI + 1].faceIndex)
			{
				edges.pushBack(i0 << 16 | i1);
				adjacentFaces.pushBack(fEdges[eI].faceIndex << 16 | fEdges[eI + 1].faceIndex);
			}
			eI += 2;
		}
		else
		{
			edges.pushBack(i0 << 16 | i1);
			adjacentFaces.pushBack(0xFFFF0000 | fEdges[eI].faceIndex);
			++eI;
		}
	}

	// Find unique edge directions, put them at the front of the edge list
	const physx::PxU32 edgeCount = edges.size();
	if (edgeCount > 0)
	{
		mParams->uniqueEdgeDirectionCount = 1;
		for (physx::PxU32 testEdgeIndex = 1; testEdgeIndex < edgeCount; ++testEdgeIndex)
		{
			const physx::PxVec3 testEdge = verts[edges[testEdgeIndex] & 0xFFFF] - verts[edges[testEdgeIndex] >> 16];
			const physx::PxF32 testEdge2 = testEdge.magnitudeSquared();
			physx::PxU32 uniqueEdgeIndex = 0;
			for (; uniqueEdgeIndex < mParams->uniqueEdgeDirectionCount; ++uniqueEdgeIndex)
			{
				const physx::PxVec3 uniqueEdge = verts[edges[uniqueEdgeIndex] & 0xFFFF] - verts[edges[uniqueEdgeIndex] >> 16];
				if (uniqueEdge.cross(testEdge).magnitudeSquared() < testEdge2 * uniqueEdge.magnitudeSquared()*PX_EPS_F32 * PX_EPS_F32)
				{
					break;
				}
			}
			if (uniqueEdgeIndex == mParams->uniqueEdgeDirectionCount)
			{
				physx::swap(edges[mParams->uniqueEdgeDirectionCount], edges[testEdgeIndex]);
				physx::swap(adjacentFaces[mParams->uniqueEdgeDirectionCount], adjacentFaces[testEdgeIndex]);
				++mParams->uniqueEdgeDirectionCount;
			}
		}
	}

	// Transfer from temporary arrays into mParams

	// uniquePlanes
	mParams->getParameterHandle("uniquePlanes", handle);
	mParams->resizeArray(handle, (physx::PxI32)uniquePlanes.size());
	for (physx::PxU32 i = 0; i < uniquePlanes.size(); ++i)
	{
		physx::PxPlane& plane = uniquePlanes[i];
		ConvexHullParametersNS::Plane_Type& paramPlane = mParams->uniquePlanes.buf[i];
		paramPlane.normal = plane.n;
		paramPlane.d = plane.d;
	}

	// edges
	mParams->getParameterHandle("edges", handle);
	mParams->resizeArray(handle, (physx::PxI32)edges.size());
	mParams->setParamU32Array(handle, edges.begin(), (physx::PxI32)edges.size());

	// adjacentFaces
	mParams->getParameterHandle("adjacentFaces", handle);
	mParams->resizeArray(handle, (physx::PxI32)adjacentFaces.size());
	mParams->setParamU32Array(handle, adjacentFaces.begin(), (physx::PxI32)adjacentFaces.size());

	// widths
	mParams->getParameterHandle("widths", handle);
	mParams->resizeArray(handle, (physx::PxI32)widths.size());
	mParams->setParamF32Array(handle, widths.begin(), (physx::PxI32)widths.size());
}
#endif



void ConvexHull::buildFromAABB(const physx::PxBounds3& aabb)
{
	physx::PxVec3 center, extent;
	center = aabb.getCenter();
	extent = aabb.getExtents();

	NxParameterized::Handle handle(*mParams);

	// Copy vertices and compute bounds
	mParams->getParameterHandle("vertices", handle);
	mParams->resizeArray(handle, 8);
	for (int i = 0; i < 8; ++i)
	{
		mParams->vertices.buf[i] = center + physx::PxVec3((2.0f * (i & 1) - 1.0f) * extent.x, ((physx::PxF32)(i & 2) - 1.0f) * extent.y, (0.5f * (i & 4) - 1.0f) * extent.z);
	}

	mParams->getParameterHandle("uniquePlanes", handle);
	mParams->resizeArray(handle, 3);
	mParams->getParameterHandle("widths", handle);
	mParams->resizeArray(handle, 3);
	for (int i = 0; i < 3; ++i)
	{
		ConvexHullParametersNS::Plane_Type& plane = mParams->uniquePlanes.buf[i];
		plane.normal = physx::PxVec3(0.0f);
		plane.normal[i] = -1.0f;
		plane.d = -aabb.maximum[i];
		mParams->widths.buf[i] = aabb.maximum[i] - aabb.minimum[i];
	}
	mParams->planeCount = 6;

	mParams->getParameterHandle("edges", handle);
	mParams->resizeArray(handle, 12);
	mParams->getParameterHandle("adjacentFaces", handle);
	mParams->resizeArray(handle, 12);
	for (physx::PxU32 i = 0; i < 3; ++i)
	{
		physx::PxU32 i1 = ((i + 1) % 3);
		physx::PxU32 i2 = ((i + 2) % 3);
		physx::PxU32 vOffset1 = 1u << i1;
		physx::PxU32 vOffset2 = 1u << i2;
		for (int j = 0; j < 4; ++j)
		{
			physx::PxU32 v0 = 0;
			physx::PxU32 v1 = 1u << i;
			physx::PxU32 f0 = i1;
			physx::PxU32 f1 = i2;
			if (j & 1)
			{
				v0 += vOffset1;
				v1 += vOffset1;
				f0 += 3;
			}
			if (j & 2)
			{
				v0 += vOffset2;
				v1 += vOffset2;
				f1 += 3;
			}
			if (j == 1 || j == 2)
			{
				physx::swap(f0, f1);
			}
			physx::PxU32 index = i + 3 * j;
			mParams->edges.buf[index] = v0 << 16 | v1;
			mParams->adjacentFaces.buf[i] = f0 << 16 | f1;
		}
	}
	mParams->uniqueEdgeDirectionCount = 3;

	mParams->bounds = aabb;

	const physx::PxVec3 diag = mParams->bounds.maximum - mParams->bounds.minimum;
	mParams->volume = diag.x * diag.y * diag.z;
}

void ConvexHull::buildKDOP(const void* points, physx::PxU32 numPoints, physx::PxU32 pointStrideBytes, const physx::PxVec3* directions, physx::PxU32 numDirections)
{
	setEmpty();

	if (numPoints == 0)
	{
		return;
	}

	physx::PxF32 size = 0;

	physx::Array<physx::PxPlane> planes;
	planes.reserve(2 * numDirections);
	for (physx::PxU32 i = 0; i < numDirections; ++i)
	{
		physx::PxVec3 dir = directions[i];
		dir.normalize();
		physx::PxF32 min, max;
		getExtent(min, max, points, numPoints, pointStrideBytes, dir);
		planes.pushBack(physx::PxPlane(dir, -max));
		planes.pushBack(physx::PxPlane(-dir, min));
		size = physx::PxMax(size, max - min);
	}

	buildFromPlanes(planes.begin(), planes.size(), 0.00001f * size);
}

void ConvexHull::intersectPlaneSide(const physx::PxPlane& plane)
{
	const physx::PxF32 size = (mParams->bounds.maximum - mParams->bounds.minimum).magnitude();
	const physx::PxF32 eps = 0.00001f * size;

	const physx::PxU32 oldVertexCount = (physx::PxU32)mParams->vertices.arraySizes[0];

	Array<PxVec3> vertices;
	vertices.resize(oldVertexCount);
	NxParameterized::Handle handle(*mParams);
	mParams->getParameterHandle("vertices", handle);
	mParams->getParamVec3Array(handle, vertices.begin(), (physx::PxI32)vertices.size());

	// Throw out all vertices on the + side of the plane
	for (physx::PxU32 i = oldVertexCount; i--;)
	{
		if (plane.distance(vertices[i]) > eps)
		{
			vertices.replaceWithLast(i);
		}
	}

	if (vertices.size() == 0)
	{
		// plane excludes this whole hull.  Delete everything.
		setEmpty();
		return;
	}

	if (vertices.size() == oldVertexCount)
	{
		// plane includes this whole hull.  Do nothing.
		return;
	}

	// Intersect new plane with all pairs of old planes.
	const physx::PxU32 planeCount = getPlaneCount();
	for (physx::PxU32 j = 1; j < planeCount; ++j)
	{
		const physx::PxPlane planeJ = getPlane(j);
		for (physx::PxU32 i = 0; i < j; ++i)
		{
			const physx::PxPlane planeI = getPlane(i);
			physx::PxVec3 point;
			if (intersectPlanes(point, plane, planeI, planeJ))
			{
				// See if point is excluded by any of the other planes
				physx::PxU32 k;
				for (k = 0; k < planeCount; ++k)
				{
					if (k != i && k != j && getPlane(k).distance(point) > eps)
					{
						break;
					}
				}
				if (k == planeCount)
				{
					// Point is part of the new intersected hull
					physx::PxU32 m = 0;
					for (; m < vertices.size(); ++m)
					{
						if ((point - vertices[m]).magnitudeSquared() < eps)
						{
							break;
						}
					}
					if (m == vertices.size())
					{
						vertices.pushBack(point);
					}
				}
			}
		}
	}

	buildFromPoints(vertices.begin(), vertices.size(), sizeof(vertices[0]));
}

void ConvexHull::intersectHull(const ConvexHull& hull)
{
	if (hull.getPlaneCount() == 0)
	{
		setEmpty();
		return;
	}

	physx::Array<physx::PxPlane> planes;
	planes.reserve(getPlaneCount() + hull.getPlaneCount());
	for (physx::PxU32 planeN = 0; planeN < getPlaneCount(); ++planeN)
	{
		planes.pushBack(getPlane(planeN));
	}
	for (physx::PxU32 planeN = 0; planeN < hull.getPlaneCount(); ++planeN)
	{
		planes.pushBack(hull.getPlane(planeN));
	}

	const physx::PxF32 size = (mParams->bounds.maximum - mParams->bounds.minimum).magnitude();

	buildFromPlanes(planes.begin(), planes.size(), 0.00001f * size);
}

// Returns true iff distance does not exceed result and maxDistance
PX_INLINE bool updateResultAndSeparation(bool& updated, bool& normalPointsFrom0to1, physx::PxF32& result, ConvexHull::Separation* separation,
										 physx::PxF32 min0, physx::PxF32 max0, physx::PxF32 min1, physx::PxF32 max1, const physx::PxVec3& n, physx::PxF32 maxDistance)
{
	physx::PxF32 midpoint;
	const physx::PxF32 dist = extentDistanceAndNormalDirection(min0, max0, min1, max1, midpoint, normalPointsFrom0to1);
	updated = dist > result;
	if (updated)
	{
		if (dist > maxDistance)
		{
			return false;
		}
		result = dist;
		if (separation != NULL)
		{
			if (normalPointsFrom0to1)
			{
				separation->plane = physx::PxPlane(n, -midpoint);
				separation->min0 = min0;
				separation->max0 = max0;
				separation->min1 = min1;
				separation->max1 = max1;
			}
			else
			{
				separation->plane = physx::PxPlane(-n, midpoint);
				separation->min0 = -max0;
				separation->max0 = -min0;
				separation->min1 = -max1;
				separation->max1 = -min1;
			}
		}
	}
	return true;
}

bool ConvexHull::hullsInProximity(const ConvexHull& hull0, const physx::PxMat34Legacy& localToWorldRT0, const physx::PxVec3& scale0,
                                  const ConvexHull& hull1, const physx::PxMat34Legacy& localToWorldRT1, const physx::PxVec3& scale1,
                                  physx::PxF32 maxDistance, Separation* separation)
{
	physx::PxF32 result = -PX_MAX_F32;

	// Transform hull0
	const physx::PxU32 numSlabs0 = hull0.getUniquePlaneNormalCount();
	const physx::PxU32 numVerts0 = hull0.getVertexCount();
	physx::PxPlane* planes0 = (physx::PxPlane*)PxAlloca(numSlabs0 * sizeof(physx::PxPlane));
	physx::PxVec3* verts0 = (physx::PxVec3*)PxAlloca(numVerts0 * sizeof(physx::PxVec3));
	physx::PxF32* widths0 = (physx::PxF32*)PxAlloca(numSlabs0 * sizeof(physx::PxF32));
	const NiCof44 cof0(localToWorldRT0, scale0);
	const physx::PxF32 det0 = scale0.x*scale0.y*scale0.z;
	physx::PxMat34Legacy srt0 = localToWorldRT0;
	srt0.M.multiplyDiagonal(scale0);

	for (physx::PxU32 slab0Index = 0; slab0Index < numSlabs0; ++slab0Index)
	{
		physx::PxPlane& plane = planes0[slab0Index];
		ConvexHullParametersNS::Plane_Type& paramPlane = hull0.mParams->uniquePlanes.buf[slab0Index];
		cof0.transform(physx::PxPlane(paramPlane.normal, paramPlane.d), plane);
		const physx::PxF32 recipLen = physx::recipSqrtFast(plane.n.magnitudeSquared());
		plane.n *= recipLen;
		plane.d *= recipLen;
		widths0[slab0Index] = hull0.mParams->widths.buf[slab0Index] * recipLen * det0;
	}

	for (physx::PxU32 vert0Index = 0; vert0Index < numVerts0; ++vert0Index)
	{
		srt0.multiply(hull0.mParams->vertices.buf[vert0Index], verts0[vert0Index]);
	}

	// Transform hull1
	const physx::PxU32 numSlabs1 = hull1.getUniquePlaneNormalCount();
	const physx::PxU32 numVerts1 = hull1.getVertexCount();
	physx::PxPlane* planes1 = (physx::PxPlane*)PxAlloca(numSlabs1 * sizeof(physx::PxPlane));
	physx::PxVec3* verts1 = (physx::PxVec3*)PxAlloca(numVerts1 * sizeof(physx::PxVec3));
	physx::PxF32* widths1 = (physx::PxF32*)PxAlloca(numSlabs1 * sizeof(physx::PxF32));
	const NiCof44 cof1(localToWorldRT1, scale1);
	const physx::PxF32 det1 = scale1.x*scale1.y*scale1.z;
	physx::PxMat34Legacy srt1 = localToWorldRT1;
	srt1.M.multiplyDiagonal(scale1);

	for (physx::PxU32 slab1Index = 0; slab1Index < numSlabs1; ++slab1Index)
	{
		physx::PxPlane& plane = planes1[slab1Index];
		ConvexHullParametersNS::Plane_Type& paramPlane = hull1.mParams->uniquePlanes.buf[slab1Index];
		cof1.transform(physx::PxPlane(paramPlane.normal, paramPlane.d), plane);
		const physx::PxF32 recipLen = physx::recipSqrtFast(plane.n.magnitudeSquared());
		plane.n *= recipLen;
		plane.d *= recipLen;
		widths1[slab1Index] = hull1.mParams->widths.buf[slab1Index] * recipLen * det1;
	}

	for (physx::PxU32 vert1Index = 0; vert1Index < numVerts1; ++vert1Index)
	{
		srt1.multiply(hull1.mParams->vertices.buf[vert1Index], verts1[vert1Index]);
	}

	// Test hull1 against slabs of hull0
	for (physx::PxU32 slab0Index = 0; slab0Index < numSlabs0; ++slab0Index)
	{
		const physx::PxPlane& plane = planes0[slab0Index];
		const physx::PxVec3& n = plane.n;
		physx::PxF32 min1, max1;
		getExtent(min1, max1, verts1, numVerts1, sizeof(physx::PxVec3), n);
		const physx::PxF32 min0 = -plane.d - widths0[slab0Index];
		const physx::PxF32 max0 = -plane.d;
		bool updated;
		bool normalPointsFrom0to1;
		if (!updateResultAndSeparation(updated, normalPointsFrom0to1, result, separation, min0, max0, min1, max1, n, maxDistance))
		{
			return false;
		}
	}

	// Test hull0 against slabs of hull1
	for (physx::PxU32 slab1Index = 0; slab1Index < numSlabs1; ++slab1Index)
	{
		const physx::PxPlane& plane = planes1[slab1Index];
		const physx::PxVec3 n = -plane.n;
		physx::PxF32 min0, max0;
		getExtent(min0, max0, verts0, numVerts0, sizeof(physx::PxVec3), n);
		const physx::PxF32 min1 = plane.d;
		const physx::PxF32 max1 = plane.d + widths1[slab1Index];
		bool updated;
		bool normalPointsFrom0to1;
		if (!updateResultAndSeparation(updated, normalPointsFrom0to1, result, separation, min0, max0, min1, max1, n, maxDistance))
		{
			return false;
		}
	}

	// Test hulls against cross-edge planes
	const physx::PxU32 numEdges0 = hull0.getUniqueEdgeDirectionCount();
	const physx::PxU32 numEdges1 = hull1.getUniqueEdgeDirectionCount();
	for (physx::PxU32 edge0Index = 0; edge0Index < numEdges0; ++edge0Index)
	{
		const physx::PxVec3 edge0 = hull0.getEdgeDirection(edge0Index);
		for (physx::PxU32 edge1Index = 0; edge1Index < numEdges1; ++edge1Index)
		{
			const physx::PxVec3 edge1 = hull1.getEdgeDirection(edge1Index);
			physx::PxVec3 n = edge0.cross(edge1);
			const physx::PxF32 n2 = n.magnitudeSquared();
			if (n2 < PX_EPS_F32 * PX_EPS_F32)
			{
				continue;
			}
			n *= physx::recipSqrtFast(n2);
			physx::PxF32 min0, max0, min1, max1;
			getExtent(min0, max0, verts0, numVerts0, sizeof(physx::PxVec3), n);
			getExtent(min1, max1, verts1, numVerts1, sizeof(physx::PxVec3), n);
			bool updated;
			bool normalPointsFrom0to1;
			if (!updateResultAndSeparation(updated, normalPointsFrom0to1, result, separation, min0, max0, min1, max1, n, maxDistance))
			{
				return false;
			}
		}
	}

	return true;
}

bool ConvexHull::sphereInProximity(const physx::PxMat34Legacy& hullLocalToWorldRT, const physx::PxVec3& hullScale,
								   const physx::PxVec3& sphereWorldCenter, physx::PxF32 sphereRadius,
								   physx::PxF32 maxDistance, Separation* separation)
{
	physx::PxF32 result = -PX_MAX_F32;

	// Transform hull
	const physx::PxU32 numSlabs = getUniquePlaneNormalCount();
	const physx::PxU32 numVerts = getVertexCount();
	physx::PxPlane* planes = (physx::PxPlane*)PxAlloca(numSlabs * sizeof(physx::PxPlane));
	physx::PxVec3* verts = (physx::PxVec3*)PxAlloca(numVerts * sizeof(physx::PxVec3));
	physx::PxF32* widths = (physx::PxF32*)PxAlloca(numSlabs * sizeof(physx::PxF32));
	const NiCof44 cof(hullLocalToWorldRT, hullScale);
	const physx::PxF32 det = hullScale.x*hullScale.y*hullScale.z;
	physx::PxMat34Legacy srt = hullLocalToWorldRT;
	srt.M.multiplyDiagonal(hullScale);

	for (physx::PxU32 slabIndex = 0; slabIndex < numSlabs; ++slabIndex)
	{
		physx::PxPlane& plane = planes[slabIndex];
		ConvexHullParametersNS::Plane_Type& paramPlane = mParams->uniquePlanes.buf[slabIndex];
		cof.transform(physx::PxPlane(paramPlane.normal, paramPlane.d), plane);
		const physx::PxF32 recipLen = physx::recipSqrtFast(plane.n.magnitudeSquared());
		plane.n *= recipLen;
		plane.d *= recipLen;
		widths[slabIndex] = mParams->widths.buf[slabIndex] * recipLen * det;
	}

	for (physx::PxU32 vertIndex = 0; vertIndex < numVerts; ++vertIndex)
	{
		srt.multiply(mParams->vertices.buf[vertIndex], verts[vertIndex]);
	}

	// Test sphere against slabs of hull
	const physx::PxU32 unpairedFaceCount = getPlaneCount() - numSlabs;	// number of faces with no exactly opposite face
	physx::PxU32 leastIntersectingFace = 0xFFFFFFFF;	// Keep track of this for later tests
	for (physx::PxU32 slabIndex = 0; slabIndex < numSlabs; ++slabIndex)
	{
		const physx::PxPlane& plane = planes[slabIndex];
		const physx::PxVec3& n = plane.n;
		const physx::PxF32 centerDisp = sphereWorldCenter.dot(n);
		const physx::PxF32 min1 = centerDisp - sphereRadius;
		const physx::PxF32 max1 = centerDisp + sphereRadius;
		const physx::PxF32 min0 = -plane.d - widths[slabIndex];
		const physx::PxF32 max0 = -plane.d;
		bool updated;
		bool normalPointsFrom0to1;
		if (!updateResultAndSeparation(updated, normalPointsFrom0to1, result, separation, min0, max0, min1, max1, n, maxDistance))
		{
			return false;
		}
		if (updated)
		{
			if (normalPointsFrom0to1)
			{
				leastIntersectingFace = slabIndex;
			}
			else
			if (slabIndex < unpairedFaceCount)
			{
				leastIntersectingFace = slabIndex + numSlabs;	// opposite face
			}
		}
	}

	if (leastIntersectingFace >= getPlaneCount() || sphereRadius <= 0.0f)
	{
		return true;
	}

	// Test least intersecting face edges against sphere
	const physx::PxU32 edgeCount = getEdgeCount();
	physx::PxU32 leastIntersectingEdge = 0xFFFFFFFF;	// Keep track of this for later tests
	for (physx::PxU32 edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex)
	{
		if (getEdgeAdjacentFaceIndex(edgeIndex, 0) != leastIntersectingFace && getEdgeAdjacentFaceIndex(edgeIndex, 1) != leastIntersectingFace)
		{
			continue;	// Not an edge on the face of interest
		}
		const physx::PxVec3 edgeOrig = getVertex(getEdgeEndpointIndex(edgeIndex, 0));
		const physx::PxVec3 edgeDisp = getEdgeDirection(edgeIndex);
		const physx::PxF32 e2 = edgeDisp.magnitudeSquared();
		if (e2 == 0.0f)
		{
			continue;
		}
		physx::PxVec3 n = sphereWorldCenter - edgeOrig;
		n -= (n.dot(edgeDisp)/e2)*edgeDisp;	// Normal from edge line to center of sphere
		if (n.normalize() == 0.0f)
		{
			continue;
		}
		physx::PxF32 min0, max0;
		getExtent(min0, max0, verts, numVerts, sizeof(physx::PxVec3), n);
		const physx::PxF32 centerDisp = sphereWorldCenter.dot(n);
		const physx::PxF32 min1 = centerDisp - sphereRadius;
		const physx::PxF32 max1 = centerDisp + sphereRadius;
		bool updated;
		bool normalPointsFrom0to1;
		if (!updateResultAndSeparation(updated, normalPointsFrom0to1, result, separation, min0, max0, min1, max1, n, maxDistance))
		{
			return false;
		}
		if (updated)
		{
			leastIntersectingEdge = edgeIndex;
		}
	}

	if (leastIntersectingEdge >= getEdgeCount())
	{
		return true;
	}

	// Test least intersecting edge endpoints against sphere
	for (physx::PxU32 endpointIndex = 0; endpointIndex < 2; ++endpointIndex)
	{
		const physx::PxVec3 vert = getVertex(getEdgeEndpointIndex(leastIntersectingEdge, endpointIndex));
		const physx::PxVec3 n = (sphereWorldCenter - vert).getNormalized();
		physx::PxF32 min0, max0;
		getExtent(min0, max0, verts, numVerts, sizeof(physx::PxVec3), n);
		const physx::PxF32 centerDisp = sphereWorldCenter.dot(n);
		const physx::PxF32 min1 = centerDisp - sphereRadius;
		const physx::PxF32 max1 = centerDisp + sphereRadius;
		bool updated;
		bool normalPointsFrom0to1;
		if (!updateResultAndSeparation(updated, normalPointsFrom0to1, result, separation, min0, max0, min1, max1, n, maxDistance))
		{
			return false;
		}
	}

	return true;
}

#if NX_SDK_VERSION_MAJOR == 2
bool ConvexHull::intersects(const NxShape& shape, const physx::PxMat34Legacy& localToWorldRT, const physx::PxVec3& scale, physx::PxF32 padding) const
{
	PX_UNUSED(localToWorldRT);
	PX_UNUSED(scale);
	PX_UNUSED(padding);

	switch (shape.getType())
	{
	case NX_SHAPE_PLANE:	// xxx todo - implement
		return true;
	case NX_SHAPE_SPHERE:	// xxx todo - implement
		return true;
	case NX_SHAPE_BOX:	// xxx todo - implement
		return true;
	case NX_SHAPE_CAPSULE:	// xxx todo - implement
		return true;
	case NX_SHAPE_CONVEX:	// xxx todo - implement
		return true;
	case NX_SHAPE_MESH:	// xxx todo - implement
		return true;
	case NX_SHAPE_HEIGHTFIELD:	// xxx todo - implement
		return true;
	default:
		return false;
	}
}
#elif NX_SDK_VERSION_MAJOR == 3
bool ConvexHull::intersects(const PxShape& shape, const physx::PxMat34Legacy& localToWorldRT, const physx::PxVec3& scale, physx::PxF32 padding) const
{
	PX_UNUSED(localToWorldRT);
	PX_UNUSED(scale);
	PX_UNUSED(padding);

	switch (shape.getGeometryType())
	{
	case PxGeometryType::ePLANE:	// xxx todo - implement
		return true;
	case PxGeometryType::eSPHERE:	// xxx todo - implement
		return true;
	case PxGeometryType::eBOX:	// xxx todo - implement
		return true;
	case PxGeometryType::eCAPSULE:	// xxx todo - implement
		return true;
	case PxGeometryType::eCONVEXMESH:	// xxx todo - implement
		return true;
	case PxGeometryType::eTRIANGLEMESH:	// xxx todo - implement
		return true;
	case PxGeometryType::eHEIGHTFIELD:	// xxx todo - implement
		return true;
	default:
		return false;
	}
}
#endif



bool ConvexHull::rayCast(physx::PxF32& in, physx::PxF32& out, const physx::PxVec3& orig, const physx::PxVec3& dir, const physx::PxMat34Legacy& localToWorldRT, const physx::PxVec3& scale, physx::PxVec3* normal) const
{
	const physx::PxU32 numSlabs = getUniquePlaneNormalCount();

	if (numSlabs == 0)
	{
		return false;
	}

	// Create hull-local ray
	physx::PxVec3 localorig, localdir;
	if (!worldToLocalRay(localorig, localdir, orig, dir, localToWorldRT, scale))
	{
		return false;
	}

	// Intersect with hull - local and world intersect 'times' are equal

	if (normal != NULL)
	{
		*normal = physx::PxVec3(0.0f);	// This will be the value if the ray origin is within the hull
	}

	const physx::PxF32 tol2 = (float)1.0e-14 * localdir.magnitudeSquared();

	for (physx::PxU32 slabN = 0; slabN < numSlabs; ++slabN)
	{
		ConvexHullParametersNS::Plane_Type& paramPlane = mParams->uniquePlanes.buf[slabN];
		const physx::PxPlane plane(paramPlane.normal, paramPlane.d);
		const physx::PxF32 num0 = -plane.distance(localorig);
		const physx::PxF32 num1 = mParams->widths.buf[slabN] - num0;
		const physx::PxF32 den = localdir.dot(plane.n);
		if (den* den <= tol2)	// Needs to be <=, so that localdir = (0,0,0) will act as a point check
		{
			if (num0 < 0 || num1 < 0)
			{
				return false;
			}
		}
		else
		{
			if (den > 0)
			{
				if (num0 < in * den || num1 < out * (-den))
				{
					return false;
				}
				const physx::PxF32 recipDen = 1.0f / den;
				const physx::PxF32 slabIn = -num1 * recipDen;
				if (slabIn > in)
				{
					in = slabIn;
					if (normal != NULL)
					{
						*normal = -plane.n;
					}
				}
				out = physx::PxMin(num0 * recipDen, out);
			}
			else
			{
				if (num0 < out * den || num1 < in * (-den))
				{
					return false;
				}
				const physx::PxF32 recipDen = 1.0f / den;
				const physx::PxF32 slabIn = num0 * recipDen;
				if (slabIn > in)
				{
					in = slabIn;
					if (normal != NULL)
					{
						*normal = plane.n;
					}
				}
				out = physx::PxMin(-num1 * recipDen, out);
			}
		}
	}

	if (normal != NULL)
	{
		NiCof44 cof(localToWorldRT, scale);
		*normal = cof.getBlock33().transform(*normal);
		normal->normalize();
	}

	return true;
}

bool ConvexHull::obbSweep(physx::PxF32& in, physx::PxF32& out, const physx::PxVec3& worldBoxCenter, const physx::PxVec3& worldBoxExtents, const physx::PxVec3 worldBoxAxes[3],
                          const physx::PxVec3& worldDisp, const physx::PxMat34Legacy& localToWorldRT, const physx::PxVec3& scale, physx::PxVec3* normal) const
{
	physx::PxF32 tNormal = -PX_MAX_F32;

	// Leave hull untransformed
	const physx::PxU32 numHullSlabs = getUniquePlaneNormalCount();
	const physx::PxU32 numHullVerts = getVertexCount();
	ConvexHullParametersNS::Plane_Type* paramPlanes = mParams->uniquePlanes.buf;
	const physx::PxVec3* hullVerts = mParams->vertices.buf;
	const physx::PxF32* hullWidths = mParams->widths.buf;

	// Create inverse transform for box and displacement
	const physx::PxF32 detS = scale.x * scale.y * scale.z;
	if (detS == 0.0f)
	{
		return false;	// Not handling singular TMs
	}
	const physx::PxF32 recipDetS = 1.0f / detS;
	const physx::PxVec3 invS(scale.y * scale.z * recipDetS, scale.z * scale.x * recipDetS, scale.x * scale.y * recipDetS);

	// Create box directions and displacement - the box will become a parallelepiped in general.  For brevity we'll still call it a box.
	physx::PxVec3 disp;
	localToWorldRT.M.multiplyByTranspose(worldDisp, disp);
	disp = invS.multiply(disp);

	physx::PxVec3 boxCenter;
	localToWorldRT.multiplyByInverseRT(worldBoxCenter, boxCenter);
	boxCenter = invS.multiply(boxCenter);

	physx::PxVec3 boxAxes[3];	// Will not be orthonormal in general
	for (physx::PxU32 i = 0; i < 3; ++i)
	{
		localToWorldRT.M.multiplyByTranspose(worldBoxAxes[i], boxAxes[i]);
		boxAxes[i] *= worldBoxExtents[i];
		boxAxes[i] = invS.multiply(boxAxes[i]);
	}

	physx::PxVec3 boxFaceNormals[3];
	physx::PxF32 boxRadii[3];
	const physx::PxF32 octantVol = boxAxes[0].dot(boxAxes[1].cross(boxAxes[2]));
	for (physx::PxU32 i = 0; i < 3; ++i)
	{
		boxFaceNormals[i] = boxAxes[(1 << i) & 3].cross(boxAxes[(3 >> i) ^ 1]);
		const physx::PxF32 norm = physx::PxRecipSqrt(boxFaceNormals[i].magnitudeSquared());
		boxFaceNormals[i] *= norm;
		boxRadii[i] = octantVol * norm;
	}

	// Test box against slabs of hull
	for (physx::PxU32 slabIndex = 0; slabIndex < numHullSlabs; ++slabIndex)
	{
		ConvexHullParametersNS::Plane_Type& paramPlane = paramPlanes[slabIndex];
		const physx::PxPlane plane(paramPlane.normal, paramPlane.d);
		const physx::PxF32 projectedRadius = physx::PxAbs(plane.n.dot(boxAxes[0])) + physx::PxAbs(plane.n.dot(boxAxes[1])) + physx::PxAbs(plane.n.dot(boxAxes[2]));
		const physx::PxF32 projectedCenter = plane.n.dot(boxCenter);
		const physx::PxF32 vel0 = disp.dot(plane.n);
		physx::PxF32 tIn, tOut;
		extentOverlapTimeInterval(tIn, tOut, vel0, projectedCenter - projectedRadius, projectedCenter + projectedRadius, -plane.d - hullWidths[slabIndex], -plane.d);
		if (!updateTimeIntervalAndNormal(in, out, tNormal, normal, tIn, tOut, (-PxSign(vel0))*plane.n))
		{
			return false;	// No intersection within the input time interval
		}
	}

	// Test hull against box face directions
	for (physx::PxU32 faceIndex = 0; faceIndex < 3; ++faceIndex)
	{
		const physx::PxVec3& faceNormal = boxFaceNormals[faceIndex];
		physx::PxF32 min, max;
		getExtent(min, max, hullVerts, numHullVerts, sizeof(physx::PxVec3), faceNormal);
		const physx::PxF32 projectedRadius = boxRadii[faceIndex];
		const physx::PxF32 projectedCenter = faceNormal.dot(boxCenter);
		const physx::PxF32 vel0 = disp.dot(faceNormal);
		physx::PxF32 tIn, tOut;
		extentOverlapTimeInterval(tIn, tOut, vel0, projectedCenter - projectedRadius, projectedCenter + projectedRadius, min, max);
		physx::PxVec3 testFace = (-PxSign(vel0)) * faceNormal;
		if (!updateTimeIntervalAndNormal(in, out, tNormal, normal, tIn, tOut, testFace))
		{
			return false;	// No intersection within the input time interval
		}
	}

	// Test hulls against cross-edge planes
	const physx::PxU32 numHullEdges = getUniqueEdgeDirectionCount();
	for (physx::PxU32 hullEdgeIndex = 0; hullEdgeIndex < numHullEdges; ++hullEdgeIndex)
	{
		const physx::PxVec3 hullEdge = getEdgeDirection(hullEdgeIndex);
		for (physx::PxU32 boxEdgeIndex = 0; boxEdgeIndex < 3; ++boxEdgeIndex)
		{
			physx::PxVec3 n = hullEdge.cross(boxAxes[boxEdgeIndex]);
			const physx::PxF32 n2 = n.magnitudeSquared();
			if (n2 < PX_EPS_F32 * PX_EPS_F32)
			{
				continue;
			}
			n *= physx::recipSqrtFast(n2);
			physx::PxF32 vel0 = disp.dot(n);
			// Choose normal direction such that the normal component of velocity is negative
			if (vel0 > 0.0f)
			{
				vel0 = -vel0;
				n *= -1.0f;
			}
			const physx::PxF32 projectedRadius = physx::PxAbs(n.dot(boxAxes[(1 << boxEdgeIndex) & 3])) +
			                                     physx::PxAbs(n.dot(boxAxes[(3 >> boxEdgeIndex) ^ 1]));
			const physx::PxF32 projectedCenter = n.dot(boxCenter);
			physx::PxF32 min, max;
			getExtent(min, max, hullVerts, numHullVerts, sizeof(physx::PxVec3), n);
			physx::PxF32 tIn, tOut;
			extentOverlapTimeInterval(tIn, tOut, vel0, projectedCenter - projectedRadius, projectedCenter + projectedRadius, min, max);
			if (!updateTimeIntervalAndNormal(in, out, tNormal, normal, tIn, tOut, n))
			{
				return false;	// No intersection within the input time interval
			}
		}
	}

	if (normal != NULL)
	{
		NiCof44 cof(localToWorldRT, scale);
		*normal = cof.getBlock33().transform(*normal);
		normal->normalize();
	}

	return true;
}

void ConvexHull::extent(physx::PxF32& min, physx::PxF32& max, const physx::PxVec3& normal) const
{
	getExtent(min, max, mParams->vertices.buf, (physx::PxU32)mParams->vertices.arraySizes[0], sizeof(physx::PxVec3), normal);
}

void ConvexHull::fill(physx::Array<physx::PxVec3>& outPoints, const physx::PxMat34Legacy& localToWorldRT, const physx::PxVec3& scale,
                      physx::PxF32 spacing, physx::PxF32 jitter, physx::PxU32 maxPoints, bool adjustSpacing) const
{
	if (!maxPoints)
	{
		return;
	}

	const physx::PxU32 numPlanes = getPlaneCount();
	physx::PxPlane* hull = (physx::PxPlane*)PxAlloca(numPlanes * sizeof(physx::PxPlane));
	physx::PxF32* recipDens = (physx::PxF32*)PxAlloca(numPlanes * sizeof(physx::PxF32));

	const NiCof44 cof(localToWorldRT, scale);	// matrix of cofactors to correctly transform planes
	physx::PxMat34Legacy srt = localToWorldRT;
	srt.M.multiplyDiagonal(scale);

	physx::PxBounds3 bounds;
	bounds.setEmpty();
	for (physx::PxU32 vertN = 0; vertN < getVertexCount(); ++vertN)
	{
		physx::PxVec3 worldVert;
		srt.multiply(getVertex(vertN), worldVert);
		bounds.include(worldVert);
	}

	physx::PxVec3 center, extents;
	center = bounds.getCenter();
	extents = bounds.getExtents();

	const physx::PxVec3 areas(extents[1]*extents[2], extents[2]*extents[0], extents[0]*extents[1]);
	const physx::PxU32 axisN = areas[0] < areas[1] ? (areas[0] < areas[2] ? 0u : 2u) : (areas[1] < areas[2] ? 1u : 2u);
	const physx::PxU32 axisN1 = (axisN + 1) % 3;
	const physx::PxU32 axisN2 = (axisN + 2) % 3;

	if (adjustSpacing)
	{
		const physx::PxF32 boxVolume = 8 * extents[0] * areas[0];
		const physx::PxF32 cellVolume = spacing * spacing * spacing;
		if (boxVolume > maxPoints * cellVolume)
		{
			spacing = physx::PxPow(boxVolume / maxPoints, 0.333333333f);
		}
	}

	for (physx::PxU32 planeN = 0; planeN < numPlanes; ++planeN)
	{
		cof.transform(getPlane(planeN), hull[planeN]);
		recipDens[planeN] = physx::PxAbs(hull[planeN].n[axisN]) > 1.0e-7 ? 1.0f / hull[planeN].n[axisN] : 0.0f;
	}

	const physx::PxF32 recipSpacing = 1.0f / spacing;
	const physx::PxI32 num1 = (physx::PxI32)(extents[axisN1] * recipSpacing);
	const physx::PxI32 num2 = (physx::PxI32)(extents[axisN2] * recipSpacing);

	QDSRand rnd;
	const physx::PxF32 scaledJitter = jitter * spacing;

	physx::PxVec3 orig;
	for (physx::PxI32 i1 = -num1; i1 <= num1; ++i1)
	{
		orig[axisN1] = i1 * spacing + center[axisN1];
		for (physx::PxI32 i2 = -num2; i2 <= num2; ++i2)
		{
			orig[axisN2] = i2 * spacing + center[axisN2];

			physx::PxF32 out = extents[axisN];
			physx::PxF32 in = -out;

			orig[axisN] = center[axisN];

			physx::PxU32 planeN;
			for (planeN = 0; planeN < numPlanes; ++planeN)
			{
				const physx::PxPlane& plane = hull[planeN];
				const physx::PxF32 recipDen = recipDens[planeN];
				const physx::PxF32 num = -plane.distance(orig);
				if (recipDen == 0.0f)
				{
					if (num < 0)
					{
						break;
					}
				}
				else
				{
					const physx::PxF32 t = num * recipDen;
					if (recipDen > 0)
					{
						if (t < in)
						{
							break;
						}
						out = physx::PxMin(t, out);
					}
					else
					{
						if (t > out)
						{
							break;
						}
						in = physx::PxMax(t, in);
					}
				}
			}

			if (planeN == numPlanes)
			{
				const physx::PxF32 depth = out - in;
				const physx::PxF32 stop = orig[axisN] + out;
				orig[axisN] += in + 0.5f * (depth - spacing * (int)(depth * recipSpacing));
				do
				{
					outPoints.pushBack(orig + scaledJitter * physx::PxVec3(rnd.getNext(), rnd.getNext(), rnd.getNext()));
					if (!--maxPoints)
					{
						return;
					}
				}
				while ((orig[axisN] += spacing) <= stop);
			}
		}
	}
}

physx::PxU32 ConvexHull::calculateCookedSizes(physx::PxU32& vertexCount, physx::PxU32& faceCount, bool inflated) const
{
	vertexCount = 0;
	faceCount = 0;
	PxMemoryBuffer memStream;
	memStream.setEndianMode(physx::PxFileBuf::ENDIAN_NONE);
	NxFromPxStream nxs(memStream);
#if NX_SDK_VERSION_MAJOR == 2
	NxConvexMeshDesc meshDesc;
	meshDesc.numVertices = getVertexCount();
	meshDesc.points = &getVertex(0);
	meshDesc.pointStrideBytes = sizeof(physx::PxVec3);
	meshDesc.flags = NX_CF_COMPUTE_CONVEX | NX_CF_USE_UNCOMPRESSED_NORMALS;
	const physx::PxF32 skinWidth = NxGetApexSDK()->getCookingInterface() != NULL ? NxGetApexSDK()->getCookingInterface()->NxGetCookingParams().skinWidth : 0.0f;
	if (inflated && skinWidth > 0.0f)
	{
		meshDesc.flags |= NX_CF_INFLATE_CONVEX;
	}
	if (NxGetApexSDK()->getCookingInterface()->NxCookConvexMesh(meshDesc, nxs))
	{
		NxConvexMesh* convexMesh = NxGetApexSDK()->getPhysXSDK()->createConvexMesh(nxs);
		if (convexMesh != NULL)
		{
			vertexCount = convexMesh->getCount(0, NX_ARRAY_VERTICES);
			faceCount = convexMesh->getCount(0, NX_ARRAY_HULL_POLYGONS);
		}
		NxGetApexSDK()->getPhysXSDK()->releaseConvexMesh(*convexMesh);
	}		
#elif NX_SDK_VERSION_MAJOR == 3
	PxConvexMeshDesc meshDesc;
	meshDesc.points.count = getVertexCount();
	meshDesc.points.data = &getVertex(0);
	meshDesc.points.stride = sizeof(physx::PxVec3);
	meshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;
	const physx::PxF32 skinWidth = NxGetApexSDK()->getCookingInterface() != NULL ? NxGetApexSDK()->getCookingInterface()->getParams().skinWidth : 0.0f;
	if (inflated && skinWidth > 0.0f)
	{
		meshDesc.flags |= PxConvexFlag::eINFLATE_CONVEX;
	}
	if (NxGetApexSDK()->getCookingInterface()->cookConvexMesh(meshDesc, nxs))
	{
		PxConvexMesh* convexMesh = NxGetApexSDK()->getPhysXSDK()->createConvexMesh(nxs);
		if (convexMesh != NULL)
		{
			vertexCount = convexMesh->getNbVertices();
			faceCount = convexMesh->getNbPolygons();
		}
		convexMesh->release();
	}
#endif
	return memStream.getFileLength();
}

bool ConvexHull::reduceByCooking()
{
	bool reduced = false;
	PxMemoryBuffer memStream;
	memStream.setEndianMode(physx::PxFileBuf::ENDIAN_NONE);
	NxFromPxStream nxs(memStream);
#if NX_SDK_VERSION_MAJOR == 2
	NxConvexMeshDesc meshDesc;
	meshDesc.numVertices = getVertexCount();
	meshDesc.points = &getVertex(0);
	meshDesc.pointStrideBytes = sizeof(physx::PxVec3);
	meshDesc.flags = NX_CF_COMPUTE_CONVEX;
	if (NxGetApexSDK()->getCookingInterface()->NxCookConvexMesh(meshDesc, nxs))
	{
		NxConvexMesh* convexMesh = NxGetApexSDK()->getPhysXSDK()->createConvexMesh(nxs);
		if (convexMesh != NULL)
		{
			const physx::PxU32 vertexCount = convexMesh->getCount(0, NX_ARRAY_VERTICES);
			reduced = vertexCount < getVertexCount();
			if (reduced)
			{
				buildFromConvexMesh(convexMesh);
			}
		}
		NxGetApexSDK()->getPhysXSDK()->releaseConvexMesh(*convexMesh);
	}		
#elif NX_SDK_VERSION_MAJOR == 3
	PxConvexMeshDesc meshDesc;
	meshDesc.points.count = getVertexCount();
	meshDesc.points.data = &getVertex(0);
	meshDesc.points.stride = sizeof(physx::PxVec3);
	meshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;
	if (NxGetApexSDK()->getCookingInterface()->cookConvexMesh(meshDesc, nxs))
	{
		PxConvexMesh* convexMesh = NxGetApexSDK()->getPhysXSDK()->createConvexMesh(nxs);
		if (convexMesh != NULL)
		{
			const physx::PxU32 vertexCount = convexMesh->getNbVertices();
			reduced = vertexCount < getVertexCount();
			if (reduced)
			{
				buildFromConvexMesh(convexMesh);
			}
		}
		convexMesh->release();
	}
#endif
	return reduced;
}

struct IndexedAngle
{
	physx::PxF32 angle;
	physx::PxU32 index;

	struct LT
	{
		PX_INLINE bool operator()(const IndexedAngle& a, const IndexedAngle& b) const
		{
			return a.angle < b.angle;
		}
	};
};

bool ConvexHull::reduceHull(physx::PxU32 maxVertexCount, physx::PxU32 maxEdgeCount, physx::PxU32 maxFaceCount, bool inflated)
{
	// Translate max = 0 => max = "infinity"
	if (maxVertexCount == 0)
	{
		maxVertexCount = 0xFFFFFFFF;
	}

	if (maxEdgeCount == 0)
	{
		maxEdgeCount = 0xFFFFFFFF;
	}

	if (maxFaceCount == 0)
	{
		maxFaceCount = 0xFFFFFFFF;
	}

	while (reduceByCooking()) {}

	// Calculate sizes
	physx::PxU32 cookedVertexCount;
	physx::PxU32 cookedFaceCount;
	calculateCookedSizes(cookedVertexCount, cookedFaceCount, inflated);
	physx::PxU32 cookedEdgeCount = cookedVertexCount + cookedFaceCount - 2;

	// Return successfully if we've met the required limits
	if (cookedVertexCount <= maxVertexCount && cookedEdgeCount <= maxEdgeCount && cookedFaceCount <= maxFaceCount)
	{
		return true;
	}

	NxParameterized::Handle handle(*mParams, "vertices");
	physx::PxU32 vertexCount = 0;
	mParams->getArraySize(handle, (physx::PxI32&)vertexCount);
	if (vertexCount < 4)
	{
		return false;
	}
	physx::Array<physx::PxVec3> vertices(vertexCount);
	mParams->getParamVec3Array(handle, &vertices[0], (physx::PxI32)vertexCount);

	// We have at least four vertices in our hull.  Find the tetrahedron with the largest volume.
	physx::PxMat44 tetrahedron;
	physx::PxF32 maxTetVolume = 0.0f;
	physx::PxU32 tetIndices[4] = {0,1,2,3};
	for (physx::PxU32 i = 0; i < vertexCount-3; ++i)
	{
		tetrahedron.column0 = physx::PxVec4(vertices[i], 1.0f);
		for (physx::PxU32 j = i+1; j < vertexCount-2; ++j)
		{
			tetrahedron.column1 = physx::PxVec4(vertices[j], 1.0f);
			for (physx::PxU32 k = j+1; k < vertexCount-1; ++k)
			{
				tetrahedron.column2 = physx::PxVec4(vertices[k], 1.0f);
				for (physx::PxU32 l = k+1; l < vertexCount; ++l)
				{
					tetrahedron.column3 = physx::PxVec4(vertices[l], 1.0f);
					const physx::PxF32 v = physx::PxAbs(det(tetrahedron));
					if (v > maxTetVolume)
					{
						maxTetVolume = v;
						tetIndices[0] = i;
						tetIndices[1] = j;
						tetIndices[2] = k;
						tetIndices[3] = l;
					}
				}
			}
		}
	}

	// Remove tetradhedral vertices from vertices array, put in reducedVertices array
	physx::Array<physx::PxVec3> reducedVertices(4);
	for (physx::PxU32 vNum = 4; vNum--;)
	{
		reducedVertices[vNum] = vertices[tetIndices[vNum]];
		vertices.replaceWithLast(tetIndices[vNum]);	// Works because tetIndices[i] < tetIndices[j] if i < j
	}
	buildFromPoints(&reducedVertices[0], 4, sizeof(physx::PxVec3));

	calculateCookedSizes(cookedVertexCount, cookedFaceCount, inflated);
	cookedEdgeCount = cookedVertexCount + cookedFaceCount - 2;
	if (cookedVertexCount > maxVertexCount || cookedEdgeCount > maxEdgeCount || cookedFaceCount > maxFaceCount)
	{
		return false;	// Even a tetrahedron exceeds our limits
	}

	physx::Array<physx::PxU32> faceEdges;
	while (vertices.size())
	{
		physx::PxF32 maxVolumeIncrease = 0.0f;
		physx::PxU32 bestVertexIndex = 0;
		for (physx::PxU32 testVertexIndex = 0; testVertexIndex < vertices.size(); ++testVertexIndex)
		{
			const physx::PxVec3& testVertex = vertices[testVertexIndex];
			physx::PxF32 volumeIncrease = 0.0f;
			physx::PxMat44 addedTet;
			addedTet.column0 = physx::PxVec4(testVertex, 1.0f);
			for (physx::PxU32 planeIndex = 0; planeIndex < getPlaneCount(); ++planeIndex)
			{
				const physx::PxPlane plane = getPlane(planeIndex);
				if (plane.distance(testVertex) <= 0.0f)
				{
					continue;
				}
				faceEdges.resize(0);
				physx::PxVec3 faceCenter(0.0f);
				for (physx::PxU32 edgeIndex = 0; edgeIndex < getEdgeCount(); ++edgeIndex)
				{
					if (getEdgeAdjacentFaceIndex(edgeIndex, 0) == planeIndex || getEdgeAdjacentFaceIndex(edgeIndex, 1) == planeIndex)
					{
						faceCenter += getVertex(getEdgeEndpointIndex(edgeIndex, 0)) + getVertex(getEdgeEndpointIndex(edgeIndex, 1));
						faceEdges.pushBack(edgeIndex);
					}
				}
				if (faceEdges.size())
				{
					faceCenter *= 0.5f/(physx::PxF32)faceEdges.size();
				}
				addedTet.column1 = physx::PxVec4(faceCenter, 1.0f);
				for (physx::PxU32 edgeNum = 0; edgeNum < faceEdges.size(); ++edgeNum)
				{
					const physx::PxU32 edgeIndex = faceEdges[edgeNum];
					addedTet.column2 = physx::PxVec4(getVertex(getEdgeEndpointIndex(edgeIndex,0)), 1.0f);
					addedTet.column3 = physx::PxVec4(getVertex(getEdgeEndpointIndex(edgeIndex,1)), 1.0f);
					volumeIncrease += physx::PxAbs(det(addedTet));
				}
			}
			if (volumeIncrease > maxVolumeIncrease)
			{
				maxVolumeIncrease = volumeIncrease;
				bestVertexIndex = testVertexIndex;
			}
		}

		reducedVertices.pushBack(vertices[bestVertexIndex]);
		vertices.replaceWithLast(bestVertexIndex);
		buildFromPoints(&reducedVertices[0], reducedVertices.size(), sizeof(physx::PxVec3));

		calculateCookedSizes(cookedVertexCount, cookedFaceCount, inflated);
		cookedEdgeCount = cookedVertexCount + cookedFaceCount - 2;
		if (cookedVertexCount > maxVertexCount || cookedEdgeCount > maxEdgeCount || cookedFaceCount > maxFaceCount)
		{
			// Exceeded limits, remove last
			reducedVertices.popBack();
			buildFromPoints(&reducedVertices[0], reducedVertices.size(), sizeof(physx::PxVec3));
			break;
		}
	}

	return true;
}

bool ConvexHull::createKDOPDirections(physx::Array<physx::PxVec3>& directions, NxConvexHullMethod::Enum method)
{
	physx::PxU32 dirs;
	switch (method)
	{
	case NxConvexHullMethod::USE_6_DOP:
		dirs = 0;
		break;
	case NxConvexHullMethod::USE_10_DOP_X:
		dirs = 1;
		break;
	case NxConvexHullMethod::USE_10_DOP_Y:
		dirs = 2;
		break;
	case NxConvexHullMethod::USE_10_DOP_Z:
		dirs = 4;
		break;
	case NxConvexHullMethod::USE_14_DOP_XY:
		dirs = 3;
		break;
	case NxConvexHullMethod::USE_14_DOP_YZ:
		dirs = 6;
		break;
	case NxConvexHullMethod::USE_14_DOP_ZX:
		dirs = 5;
		break;
	case NxConvexHullMethod::USE_18_DOP:
		dirs = 7;
		break;
	case NxConvexHullMethod::USE_26_DOP:
		dirs = 15;
		break;
	default:
		return false;
	}
	directions.reset();
	directions.pushBack(physx::PxVec3(1, 0, 0));
	directions.pushBack(physx::PxVec3(0, 1, 0));
	directions.pushBack(physx::PxVec3(0, 0, 1));
	if (dirs & 1)
	{
		directions.pushBack(physx::PxVec3(0, 1, 1));
		directions.pushBack(physx::PxVec3(0, -1, 1));
	}
	if (dirs & 2)
	{
		directions.pushBack(physx::PxVec3(1, 0, 1));
		directions.pushBack(physx::PxVec3(1, 0, -1));
	}
	if (dirs & 4)
	{
		directions.pushBack(physx::PxVec3(1, 1, 0));
		directions.pushBack(physx::PxVec3(-1, 1, 0));
	}
	if (dirs & 8)
	{
		directions.pushBack(physx::PxVec3(1, 1, 1));
		directions.pushBack(physx::PxVec3(-1, 1, 1));
		directions.pushBack(physx::PxVec3(1, -1, 1));
		directions.pushBack(physx::PxVec3(1, 1, -1));
	}
	return true;
}


void createIndexStartLookup(physx::Array<physx::PxU32>& lookup, physx::PxI32 indexBase, physx::PxU32 indexRange, physx::PxI32* indexSource, physx::PxU32 indexCount, physx::PxU32 indexByteStride)
{
	if (indexRange == 0)
	{
		lookup.resize(physx::PxMax(indexRange + 1, 2u));
		lookup[0] = 0;
		lookup[1] = indexCount;
	}
	else
	{
		lookup.resize(indexRange + 1);
		physx::PxU32 indexPos = 0;
		for (physx::PxU32 i = 0; i < indexRange; ++i)
		{
			for (; indexPos < indexCount; ++indexPos, indexSource = (physx::PxI32*)((uintptr_t)indexSource + indexByteStride))
			{
				if (*indexSource >= (physx::PxI32)i + indexBase)
				{
					lookup[i] = indexPos;
					break;
				}
			}
			if (indexPos == indexCount)
			{
				lookup[i] = indexPos;
			}
		}
		lookup[indexRange] = indexCount;
	}
}

void findIslands(physx::Array< physx::Array<physx::PxU32> >& islands, const physx::Array<IntPair>& overlaps, physx::PxU32 indexRange)
{
	// symmetrize the overlap list
	physx::Array<IntPair> symOverlaps;
	symOverlaps.reserve(2 * overlaps.size());
	for (physx::PxU32 i = 0; i < overlaps.size(); ++i)
	{
		const IntPair& pair = overlaps[i];
		symOverlaps.pushBack(pair);
		IntPair& reversed = symOverlaps.insert();
		reversed.set(pair.i1, pair.i0);
	}
	// Sort symmetrized list
	qsort(symOverlaps.begin(), symOverlaps.size(), sizeof(IntPair), IntPair::compare);
	// Create overlap look-up
	physx::Array<physx::PxU32> overlapStarts;
	createIndexStartLookup(overlapStarts, 0, indexRange, &symOverlaps.begin()->i0, symOverlaps.size(), sizeof(IntPair));
	// Find islands
	NxIndexBank<physx::PxU32> objectIndices(indexRange);
	objectIndices.lockCapacity(true);
	physx::PxU32 seedIndex = PX_MAX_U32;
	while (objectIndices.useNextFree(seedIndex))
	{
		physx::Array<physx::PxU32>& island = islands.insert();
		island.pushBack(seedIndex);
		for (physx::PxU32 i = 0; i < island.size(); ++i)
		{
			const physx::PxU32 index = island[i];
			for (physx::PxU32 j = overlapStarts[index]; j < overlapStarts[index + 1]; ++j)
			{
				PX_ASSERT(symOverlaps[j].i0 == (physx::PxI32)index);
				const physx::PxU32 overlapIndex = (physx::PxU32)symOverlaps[j].i1;
				if (objectIndices.use(overlapIndex))
				{
					island.pushBack(overlapIndex);
				}
			}
		}
	}
}


// Neighbor-finding utility class

void NeighborLookup::setBounds(const BoundsRep* bounds, physx::PxU32 boundsCount, physx::PxU32 boundsByteStride)
{
	m_neighbors.resize(0);
	m_firstNeighbor.resize(0);

	if (bounds != NULL && boundsCount > 0 && boundsByteStride >= sizeof(BoundsRep))
	{
		physx::Array<IntPair> overlaps;
		boundsCalculateOverlaps(overlaps, Bounds3XYZ, bounds, boundsCount, boundsByteStride);
		// symmetrize the overlap list
		const physx::PxU32 oldSize = overlaps.size();
		overlaps.resize(2 * oldSize);
		for (physx::PxU32 i = 0; i < oldSize; ++i)
		{
			const IntPair& pair = overlaps[i];
			overlaps[i+oldSize].set(pair.i1, pair.i0);
		}
		// Sort symmetrized list
		qsort(overlaps.begin(), overlaps.size(), sizeof(IntPair), IntPair::compare);
		// Create overlap look-up
		if (overlaps.size() > 0)
		{
			createIndexStartLookup(m_firstNeighbor, 0, boundsCount, &overlaps[0].i0, overlaps.size(), sizeof(IntPair));
			m_neighbors.resize(overlaps.size());
			for (physx::PxU32 i = 0; i < overlaps.size(); ++i)
			{
				m_neighbors[i] = (physx::PxU32)overlaps[i].i1;
			}
		}
	}
}

physx::PxU32 NeighborLookup::getNeighborCount(const physx::PxU32 index) const
{
	if (index + 1 >= m_firstNeighbor.size())
	{
		return 0;	// Invalid neighbor list or index
	}

	return m_firstNeighbor[index+1] - m_firstNeighbor[index];
}

const physx::PxU32* NeighborLookup::getNeighbors(const physx::PxU32 index) const
{
	if (index + 1 >= m_firstNeighbor.size())
	{
		return 0;	// Invalid neighbor list or index
	}

	return &m_neighbors[m_firstNeighbor[index]];
}


// Data conversion macros

#define copyBuffer( _DstFormat, _SrcFormat ) \
	{ \
		PxU32 _numVertices = numVertices; \
		PxI32* _invMap = invMap; \
		_DstFormat##_TYPE* _dst = (_DstFormat##_TYPE*)dst; \
		const _SrcFormat##_TYPE* _src = (const _SrcFormat##_TYPE*)src; \
		if( _invMap == NULL ) \
		{ \
			while( _numVertices-- ) \
			{ \
				convert_##_DstFormat##_from_##_SrcFormat( *_dst, *_src ); \
				((PxU8*&)_dst) += dstStride; \
				((const PxU8*&)_src) += srcStride; \
			} \
		} \
		else \
		{ \
			while( _numVertices-- ) \
			{ \
				const PxI32 invMapValue = *_invMap++; \
				if (invMapValue >= 0) \
				{ \
					_DstFormat##_TYPE* _dstElem = (_DstFormat##_TYPE*)((PxU8*)_dst + invMapValue*dstStride); \
					convert_##_DstFormat##_from_##_SrcFormat( *_dstElem, *_src ); \
				} \
				((const PxU8*&)_src) += srcStride; \
			} \
		} \
	}


#define HANDLE_COPY1( _DstFormat, _SrcFormat ) \
	case NxRenderDataFormat::_DstFormat : \
		if( srcFormat == NxRenderDataFormat::_SrcFormat ) \
		{ \
			copyBuffer( _DstFormat, _SrcFormat ); \
		} \
		break;

#define HANDLE_COPY2( _DstFormat, _SrcFormat1, _SrcFormat2 ) \
	case NxRenderDataFormat::_DstFormat : \
		if( srcFormat == NxRenderDataFormat::_SrcFormat1 ) \
		{ \
			copyBuffer( _DstFormat, _SrcFormat1 ); \
		} \
		else if( srcFormat == NxRenderDataFormat::_SrcFormat2 ) \
		{ \
			copyBuffer( _DstFormat, _SrcFormat2 ); \
		} \
		break;

#define HANDLE_COPY3( _DstFormat, _SrcFormat1, _SrcFormat2, _SrcFormat3 ) \
	case NxRenderDataFormat::_DstFormat : \
		if( srcFormat == NxRenderDataFormat::_SrcFormat1 ) \
		{ \
			copyBuffer( _DstFormat, _SrcFormat1 ); \
		} \
		else if( srcFormat == NxRenderDataFormat::_SrcFormat2 ) \
		{ \
			copyBuffer( _DstFormat, _SrcFormat2 ); \
		} \
		else if( srcFormat == NxRenderDataFormat::_SrcFormat3 ) \
		{ \
			copyBuffer( _DstFormat, _SrcFormat3 ); \
		} \
		break;

// ... etc.


bool copyRenderVertexBuffer(void* dst, NxRenderDataFormat::Enum dstFormat, PxU32 dstStride, PxU32 dstStart, const void* src, NxRenderDataFormat::Enum srcFormat, PxU32 srcStride, PxU32 srcStart, PxU32 numVertices, PxI32* invMap)
{
	const PxU32 dstDataSize = NxRenderDataFormat::getFormatDataSize(dstFormat);
	if (dstStride == 0)
	{
		dstStride = dstDataSize;
	}

	if (dstStride < dstDataSize)
	{
		return false;
	}

	// advance src pointer
	((const PxU8*&)src) += srcStart * srcStride;

	PX_ASSERT((dstStart == 0) || (invMap == NULL)); // can only be one of them, won't work if its both!

	if (dstFormat == srcFormat)
	{
		if (dstFormat != NxRenderDataFormat::UNSPECIFIED)
		{
			// Direct data copy

			// advance dst pointer
			((const PxU8*&)dst) += dstStart * dstStride;

			if (invMap == NULL)
			{
				while (numVertices--)
				{
					memcpy(dst, src, dstDataSize);
					((PxU8*&)dst) += dstStride;
					((const PxU8*&)src) += srcStride;
				}
			}
			else
			{
				while (numVertices--)
				{
					const PxI32 invMapValue = *invMap++;
					if (invMapValue >= 0)
					{
						void* mappedDst = (void*)((PxU8*)dst + dstStride * (invMapValue));
						memcpy(mappedDst, src, dstDataSize);
					}
					((const PxU8*&)src) += srcStride;
				}
			}
		}

		return true;
	}

	// advance dst pointer
	((const PxU8*&)dst) += dstStart * dstStride;

	switch (dstFormat)
	{
	case NxRenderDataFormat::UNSPECIFIED:
		// Handle unspecified by doing nothing (still no error!)
		break;

		// Put format converters here
		HANDLE_COPY1(USHORT1, UINT1)
		HANDLE_COPY1(USHORT2, UINT2)
		HANDLE_COPY1(USHORT3, UINT3)
		HANDLE_COPY1(USHORT4, UINT4)

		HANDLE_COPY1(UINT1, USHORT1)
		HANDLE_COPY1(UINT2, USHORT2)
		HANDLE_COPY1(UINT3, USHORT3)
		HANDLE_COPY1(UINT4, USHORT4)

		HANDLE_COPY1(BYTE_SNORM1, FLOAT1)
		HANDLE_COPY1(BYTE_SNORM2, FLOAT2)
		HANDLE_COPY1(BYTE_SNORM3, FLOAT3)
		HANDLE_COPY1(BYTE_SNORM4, FLOAT4)
		HANDLE_COPY1(BYTE_SNORM4_QUATXYZW, FLOAT4_QUAT)
		HANDLE_COPY1(SHORT_SNORM1, FLOAT1)
		HANDLE_COPY1(SHORT_SNORM2, FLOAT2)
		HANDLE_COPY1(SHORT_SNORM3, FLOAT3)
		HANDLE_COPY1(SHORT_SNORM4, FLOAT4)
		HANDLE_COPY1(SHORT_SNORM4_QUATXYZW, FLOAT4_QUAT)

		HANDLE_COPY2(FLOAT1, BYTE_SNORM1, SHORT_SNORM1)
		HANDLE_COPY2(FLOAT2, BYTE_SNORM2, SHORT_SNORM2)
		HANDLE_COPY2(FLOAT3, BYTE_SNORM3, SHORT_SNORM3)
		HANDLE_COPY2(FLOAT4, BYTE_SNORM4, SHORT_SNORM4)
		HANDLE_COPY2(FLOAT4_QUAT, BYTE_SNORM4_QUATXYZW, SHORT_SNORM4_QUATXYZW)

		HANDLE_COPY3(R8G8B8A8, B8G8R8A8, R32G32B32A32_FLOAT, B32G32R32A32_FLOAT)
		HANDLE_COPY3(B8G8R8A8, R8G8B8A8, R32G32B32A32_FLOAT, B32G32R32A32_FLOAT)
		HANDLE_COPY3(R32G32B32A32_FLOAT, R8G8B8A8, B8G8R8A8, B32G32R32A32_FLOAT)
		HANDLE_COPY3(B32G32R32A32_FLOAT, R8G8B8A8, B8G8R8A8, R32G32B32A32_FLOAT)

	default:
	{
		PX_ALWAYS_ASSERT();	// Format conversion not handled
		return false;
	}
	}


	return true;
}



/************************************************************************/
// Convex Hull from Planes
/************************************************************************/

// copied from //sw/physx/PhysXSDK/3.2/trunk/Source/Common/src/CmMathUtils.cpp
PxQuat PxShortestRotation(const PxVec3& v0, const PxVec3& v1)
{
	const PxReal d = v0.dot(v1);
	const PxVec3 cross = v0.cross(v1);

	PxQuat q = d>-1 ? PxQuat(cross.x, cross.y, cross.z, 1+d) 
		: PxAbs(v0.x)<0.1f ? PxQuat(0.0f, v0.z, -v0.y, 0.0f) : PxQuat(v0.y, -v0.x, 0.0f, 0.0f);

	return q.getNormalized();
}

PxTransform PxTransformFromPlaneEquation(const PxPlane& plane)
{
	PxPlane p = plane; 
	p.normalize();

	// special case handling for axis aligned planes
	const PxReal halfsqrt2 = 0.707106781;
	PxQuat q;
	if(2 == (p.n.x == 0.0f) + (p.n.y == 0.0f) + (p.n.z == 0.0f)) // special handling for axis aligned planes
	{
		if(p.n.x > 0)		q = PxQuat::createIdentity();
		else if(p.n.x < 0)	q = PxQuat(0, 0, 1, 0);
		else				q = PxQuat(0.0f, -p.n.z, p.n.y, 1) * halfsqrt2;
	}
	else q = PxShortestRotation(PxVec3(1,0,0), p.n);

	return PxTransform(-p.n * p.d, q);
}



PxVec3 intersect(PxVec4 p0, PxVec4 p1, PxVec4 p2)
{
	const PxVec3& d0 = reinterpret_cast<const PxVec3&>(p0);
	const PxVec3& d1 = reinterpret_cast<const PxVec3&>(p1);
	const PxVec3& d2 = reinterpret_cast<const PxVec3&>(p2);

	return (p0.w * d1.cross(d2) 
		  + p1.w * d2.cross(d0) 
		  + p2.w * d0.cross(d1))
		  / d0.dot(d2.cross(d1));
}

const PxU16 sInvalid = PxU16(-1);

// restriction: only supports a single patch per vertex.
struct HalfedgeMesh
{
	struct Halfedge
	{
		Halfedge(PxU16 vertex = sInvalid, PxU16 face = sInvalid, 
			PxU16 next = sInvalid, PxU16 prev = sInvalid)
			: mVertex(vertex), mFace(face), mNext(next), mPrev(prev)
		{}

		PxU16 mVertex; // to
		PxU16 mFace; // left
		PxU16 mNext; // ccw
		PxU16 mPrev; // cw
	};

	HalfedgeMesh() : mNumTriangles(0) {}

	PxU16 findHalfedge(PxU16 v0, PxU16 v1)
	{
		PxU16 h = mVertices[v0], start = h;
		while(h != sInvalid && mHalfedges[h].mVertex != v1)
		{
			h = mHalfedges[(physx::PxU32)h ^ 1].mNext;
			if(h == start) 
				return sInvalid;
		}
		return h;
	}

	void connect(PxU16 h0, PxU16 h1)
	{
		mHalfedges[h0].mNext = h1;
		mHalfedges[h1].mPrev = h0;
	}

	void addTriangle(PxU16 v0, PxU16 v1, PxU16 v2)
	{
		// add new vertices
		PxU16 n = (PxU16)(PxMax(v0, PxMax(v1, v2))+1);
		if(mVertices.size() < n)
			mVertices.resize(n, sInvalid);

		// collect halfedges, prev and next of triangle
		PxU16 verts[] = { v0, v1, v2 };
		PxU16 handles[3], prev[3], next[3];
		for(PxU16 i=0; i<3; ++i)
		{
			PxU16 j = (PxU16)((i+1)%3);
			PxU16 h = findHalfedge(verts[i], verts[j]);
			if(h == sInvalid)
			{
				// add new edge
				PX_ASSERT(mHalfedges.size() <= PX_MAX_U16);
				h = (PxU16)mHalfedges.size();
				mHalfedges.pushBack(Halfedge(verts[j]));
				mHalfedges.pushBack(Halfedge(verts[i]));
			}
			handles[i] = h;
			prev[i] = mHalfedges[h].mPrev;
			next[i] = mHalfedges[h].mNext;
		}

		// patch connectivity
		for(PxU16 i=0; i<3; ++i)
		{
			PxU16 j = (PxU16)((i+1)%3);

			PX_ASSERT(mFaces.size() <= PX_MAX_U16);
			mHalfedges[handles[i]].mFace = (PxU16)mFaces.size();

			// connect prev and next
			connect(handles[i], handles[j]);

			if(next[j] == sInvalid) // new next edge, connect opposite
				connect((PxU32)(handles[j]^1), next[i]!=sInvalid ? next[i] : (PxU32)(handles[i]^1));

			if(prev[i] == sInvalid) // new prev edge, connect opposite
				connect(prev[j]!=sInvalid ? prev[j] : (PxU32)(handles[j]^1), (PxU32)(handles[i]^1));

			// prev is boundary, update middle vertex
			if(mHalfedges[(PxU32)(handles[i]^1)].mFace == sInvalid)
				mVertices[verts[j]] = (PxU32)(handles[i]^1);
		}

		mFaces.pushBack(handles[2]);
		++mNumTriangles;
	}

	PxU16 removeTriangle(PxU16 f)
	{
		PxU16 result = sInvalid;

		for(PxU16 i=0, h = mFaces[f]; i<3; ++i)
		{
			PxU16 v0 = mHalfedges[(PxU32)(h^1)].mVertex;
			PxU16 v1 = mHalfedges[h].mVertex;

			mHalfedges[h].mFace = sInvalid;

			if(mHalfedges[(PxU32)(h^1)].mFace == sInvalid) // was boundary edge, remove
			{
				PxU16 v0Prev = mHalfedges[h  ].mPrev;
				PxU16 v0Next = mHalfedges[(PxU32)(h^1)].mNext;
				PxU16 v1Prev = mHalfedges[(PxU32)(h^1)].mPrev;
				PxU16 v1Next = mHalfedges[h  ].mNext;

				// update halfedge connectivity
				connect(v0Prev, v0Next);
				connect(v1Prev, v1Next);

				// update vertex boundary or delete
				mVertices[v0] = (v0Prev^1) == v0Next ? sInvalid : v0Next;
				mVertices[v1] = (v1Prev^1) == v1Next ? sInvalid : v1Next;
			} 
			else 
			{
				mVertices[v0] = h; // update vertex boundary
				result = v1;
			}

			h = mHalfedges[h].mNext;
		}

		mFaces[f] = sInvalid;
		--mNumTriangles;

		return result;
	}

	// true if vertex v is in front of face f
	bool visible(PxU16 v, PxU16 f)
	{
		PxU16 h = mFaces[f];
		if(h == sInvalid)
			return false;

		PxU16 v0 = mHalfedges[h].mVertex;
		h = mHalfedges[h].mNext;
		PxU16 v1 = mHalfedges[h].mVertex;
		h = mHalfedges[h].mNext;
		PxU16 v2 = mHalfedges[h].mVertex;
		h = mHalfedges[h].mNext;

		return det(mPoints[v], mPoints[v0], mPoints[v1], mPoints[v2]) < -1e-5f;
	}

	/*
	void print() const
	{
		for(PxU32 i=0; i<mFaces.size(); ++i)
		{
			printf("f%u: ", i);
			PxU16 h = mFaces[i];
			if(h == sInvalid)
			{
				printf("deleted\n");
				continue;
			}

			for(int j=0; j<3; ++j)
			{
				printf("h%u -> v%u -> ", PxU32(h), PxU32(mHalfedges[h].mVertex));
				h = mHalfedges[h].mNext;
			}

			printf("\n");
		}

		for(PxU32 i=0; i<mVertices.size(); ++i)
		{
			printf("v%u: ", i);
			PxU16 h = mVertices[i];
			if(h == sInvalid)
			{
				printf("deleted\n");
				continue;
			}
			
			PxU16 start = h;
			do {
				printf("h%u -> v%u, ", PxU32(h), PxU32(mHalfedges[h].mVertex));
				h = mHalfedges[h^1].mNext;
			} while (h != start);

			printf("\n");
		}

		for(PxU32 i=0; i<mHalfedges.size(); ++i)
			printf("h%u: v%u, f%u, p%u, n%u\n", i, PxU32(mHalfedges[i].mVertex),
				PxU32(mHalfedges[i].mFace), PxU32(mHalfedges[i].mPrev), PxU32(mHalfedges[i].mNext));
	}
	*/

	shdfnd::Array<Halfedge> mHalfedges;
	shdfnd::Array<PxU16> mVertices; // vertex -> (boundary) halfedge
	shdfnd::Array<PxU16> mFaces; // face -> halfedge
	shdfnd::Array<PxVec4> mPoints;
	PxU16 mNumTriangles;
};



void ConvexMeshBuilder::operator()(PxU32 planeMask, float scale)
{
	PxU32 numPlanes = shdfnd::bitCount(planeMask);

	if (numPlanes == 1)
	{
		PxTransform t = physx::apex::PxTransformFromPlaneEquation(reinterpret_cast<const PxPlane&>(mPlanes[lowestSetBit(planeMask)]));

		if (!t.isValid())
			return;

		const PxU16 indices[] = { 0, 1, 2, 0, 2, 3 };
		const PxVec3 vertices[] = { 
			PxVec3(0.0f,  scale,  scale), 
			PxVec3(0.0f, -scale,  scale),
			PxVec3(0.0f, -scale, -scale),			
			PxVec3(0.0f,  scale, -scale) };

			PX_ASSERT(mVertices.size() <= PX_MAX_U16);
			PxU16 baseIndex = (PxU16)mVertices.size();

			for (PxU32 i=0; i < 4; ++i)
				mVertices.pushBack(t.transform(vertices[i]));

			for (PxU32 i=0; i < 6; ++i)
				mIndices.pushBack((PxU16)(indices[i] + baseIndex));

			return;
	}

	if(numPlanes < 4)
		return; // todo: handle degenerate cases

	HalfedgeMesh mesh;

	// gather points (planes, that is)
	mesh.mPoints.reserve(numPlanes);
	for(; planeMask; planeMask &= planeMask-1)
		mesh.mPoints.pushBack(mPlanes[shdfnd::lowestSetBit(planeMask)]);

	// initialize to tetrahedron
	mesh.addTriangle(0, 1, 2);
	mesh.addTriangle(0, 3, 1);
	mesh.addTriangle(1, 3, 2);
	mesh.addTriangle(2, 3, 0);

	// flip if inside-out
	if(mesh.visible(3, 0))
		shdfnd::swap(mesh.mPoints[0], mesh.mPoints[1]);

	// iterate through remaining points
	for(PxU16 i=4; i<mesh.mPoints.size(); ++i)
	{
		// remove any visible triangle
		PxU16 v0 = sInvalid;
		for(PxU16 j=0; j<mesh.mFaces.size(); ++j)
		{
			if(mesh.visible(i, j))
				v0 = PxMin(v0, mesh.removeTriangle(j));
		}

		if(v0 == sInvalid)
			continue; // no triangle removed

		if(!mesh.mNumTriangles)
			return; // empty mesh

		// find non-deleted boundary vertex
		for(PxU16 h=0; mesh.mVertices[v0] == sInvalid; h+=2)
		{
			if ((mesh.mHalfedges[h  ].mFace == sInvalid) ^ 
				(mesh.mHalfedges[(PxU32)(h+1)].mFace == sInvalid))
			{
				v0 = mesh.mHalfedges[h].mVertex;
			}
		}

		// tesselate hole
		PxU16 start = v0;
		do {
			PxU16 h = mesh.mVertices[v0];
			PxU16 v1 = mesh.mHalfedges[h].mVertex;
			if(mesh.mFaces.size() == PX_MAX_U16)
				break; // safety net
			mesh.addTriangle(v0, v1, i);
			v0 = v1;
		} while(v0 != start);

		bool noHole = true;
		for(PxU16 h=0; noHole && h < mesh.mHalfedges.size(); h+=2)
		{
			if ((mesh.mHalfedges[h  ].mFace == sInvalid) ^ 
				(mesh.mHalfedges[(PxU32)(h+1)].mFace == sInvalid))
				noHole = false;
		}

		if(!noHole || mesh.mFaces.size() == PX_MAX_U16)
		{
			mesh.mFaces.resize(0);
			mesh.mVertices.resize(0);
			break;
		}
	}

	// convert triangles to vertices (intersection of 3 planes)
	shdfnd::Array<PxU32> face2Vertex(mesh.mFaces.size());
	for(PxU32 i=0; i<mesh.mFaces.size(); ++i)
	{
		face2Vertex[i] = mVertices.size();

		PxU16 h = mesh.mFaces[i];
		if(h == sInvalid)
			continue;

		PxU16 v0 = mesh.mHalfedges[h].mVertex;
		h = mesh.mHalfedges[h].mNext;
		PxU16 v1 = mesh.mHalfedges[h].mVertex;
		h = mesh.mHalfedges[h].mNext;
		PxU16 v2 = mesh.mHalfedges[h].mVertex;

		mVertices.pushBack(intersect(mesh.mPoints[v0], mesh.mPoints[v1], mesh.mPoints[v2]));
	}

	// convert vertices to polygons (face one-ring)
	for(PxU32 i=0; i<mesh.mVertices.size(); ++i)
	{
		PxU16 h = mesh.mVertices[i];
		if(h == sInvalid)
			continue;

		PxU32 v0 = face2Vertex[mesh.mHalfedges[h].mFace];
		h = (PxU16)(mesh.mHalfedges[h].mPrev^1);
		PxU32 v1 = face2Vertex[mesh.mHalfedges[h].mFace];

		while(true)
		{
			h = (PxU16)(mesh.mHalfedges[h].mPrev^1);
			PxU32 v2 = face2Vertex[mesh.mHalfedges[h].mFace];

			if(v0 == v2) 
				break;
			
			PX_ASSERT(v0 <= PX_MAX_U16);
			PX_ASSERT(v1 <= PX_MAX_U16);
			PX_ASSERT(v2 <= PX_MAX_U16);
			mIndices.pushBack((PxU16)v0);
			mIndices.pushBack((PxU16)v2);
			mIndices.pushBack((PxU16)v1);

			v1 = v2; 
		}

	}
}

// Diagonalize a symmetric 3x3 matrix.  Returns the eigenvectors in the first parameter, eigenvalues as the return value.
PxVec3 diagonalizeSymmetric(PxMat33& eigenvectors, const PxMat33& m)
{
	// jacobi rotation using quaternions (from an idea of Stan Melax, with fix for precision issues)

	const PxU32 MAX_ITERS = 24;

	PxQuat q = PxQuat::createIdentity();

	PxMat33 d;
	for(PxU32 i=0; i < MAX_ITERS;i++)
	{
		eigenvectors = PxMat33(q);
		d = eigenvectors.getTranspose() * m * eigenvectors;

		const PxReal d0 = PxAbs(d[1][2]), d1 = PxAbs(d[0][2]), d2 = PxAbs(d[0][1]);
		const PxU32 a = d0 > d1 && d0 > d2 ? 0u : d1 > d2 ? 1u : 2u;						// rotation axis index, from largest off-diagonal element

		const PxU32 a1 = (a+1)%3;
		const PxU32 a2 = (a+2)%3;											
		if(d[a1][a2] == 0.0f || PxAbs(d[a1][a1]-d[a2][a2]) > 2e6*PxAbs(2.0f*d[a1][a2]))
		{
			break;
		}

		const PxReal w = (d[a1][a1]-d[a2][a2]) / (2.0f*d[a1][a2]);					// cot(2 * phi), where phi is the rotation angle
		const PxReal absw = PxAbs(w);

		PxReal c, s;
		if(absw>1000)
		{
			c = 1;
			s = 1/(4*w);														// h will be very close to 1, so use small angle approx instead
		}
		else
		{
			PxReal t = 1 / (absw + PxSqrt(w*w+1));								// absolute value of tan phi
			PxReal h = 1 / PxSqrt(t*t+1);										// absolute value of cos phi
			PX_ASSERT(h!=1);													// |w|<1000 guarantees this with typical IEEE754 machine eps (approx 6e-8)
			c = PxSqrt((1+h)/2);
			s = PxSqrt((1-h)/2) * PxSign(w);
		}

		PxQuat r(0,0,0,c);
		reinterpret_cast<PxVec4&>(r)[a] = s;

		q = (q*r).getNormalized();
	}

	return PxVec3(d.column0.x, d.column1.y, d.column2.z);
}



}
} // end namespace physx::apex
