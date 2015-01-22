/************************************************************************************

Filename    :   RtTrace.h
Content     :   Ray tracer using a KD-Tree.
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef __RTTRACE_H__
#define __RTTRACE_H__

#include "../LibOVR/Src/Kernel/OVR_Math.h"
#include "../LibOVR/Src/Kernel/OVR_Array.h"

namespace OVR
{

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

class RtTrace
{
public:
							RtTrace() {}
							~RtTrace() {}

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

#endif // !__RTTRACE_H__
