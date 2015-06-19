/************************************************************************************

Filename    :   ModelTrace.h
Content     :   Ray tracer using a KD-Tree.
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef __MODELTRACE_H__
#define __MODELTRACE_H__

// Use explicit path for FbxConvert
#include "LibOVR/Src/Kernel/OVR_Math.h"
#include "LibOVR/Src/Kernel/OVR_Array.h"

namespace OVR
{

/*
	An Efficient and Robust Ray–Box Intersection Algorithm
	Amy Williams, Steve Barrus, R. Keith Morley, Peter Shirley
	Journal of Graphics Tools, Issue 10, Pages 49-54, June 2005

	Returns true if the ray intersects the bounds.
	't0' and 't1' are the distances along the ray where the intersections occurs.
	1st intersection = rayStart + t0 * rayDir
	2nd intersection = rayStart + t1 * rayDir
*/
bool Intersect_RayBounds( const OVR::Vector3f & rayStart, const OVR::Vector3f & rayDir,
				const OVR::Vector3f & mins, const OVR::Vector3f & maxs,
				float & t0, float & t1 );

/*
	Fast, Minimum Storage Ray/Triangle Intersection
	Tomas Möller, Ben Trumbore
	Journal of Graphics Tools, 1997

	Triangles are back-face culled.
	Returns true if the ray intersects the triangle.
	't0' is the distance along the ray where the intersection occurs.
	intersection = rayStart + t0 * rayDir;
	'u' and 'v' are the barycentric coordinates.
	intersection = ( 1 - u - v ) * v0 + u * v1 + v * v2
*/
bool Intersect_RayTriangle( const OVR::Vector3f & rayStart, const OVR::Vector3f & rayDir,
				const OVR::Vector3f & v0, const OVR::Vector3f & v1, const OVR::Vector3f & v2,
				float & t0, float & u, float & v );

const int RT_KDTREE_MAX_LEAF_TRIANGLES	= 4;

struct kdtree_header_t
{
	int			numVertices;
	int			numUvs;
	int			numIndices;
	int			numNodes;
	int			numLeafs;
	int			numOverflow;
	Bounds3f	bounds;
};

struct kdtree_node_t
{
	// bits [ 0,0] = leaf flag
	// bits [ 2,1] = split plane (0 = x, 1 = y, 2 = z, 3 = invalid)
	// bits [31,3] = index of left child (+1 = right child index), or index of leaf data
	unsigned int	data;
	float			dist;
};

struct kdtree_leaf_t
{
	int			triangles[RT_KDTREE_MAX_LEAF_TRIANGLES];
	int			ropes[6];
	Bounds3f	bounds;
};

struct traceResult_t
{
	int			triangleIndex;
	float		fraction;
	Vector2f	uv;
	Vector3f	normal;
};

class ModelTrace
{
public:
							ModelTrace() {}
							~ModelTrace() {}

	traceResult_t			Trace( const Vector3f & start, const Vector3f & end ) const;
	traceResult_t			Trace_Exhaustive( const Vector3f & start, const Vector3f & end ) const;

public:
	kdtree_header_t			header;
	Array< Vector3f >		vertices;
	Array< Vector2f >		uvs;
	Array< int >			indices;
	Array< kdtree_node_t >	nodes;
	Array< kdtree_leaf_t >	leafs;
	Array< int >			overflow;
};

}	// namespace OVR

#endif // !__MODELTRACE_H__
