/* Copyright (c) <2003-2011> <Julio Jerez, Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 
* 3. This notice may not be removed or altered from any source distribution.
*/

#ifndef __dgIntersections__
#define __dgIntersections__

#include "dgTypes.h"
#include "dgVector.h"


class dgPlane;
class dgObject;
class dgPolyhedra;

class dgFastRayTest
{
	public:
	dgFastRayTest(const dgVector& l0, const dgVector& l1);

	hacd::HaI32 BoxTest (const dgVector& minBox, const dgVector& maxBox) const;

	hacd::HaF32 PolygonIntersect (const dgVector& normal, const hacd::HaF32* const polygon, hacd::HaI32 strideInBytes, const hacd::HaI32* const indexArray, hacd::HaI32 indexCount) const;

	void Reset (hacd::HaF32 t) 
	{
		m_dpInv = m_dpBaseInv.Scale (hacd::HaF32 (1.0f) / t);
	}

	dgVector m_p0;
	dgVector m_p1;
	dgVector m_diff;
	dgVector m_dpInv;
	dgVector m_dpBaseInv;
	dgVector m_minT;
	dgVector m_maxT;

	dgVector m_tolerance;
	dgVector m_zero;
	hacd::HaI32 m_isParallel[4];

	hacd::HaF32 m_dirError;
}DG_GCC_VECTOR_ALIGMENT;



enum dgIntersectStatus
{
	t_StopSearh,
	t_ContinueSearh
};

typedef dgIntersectStatus (*dgAABBIntersectCallback) (void *context, 
													  const hacd::HaF32* const polygon, hacd::HaI32 strideInBytes,
											          const hacd::HaI32* const indexArray, hacd::HaI32 indexCount);

typedef hacd::HaF32 (*dgRayIntersectCallback) (void *context, 
											 const hacd::HaF32* const polygon, hacd::HaI32 strideInBytes,
											 const hacd::HaI32* const indexArray, hacd::HaI32 indexCount);


class dgBeamHitStruct
{
	public:
	dgVector m_Origin; 
	dgVector m_NormalOut;
	dgObject* m_HitObjectOut; 
	hacd::HaF32 m_ParametricIntersctionOut;
};


bool  dgRayBoxClip(dgVector& ray_p0, dgVector& ray_p1, const dgVector& boxP0, const dgVector& boxP1); 

dgVector  dgPointToRayDistance(const dgVector& point, const dgVector& ray_p0, const dgVector& ray_p1); 

void  dgRayToRayDistance (const dgVector& ray_p0, 
							   const dgVector& ray_p1,
							   const dgVector& ray_q0, 
							   const dgVector& ray_q1,
							   dgVector& p0Out, 
							   dgVector& p1Out); 

dgVector    dgPointToTriangleDistance (const dgVector& point, const dgVector& p0, const dgVector& p1, const dgVector& p2);
dgBigVector dgPointToTriangleDistance (const dgBigVector& point, const dgBigVector& p0, const dgBigVector& p1, const dgBigVector& p2);

bool   dgPointToPolygonDistance (const dgVector& point, const hacd::HaF32* const polygon,  hacd::HaI32 strideInInBytes,   
									  const hacd::HaI32* const indexArray, hacd::HaI32 indexCount, hacd::HaF32 bailOutDistance, dgVector& pointOut);   


/*
hacd::HaI32  dgConvexPolyhedraToPlaneIntersection (
							const dgPolyhedra& convexPolyhedra,
							const hacd::HaF32 polyhedraVertexArray[],
							int polyhedraVertexStrideInBytes,
							const dgPlane& plane,
							dgVector* intersectionOut,
							hacd::HaI32 maxSize);


hacd::HaI32  ConvecxPolygonToConvexPolygonIntersection (
							hacd::HaI32 polygonIndexCount_1,
							const hacd::HaI32 polygonIndexArray_1[],
						   hacd::HaI32 convexPolygonStrideInBytes_1,   
							const hacd::HaF32 convexPolygonVertex_1[], 
							hacd::HaI32 polygonIndexCount_2,
							const hacd::HaI32 polygonIndexArray_2[],
						   hacd::HaI32 convexPolygonStrideInBytes_2,   
							const hacd::HaF32 convexPolygonVertex_2[], 
							dgVector* intersectionOut,
							hacd::HaI32 maxSize);


hacd::HaI32  dgConvexIntersection (const dgVector convexPolyA[], hacd::HaI32 countA,
																 const dgVector convexPolyB[], hacd::HaI32 countB,
																 dgVector output[]);

*/


HACD_FORCE_INLINE hacd::HaI32 dgOverlapTest (const dgVector& p0, const dgVector& p1, const dgVector& q0, const dgVector& q1)
{
	return ((p0.m_x < q1.m_x) && (p1.m_x > q0.m_x) && (p0.m_z < q1.m_z) && (p1.m_z > q0.m_z) && (p0.m_y < q1.m_y) && (p1.m_y > q0.m_y)); 
}


HACD_FORCE_INLINE hacd::HaI32 dgBoxInclusionTest (const dgVector& p0, const dgVector& p1, const dgVector& q0, const dgVector& q1)
{
	return (p0.m_x >= q0.m_x) && (p0.m_y >= q0.m_y) && (p0.m_z >= q0.m_z) && (p1.m_x <= q1.m_x) && (p1.m_y <= q1.m_y) && (p1.m_z <= q1.m_z);
}


HACD_FORCE_INLINE hacd::HaI32 dgCompareBox (const dgVector& p0, const dgVector& p1, const dgVector& q0, const dgVector& q1)
{
	return (p0.m_x != q0.m_x) || (p0.m_y != q0.m_y) || (p0.m_z != q0.m_z) || (p1.m_x != q1.m_x) || (p1.m_y != q1.m_y) || (p1.m_z != q1.m_z);
}



dgBigVector LineTriangleIntersection (const dgBigVector& l0, const dgBigVector& l1, const dgBigVector& A, const dgBigVector& B, const dgBigVector& C);

#endif

