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

#include "dgTypes.h"
#include "dgHeap.h"
#include "dgStack.h"
#include "dgSphere.h"
#include "dgPolyhedra.h"
#include "dgConvexHull3d.h"
#include "dgSmallDeterminant.h"
#include <string.h>


#pragma warning(disable:4100)
//#define DG_MIN_EDGE_ASPECT_RATIO  hacd::HaF64 (0.02f)

class dgDiagonalEdge
{
	public:
	dgDiagonalEdge (dgEdge* const edge)
		:m_i0(edge->m_incidentVertex), m_i1(edge->m_twin->m_incidentVertex)
	{
	}
	hacd::HaI32 m_i0;
	hacd::HaI32 m_i1;
};


struct dgEdgeCollapseEdgeHandle
{
	dgEdgeCollapseEdgeHandle (dgEdge* const newEdge)
		:m_inList(false), m_edge(newEdge)
	{
	}

	dgEdgeCollapseEdgeHandle (const dgEdgeCollapseEdgeHandle &dataHandle)
		:m_inList(true), m_edge(dataHandle.m_edge)
	{
		dgEdgeCollapseEdgeHandle* const handle = (dgEdgeCollapseEdgeHandle *)IntToPointer (m_edge->m_userData);
		if (handle) {
			HACD_ASSERT (handle != this);
			handle->m_edge = NULL;
		}
		m_edge->m_userData = hacd::HaU64 (PointerToInt(this));
	}

	~dgEdgeCollapseEdgeHandle ()
	{
		if (m_inList) {
			if (m_edge) {
				dgEdgeCollapseEdgeHandle* const handle = (dgEdgeCollapseEdgeHandle *)IntToPointer (m_edge->m_userData);
				if (handle == this) {
					m_edge->m_userData = PointerToInt (NULL);
				}
			}
		}
		m_edge = NULL;
	}

	hacd::HaU32 m_inList;
	dgEdge* m_edge;
};


class dgVertexCollapseVertexMetric
{
	public:
	hacd::HaF64 elem[10];

	dgVertexCollapseVertexMetric (const dgBigPlane &plane) 
	{
		elem[0] = plane.m_x * plane.m_x;  
		elem[1] = plane.m_y * plane.m_y;  
		elem[2] = plane.m_z * plane.m_z;  
		elem[3] = plane.m_w * plane.m_w;  
		elem[4] = hacd::HaF64 (2.0) * plane.m_x * plane.m_y;  
		elem[5] = hacd::HaF64 (2.0) * plane.m_x * plane.m_z;  
		elem[6] = hacd::HaF64 (2.0) * plane.m_x * plane.m_w;  
		elem[7] = hacd::HaF64 (2.0) * plane.m_y * plane.m_z;  
		elem[8] = hacd::HaF64 (2.0) * plane.m_y * plane.m_w;  
		elem[9] = hacd::HaF64 (2.0) * plane.m_z * plane.m_w;  
	}

	void Clear ()
	{
		memset (elem, 0, 10 * sizeof (hacd::HaF64));
	}

	void Accumulate (const dgVertexCollapseVertexMetric& p) 
	{
		elem[0] += p.elem[0]; 
		elem[1] += p.elem[1]; 
		elem[2] += p.elem[2]; 
		elem[3] += p.elem[3]; 
		elem[4] += p.elem[4]; 
		elem[5] += p.elem[5]; 
		elem[6] += p.elem[6]; 
		elem[7] += p.elem[7]; 
		elem[8] += p.elem[8]; 
		elem[9] += p.elem[9]; 
	}

	void Accumulate (const dgBigPlane& plane) 
	{
		elem[0] += plane.m_x * plane.m_x;  
		elem[1] += plane.m_y * plane.m_y;  
		elem[2] += plane.m_z * plane.m_z;  
		elem[3] += plane.m_w * plane.m_w;  

		elem[4] += hacd::HaF64 (2.0f) * plane.m_x * plane.m_y;  
		elem[5] += hacd::HaF64 (2.0f) * plane.m_x * plane.m_z;  
		elem[7] += hacd::HaF64 (2.0f) * plane.m_y * plane.m_z;  

		elem[6] += hacd::HaF64 (2.0f) * plane.m_x * plane.m_w;  
		elem[8] += hacd::HaF64 (2.0f) * plane.m_y * plane.m_w;  
		elem[9] += hacd::HaF64 (2.0f) * plane.m_z * plane.m_w;  
	}


	hacd::HaF64 Evalue (const dgVector &p) const 
	{
		hacd::HaF64 acc = elem[0] * p.m_x * p.m_x + elem[1] * p.m_y * p.m_y + elem[2] * p.m_z * p.m_z + 
						elem[4] * p.m_x * p.m_y + elem[5] * p.m_x * p.m_z + elem[7] * p.m_y * p.m_z + 
						elem[6] * p.m_x + elem[8] * p.m_y + elem[9] * p.m_z + elem[3];  
		return fabs (acc);
	}
};



dgPolyhedra::dgPolyhedra (void)
	:dgTree <dgEdge, hacd::HaI64>()
	,m_baseMark(0)
	,m_edgeMark(0)
	,m_faceSecuence(0)
{
}

dgPolyhedra::dgPolyhedra (const dgPolyhedra &polyhedra)
	:dgTree <dgEdge, hacd::HaI64>()
	,m_baseMark(0)
	,m_edgeMark(0)
	,m_faceSecuence(0)
{
	dgStack<hacd::HaI32> indexPool (1024 * 16);
	dgStack<hacd::HaU64> userPool (1024 * 16);
	hacd::HaI32* const index = &indexPool[0];
	hacd::HaU64* const user = &userPool[0];

	BeginFace ();
	Iterator iter(polyhedra);
	for (iter.Begin(); iter; iter ++) {
		dgEdge* const edge = &(*iter);
		if (edge->m_incidentFace < 0) {
			continue;
		}

		if (!FindEdge(edge->m_incidentVertex, edge->m_twin->m_incidentVertex))	{
			hacd::HaI32 indexCount = 0;
			dgEdge* ptr = edge;
			do {
				user[indexCount] = ptr->m_userData;
				index[indexCount] = ptr->m_incidentVertex;
				indexCount ++;
				ptr = ptr->m_next;
			} while (ptr != edge);

			dgEdge* const face = AddFace (indexCount, index, (hacd::HaI64*) user);
			ptr = face;
			do {
				ptr->m_incidentFace = edge->m_incidentFace;
				ptr = ptr->m_next;
			} while (ptr != face);
		}
	}
	EndFace();

	m_faceSecuence = polyhedra.m_faceSecuence;

#ifdef __ENABLE_SANITY_CHECK 
	HACD_ASSERT (SanityCheck());
#endif
}

dgPolyhedra::~dgPolyhedra ()
{
}


hacd::HaI32 dgPolyhedra::GetFaceCount() const
{
	Iterator iter (*this);
	hacd::HaI32 count = 0;
	hacd::HaI32 mark = IncLRU();
	for (iter.Begin(); iter; iter ++) {
		dgEdge* const edge = &(*iter);
		if (edge->m_mark == mark) {
			continue;
		}

		if (edge->m_incidentFace < 0) {
			continue;
		}

		count ++;
		dgEdge* ptr = edge;
		do {
			ptr->m_mark = mark;
			ptr = ptr->m_next;
		} while (ptr != edge);
	}
	return count;
}



dgEdge* dgPolyhedra::AddFace ( hacd::HaI32 count, const hacd::HaI32* const index, const hacd::HaI64* const userdata)
{
	class IntersectionFilter
	{
		public:
		IntersectionFilter ()
		{
			m_count = 0;
		}

		bool Insert (hacd::HaI32 dummy, hacd::HaI64 value)
		{
			hacd::HaI32 i;				
			for (i = 0 ; i < m_count; i ++) {
				if (m_array[i] == value) {
					return false;
				}
			}
			m_array[i] = value;
			m_count ++;
			return true;
		}

		hacd::HaI32 m_count;
		hacd::HaI64 m_array[2048];
	};

	IntersectionFilter selfIntersectingFaceFilter;

	hacd::HaI32 dummyValues = 0;
	hacd::HaI32 i0 = index[count-1];
	for (hacd::HaI32 i = 0; i < count; i ++) {
		hacd::HaI32 i1 = index[i];
		dgPairKey code0 (i0, i1);

		if (!selfIntersectingFaceFilter.Insert (dummyValues, code0.GetVal())) {
			return NULL;
		}

		dgPairKey code1 (i1, i0);
		if (!selfIntersectingFaceFilter.Insert (dummyValues, code1.GetVal())) {
			return NULL;
		}


		if (i0 == i1) {
			return NULL;
		}
		if (FindEdge (i0, i1)) {
			return NULL;
		}
		i0 = i1;
	}

	m_faceSecuence ++;

	i0 = index[count-1];
	hacd::HaI32 i1 = index[0];
	hacd::HaU64 udata0 = 0;
	hacd::HaU64 udata1 = 0;
	if (userdata) {
		udata0 = hacd::HaU64 (userdata[count-1]);
		udata1 = hacd::HaU64 (userdata[0]);
	} 

	bool state;
	dgPairKey code (i0, i1);
	dgEdge tmpEdge (i0, m_faceSecuence, udata0);
	dgTreeNode* node = Insert (tmpEdge, code.GetVal(), state); 
	HACD_ASSERT (!state);
	dgEdge* edge0 = &node->GetInfo();
	dgEdge* const first = edge0;

	for (hacd::HaI32 i = 1; i < count; i ++) {
		i0 = i1;
		i1 = index[i];
		udata0 = udata1;
		udata1 = hacd::HaU64 (userdata ? userdata[i] : 0);

		dgPairKey code (i0, i1);
		dgEdge tmpEdge (i0, m_faceSecuence, udata0);
		node = Insert (tmpEdge, code.GetVal(), state); 
		HACD_ASSERT (!state);

		dgEdge* const edge1 = &node->GetInfo();
		edge0->m_next = edge1;
		edge1->m_prev = edge0;
		edge0 = edge1;
	}

	first->m_prev = edge0;
	edge0->m_next = first;

	return first->m_next;
}


void dgPolyhedra::EndFace ()
{
	dgPolyhedra::Iterator iter (*this);

	// Connect all twin edge
	for (iter.Begin(); iter; iter ++) {
		dgEdge* const edge = &(*iter);
		if (!edge->m_twin) {
			edge->m_twin = FindEdge (edge->m_next->m_incidentVertex, edge->m_incidentVertex);
			if (edge->m_twin) {
				edge->m_twin->m_twin = edge; 
			}
		}
	}

#ifdef __ENABLE_SANITY_CHECK 
	HACD_ASSERT (SanityCheck());
#endif
	dgStack<dgEdge*> edgeArrayPool(GetCount() * 2 + 256);

	hacd::HaI32 edgeCount = 0;
	dgEdge** const edgeArray = &edgeArrayPool[0];
	for (iter.Begin(); iter; iter ++) {
		dgEdge* const edge = &(*iter);
		if (!edge->m_twin) {
			bool state;
			dgPolyhedra::dgPairKey code (edge->m_next->m_incidentVertex, edge->m_incidentVertex);
			dgEdge tmpEdge (edge->m_next->m_incidentVertex, -1);
			tmpEdge.m_incidentFace = -1; 
			dgPolyhedra::dgTreeNode* const node = Insert (tmpEdge, code.GetVal(), state); 
			HACD_ASSERT (!state);
			edge->m_twin = &node->GetInfo();
			edge->m_twin->m_twin = edge; 
			edgeArray[edgeCount] = edge->m_twin;
			edgeCount ++;
		}
	}

	for (hacd::HaI32 i = 0; i < edgeCount; i ++) {
		dgEdge* const edge = edgeArray[i];
		HACD_ASSERT (!edge->m_prev);
		dgEdge *ptr = edge->m_twin;
		for (; ptr->m_next; ptr = ptr->m_next->m_twin){}
		ptr->m_next = edge;
		edge->m_prev = ptr;
	}

#ifdef __ENABLE_SANITY_CHECK 
	HACD_ASSERT (SanityCheck ());
#endif
}


void dgPolyhedra::DeleteFace(dgEdge* const face)
{
	dgEdge* edgeList[1024 * 16];

	if (face->m_incidentFace > 0) {
		hacd::HaI32 count = 0;
		dgEdge* ptr = face;
		do {
			ptr->m_incidentFace = -1;
			hacd::HaI32 i = 0;
			for (; i < count; i ++) {
				if ((edgeList[i] == ptr) || (edgeList[i]->m_twin == ptr)) {
					break;
				}
			}
			if (i == count) {
				edgeList[count] = ptr;
				count ++;
			}
			ptr = ptr->m_next;
		} while (ptr != face);


		for (hacd::HaI32 i = 0; i < count; i ++) {
			dgEdge* const ptr = edgeList[i];
			if (ptr->m_twin->m_incidentFace < 0) {
				DeleteEdge (ptr);
			}
		}
	}
}



dgBigVector dgPolyhedra::FaceNormal (dgEdge* const face, const hacd::HaF64* const pool, hacd::HaI32 strideInBytes) const
{
	hacd::HaI32 stride = strideInBytes / sizeof (hacd::HaF64);
	dgEdge* edge = face;
	dgBigVector p0 (&pool[edge->m_incidentVertex * stride]);
	edge = edge->m_next;
	dgBigVector p1 (&pool[edge->m_incidentVertex * stride]);
	dgBigVector e1 (p1 - p0);

	dgBigVector normal (hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f));
	for (edge = edge->m_next; edge != face; edge = edge->m_next) {
		dgBigVector p2 (&pool[edge->m_incidentVertex * stride]);
		dgBigVector e2 (p2 - p0);
		normal += e1 * e2;
		e1 = e2;
	} 
	return normal;
}


dgEdge* dgPolyhedra::AddHalfEdge (hacd::HaI32 v0, hacd::HaI32 v1)
{
	if (v0 != v1) {
		dgPairKey pairKey (v0, v1);
		dgEdge tmpEdge (v0, -1);

		dgTreeNode* node = Insert (tmpEdge, pairKey.GetVal()); 
		return node ? &node->GetInfo() : NULL;
	} else {
		return NULL;
	}
}


void dgPolyhedra::DeleteEdge (dgEdge* const edge)
{
	dgEdge *const twin = edge->m_twin;

	edge->m_prev->m_next = twin->m_next;
	twin->m_next->m_prev = edge->m_prev;
	edge->m_next->m_prev = twin->m_prev;
	twin->m_prev->m_next = edge->m_next;

	dgTreeNode *const nodeA = GetNodeFromInfo (*edge);
	dgTreeNode *const nodeB = GetNodeFromInfo (*twin);

	HACD_ASSERT (&nodeA->GetInfo() == edge);
	HACD_ASSERT (&nodeB->GetInfo() == twin);

	Remove (nodeA);
	Remove (nodeB);
}


dgEdge* dgPolyhedra::SpliteEdge (hacd::HaI32 newIndex,	dgEdge* const edge)
{
	dgEdge* const edge00 = edge->m_prev;
	dgEdge* const edge01 = edge->m_next;
	dgEdge* const twin00 = edge->m_twin->m_next;
	dgEdge* const twin01 = edge->m_twin->m_prev;

	hacd::HaI32 i0 = edge->m_incidentVertex;
	hacd::HaI32 i1 = edge->m_twin->m_incidentVertex;

	hacd::HaI32 f0 = edge->m_incidentFace;
	hacd::HaI32 f1 = edge->m_twin->m_incidentFace;

	DeleteEdge (edge);

	dgEdge* const edge0 = AddHalfEdge (i0, newIndex);
	dgEdge* const edge1 = AddHalfEdge (newIndex, i1);

	dgEdge* const twin0 = AddHalfEdge (newIndex, i0);
	dgEdge* const twin1 = AddHalfEdge (i1, newIndex);
	HACD_ASSERT (edge0);
	HACD_ASSERT (edge1);
	HACD_ASSERT (twin0);
	HACD_ASSERT (twin1);

	edge0->m_twin = twin0;
	twin0->m_twin = edge0;

	edge1->m_twin = twin1;
	twin1->m_twin = edge1;

	edge0->m_next = edge1;
	edge1->m_prev = edge0;

	twin1->m_next = twin0;
	twin0->m_prev = twin1;

	edge0->m_prev = edge00;
	edge00 ->m_next = edge0;

	edge1->m_next = edge01;
	edge01->m_prev = edge1;

	twin0->m_next = twin00;
	twin00->m_prev = twin0;

	twin1->m_prev = twin01;
	twin01->m_next = twin1;

	edge0->m_incidentFace = f0;
	edge1->m_incidentFace = f0;

	twin0->m_incidentFace = f1;
	twin1->m_incidentFace = f1;

#ifdef __ENABLE_SANITY_CHECK 
	//	HACD_ASSERT (SanityCheck ());
#endif

	return edge0;
}



bool dgPolyhedra::FlipEdge (dgEdge* const edge)
{
	//	dgTreeNode *node;
	if (edge->m_next->m_next->m_next != edge) {
		return false;
	}

	if (edge->m_twin->m_next->m_next->m_next != edge->m_twin) {
		return false;
	}

	if (FindEdge(edge->m_prev->m_incidentVertex, edge->m_twin->m_prev->m_incidentVertex)) {
		return false;
	}

	dgEdge *const prevEdge = edge->m_prev;
	dgEdge *const prevTwin = edge->m_twin->m_prev;

	dgPairKey edgeKey (prevTwin->m_incidentVertex, prevEdge->m_incidentVertex);
	dgPairKey twinKey (prevEdge->m_incidentVertex, prevTwin->m_incidentVertex);

	ReplaceKey (GetNodeFromInfo (*edge), edgeKey.GetVal());
	//	HACD_ASSERT (node);

	ReplaceKey (GetNodeFromInfo (*edge->m_twin), twinKey.GetVal());
	//	HACD_ASSERT (node);

	edge->m_incidentVertex = prevTwin->m_incidentVertex;
	edge->m_twin->m_incidentVertex = prevEdge->m_incidentVertex;

	edge->m_userData = prevTwin->m_userData;
	edge->m_twin->m_userData = prevEdge->m_userData;

	prevEdge->m_next = edge->m_twin->m_next;
	prevTwin->m_prev->m_prev = edge->m_prev;

	prevTwin->m_next = edge->m_next;
	prevEdge->m_prev->m_prev = edge->m_twin->m_prev;

	edge->m_prev = prevTwin->m_prev;
	edge->m_next = prevEdge;

	edge->m_twin->m_prev = prevEdge->m_prev;
	edge->m_twin->m_next = prevTwin;

	prevTwin->m_prev->m_next = edge;
	prevTwin->m_prev = edge->m_twin;

	prevEdge->m_prev->m_next = edge->m_twin;
	prevEdge->m_prev = edge;

	edge->m_next->m_incidentFace = edge->m_incidentFace;
	edge->m_prev->m_incidentFace = edge->m_incidentFace;

	edge->m_twin->m_next->m_incidentFace = edge->m_twin->m_incidentFace;
	edge->m_twin->m_prev->m_incidentFace = edge->m_twin->m_incidentFace;


#ifdef __ENABLE_SANITY_CHECK 
	HACD_ASSERT (SanityCheck ());
#endif

	return true;
}



bool dgPolyhedra::GetConectedSurface (dgPolyhedra &polyhedra) const
{
	if (!GetCount()) {
		return false;
	}

	dgEdge* edge = NULL;
	Iterator iter(*this);
	for (iter.Begin (); iter; iter ++) {
		edge = &(*iter);
		if ((edge->m_mark < m_baseMark) && (edge->m_incidentFace > 0)) {
			break;
		}
	}

	if (!iter) {
		return false;
	}

	hacd::HaI32 faceIndex[4096];
	hacd::HaI64 faceDataIndex[4096];
	dgStack<dgEdge*> stackPool (GetCount()); 
	dgEdge** const stack = &stackPool[0];

	hacd::HaI32 mark = IncLRU();

	polyhedra.BeginFace ();
	stack[0] = edge;
	hacd::HaI32 index = 1;
	while (index) {
		index --;
		dgEdge* const edge = stack[index];

		if (edge->m_mark == mark) {
			continue;
		}

		hacd::HaI32 count = 0;
		dgEdge* ptr = edge;
		do {
			ptr->m_mark = mark;
			faceIndex[count] = ptr->m_incidentVertex;
			faceDataIndex[count] = hacd::HaI64 (ptr->m_userData);
			count ++;
			HACD_ASSERT (count <  hacd::HaI32 ((sizeof (faceIndex)/sizeof(faceIndex[0]))));

			if ((ptr->m_twin->m_incidentFace > 0) && (ptr->m_twin->m_mark != mark)) {
				stack[index] = ptr->m_twin;
				index ++;
				HACD_ASSERT (index < GetCount());
			}

			ptr = ptr->m_next;
		} while (ptr != edge);

		polyhedra.AddFace (count, &faceIndex[0], &faceDataIndex[0]);
	}

	polyhedra.EndFace ();

	return true;
}


void dgPolyhedra::ChangeEdgeIncidentVertex (dgEdge* const edge, hacd::HaI32 newIndex)
{
	dgEdge* ptr = edge;
	do {
		dgTreeNode* node = GetNodeFromInfo(*ptr);
		dgPairKey Key0 (newIndex, ptr->m_twin->m_incidentVertex);
		ReplaceKey (node, Key0.GetVal());

		node = GetNodeFromInfo(*ptr->m_twin);
		dgPairKey Key1 (ptr->m_twin->m_incidentVertex, newIndex);
		ReplaceKey (node, Key1.GetVal());

		ptr->m_incidentVertex = newIndex;

		ptr = ptr->m_twin->m_next;
	} while (ptr != edge);
}


void dgPolyhedra::DeleteDegenerateFaces (const hacd::HaF64* const pool, hacd::HaI32 strideInBytes, hacd::HaF64 area)
{
	if (!GetCount()) {
		return;
	}

#ifdef __ENABLE_SANITY_CHECK 
	HACD_ASSERT (SanityCheck ());
#endif
	dgStack <dgPolyhedra::dgTreeNode*> faceArrayPool(GetCount() / 2 + 100);

	hacd::HaI32 count = 0;
	dgPolyhedra::dgTreeNode** const faceArray = &faceArrayPool[0];
	hacd::HaI32 mark = IncLRU();
	Iterator iter (*this);
	for (iter.Begin(); iter; iter ++) {
		dgEdge* const edge = &(*iter);

		if ((edge->m_mark != mark) && (edge->m_incidentFace > 0)) {
			faceArray[count] = iter.GetNode();
			count ++;
			dgEdge* ptr = edge;
			do	{
				ptr->m_mark = mark;
				ptr = ptr->m_next;
			} while (ptr != edge);
		}
	}

	hacd::HaF64 area2 = area * area;
	area2 *= hacd::HaF64 (4.0f);

	for (hacd::HaI32 i = 0; i < count; i ++) {
		dgPolyhedra::dgTreeNode* const faceNode = faceArray[i];
		dgEdge* const edge = &faceNode->GetInfo();

		dgBigVector normal (FaceNormal (edge, pool, strideInBytes));

		hacd::HaF64 faceArea = normal % normal;
		if (faceArea < area2) {
			DeleteFace (edge);
		}
	}

#ifdef __ENABLE_SANITY_CHECK 
	mark = IncLRU();
	for (iter.Begin(); iter; iter ++) {
		dgEdge* const edge = &(*iter);
		if ((edge->m_mark != mark) && (edge->m_incidentFace > 0)) {
			//HACD_ASSERT (edge->m_next->m_next->m_next == edge);
			dgEdge* ptr = edge;
			do	{
				ptr->m_mark = mark;
				ptr = ptr->m_next;
			} while (ptr != edge);

			dgBigVector normal (FaceNormal (edge, pool, strideInBytes));

			hacd::HaF64 faceArea = normal % normal;
			HACD_ASSERT (faceArea >= area2);
		}
	}
	HACD_ASSERT (SanityCheck ());
#endif
}


static void NormalizeVertex (hacd::HaI32 count, dgBigVector* const dst, const hacd::HaF64* const src, hacd::HaI32 stride)
{
//	dgBigVector min;
//	dgBigVector max;
//	GetMinMax (min, max, src, count, hacd::HaI32 (stride * sizeof (hacd::HaF64)));
//	dgBigVector centre (hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f));
	for (hacd::HaI32 i = 0; i < count; i ++) {
//		dst[i].m_x = centre.m_x + src[i * stride + 0];
//		dst[i].m_y = centre.m_y + src[i * stride + 1];
//		dst[i].m_z = centre.m_z + src[i * stride + 2];
		dst[i].m_x = src[i * stride + 0];
		dst[i].m_y = src[i * stride + 1];
		dst[i].m_z = src[i * stride + 2];
		dst[i].m_w= hacd::HaF64 (0.0f);
	}
}

static dgBigPlane EdgePlane (hacd::HaI32 i0, hacd::HaI32 i1, hacd::HaI32 i2, const dgBigVector* const pool)
{
	const dgBigVector& p0 = pool[i0];
	const dgBigVector& p1 = pool[i1];
	const dgBigVector& p2 = pool[i2];

	dgBigPlane plane (p0, p1, p2);
	hacd::HaF64 mag = sqrt (plane % plane);
	if (mag < hacd::HaF64 (1.0e-12f)) {
		mag = hacd::HaF64 (1.0e-12f);
	}
	mag = hacd::HaF64 (1.0f) / mag;

	plane.m_x *= mag;
	plane.m_y *= mag;
	plane.m_z *= mag;
	plane.m_w *= mag;

	return plane;
}


static dgBigPlane UnboundedLoopPlane (hacd::HaI32 i0, hacd::HaI32 i1, hacd::HaI32 i2, const dgBigVector* const pool)
{
	const dgBigVector p0 = pool[i0];
	const dgBigVector p1 = pool[i1];
	const dgBigVector p2 = pool[i2];
	dgBigVector E0 (p1 - p0); 
	dgBigVector E1 (p2 - p0); 

	dgBigVector N ((E0 * E1) * E0); 
	hacd::HaF64 dist = - (N % p0);
	dgBigPlane plane (N, dist);

	hacd::HaF64 mag = sqrt (plane % plane);
	if (mag < hacd::HaF64 (1.0e-12f)) {
		mag = hacd::HaF64 (1.0e-12f);
	}
	mag = hacd::HaF64 (10.0f) / mag;

	plane.m_x *= mag;
	plane.m_y *= mag;
	plane.m_z *= mag;
	plane.m_w *= mag;

	return plane;
}


static void CalculateAllMetrics (const dgPolyhedra* const polyhedra, dgVertexCollapseVertexMetric* const table, const dgBigVector* const pool)
{
	hacd::HaI32 edgeMark = polyhedra->IncLRU();
	dgPolyhedra::Iterator iter (*polyhedra);
	for (iter.Begin(); iter; iter ++) {
		dgEdge* const edge = &(*iter);

		HACD_ASSERT (edge);
		if (edge->m_mark != edgeMark) {

			if (edge->m_incidentFace > 0) {
				hacd::HaI32 i0 = edge->m_incidentVertex;
				hacd::HaI32 i1 = edge->m_next->m_incidentVertex;
				hacd::HaI32 i2 = edge->m_prev->m_incidentVertex;

				dgBigPlane constrainPlane (EdgePlane (i0, i1, i2, pool));
				dgVertexCollapseVertexMetric tmp (constrainPlane);

				dgEdge* ptr = edge;
				do {
					ptr->m_mark = edgeMark;
					i0 = ptr->m_incidentVertex;
					table[i0].Accumulate(tmp);

					ptr = ptr->m_next;
				} while (ptr != edge);

			} else {
				HACD_ASSERT (edge->m_twin->m_incidentFace > 0);
				hacd::HaI32 i0 = edge->m_twin->m_incidentVertex;
				hacd::HaI32 i1 = edge->m_twin->m_next->m_incidentVertex;
				hacd::HaI32 i2 = edge->m_twin->m_prev->m_incidentVertex;

				edge->m_mark = edgeMark;
				dgBigPlane constrainPlane (UnboundedLoopPlane (i0, i1, i2, pool));
				dgVertexCollapseVertexMetric tmp (constrainPlane);

				i0 = edge->m_incidentVertex;
				table[i0].Accumulate(tmp);

				i0 = edge->m_twin->m_incidentVertex;
				table[i0].Accumulate(tmp);
			}
		}
	}
}


hacd::HaF64 dgPolyhedra::EdgePenalty (const dgBigVector* const pool, dgEdge* const edge) const
{
	hacd::HaI32 i0 = edge->m_incidentVertex;
	hacd::HaI32 i1 = edge->m_next->m_incidentVertex;

	const dgBigVector& p0 = pool[i0];
	const dgBigVector& p1 = pool[i1];
	dgBigVector dp (p1 - p0);

	hacd::HaF64 dot = dp % dp;
	if (dot < hacd::HaF64(1.0e-6f)) {
		return hacd::HaF64 (-1.0f);
	}

	if ((edge->m_incidentFace > 0) && (edge->m_twin->m_incidentFace > 0)) {
		dgBigVector edgeNormal (FaceNormal (edge, &pool[0].m_x, sizeof (dgBigVector)));
		dgBigVector twinNormal (FaceNormal (edge->m_twin, &pool[0].m_x, sizeof (dgBigVector)));

		hacd::HaF64 mag0 = edgeNormal % edgeNormal;
		hacd::HaF64 mag1 = twinNormal % twinNormal;
		if ((mag0 < hacd::HaF64 (1.0e-24f)) || (mag1 < hacd::HaF64 (1.0e-24f))) {
			return hacd::HaF64 (-1.0f);
		}

		edgeNormal = edgeNormal.Scale (hacd::HaF64 (1.0f) / sqrt(mag0));
		twinNormal = twinNormal.Scale (hacd::HaF64 (1.0f) / sqrt(mag1));

		dot = edgeNormal % twinNormal;
		if (dot < hacd::HaF64 (-0.9f)) {
			return hacd::HaF32 (-1.0f);
		}

		dgEdge* ptr = edge;
		do {
			if ((ptr->m_incidentFace <= 0) || (ptr->m_twin->m_incidentFace <= 0)){
				dgEdge* const adj = edge->m_twin;
				ptr = edge;
				do {
					if ((ptr->m_incidentFace <= 0) || (ptr->m_twin->m_incidentFace <= 0)){
						return hacd::HaF32 (-1.0);
					}
					ptr = ptr->m_twin->m_next;
				} while (ptr != adj);
			}
			ptr = ptr->m_twin->m_next;
		} while (ptr != edge);
	}

	hacd::HaI32 faceA = edge->m_incidentFace;
	hacd::HaI32 faceB = edge->m_twin->m_incidentFace;

	i0 = edge->m_twin->m_incidentVertex;
	dgBigVector p (pool[i0].m_x, pool[i0].m_y, pool[i0].m_z, hacd::HaF32 (0.0f));

	bool penalty = false;
	dgEdge* ptr = edge;
	do {
		dgEdge* const adj = ptr->m_twin;

		hacd::HaI32 face = adj->m_incidentFace;
		if ((face != faceB) && (face != faceA) && (face >= 0) && (adj->m_next->m_incidentFace == face) && (adj->m_prev->m_incidentFace == face)){

			hacd::HaI32 i0 = adj->m_next->m_incidentVertex;
			const dgBigVector& p0 = pool[i0];

			hacd::HaI32 i1 = adj->m_incidentVertex;
			const dgBigVector& p1 = pool[i1];

			hacd::HaI32 i2 = adj->m_prev->m_incidentVertex;
			const dgBigVector& p2 = pool[i2];

			dgBigVector n0 ((p1 - p0) * (p2 - p0));
			dgBigVector n1 ((p1 - p) * (p2 - p));

//			hacd::HaF64 mag0 = n0 % n0;
//			HACD_ASSERT (mag0 > hacd::HaF64(1.0e-16f));
//			mag0 = sqrt (mag0);

//			hacd::HaF64 mag1 = n1 % n1;
//			mag1 = sqrt (mag1);

			hacd::HaF64 dot = n0 % n1;
			if (dot < hacd::HaF64 (0.0f)) {
//			if (dot <= (mag0 * mag1 * hacd::HaF32 (0.707f)) || (mag0 > (hacd::HaF64(16.0f) * mag1))) {
				penalty = true;
				break;
			}
		}

		ptr = ptr->m_twin->m_next;
	} while (ptr != edge);

	hacd::HaF64 aspect = hacd::HaF32 (-1.0f);
	if (!penalty) {
		hacd::HaI32 i0 = edge->m_twin->m_incidentVertex;
		dgBigVector p0 (pool[i0]);

		aspect = hacd::HaF32 (1.0f);
		for (dgEdge* ptr = edge->m_twin->m_next->m_twin->m_next; ptr != edge; ptr = ptr->m_twin->m_next) {
			if (ptr->m_incidentFace > 0) {
				hacd::HaI32 i0 = ptr->m_next->m_incidentVertex;
				const dgBigVector& p1 = pool[i0];

				hacd::HaI32 i1 = ptr->m_prev->m_incidentVertex;
				const dgBigVector& p2 = pool[i1];

				dgBigVector e0 (p1 - p0);
				dgBigVector e1 (p2 - p1);
				dgBigVector e2 (p0 - p2);

				hacd::HaF64 mag0 = e0 % e0;
				hacd::HaF64 mag1 = e1 % e1;
				hacd::HaF64 mag2 = e2 % e2;
				hacd::HaF64 maxMag = GetMax (mag0, mag1, mag2);
				hacd::HaF64 minMag = GetMin (mag0, mag1, mag2);
				hacd::HaF64 ratio = minMag / maxMag;

				if (ratio < aspect) {
					aspect = ratio;
				}
			}
		}
		aspect = sqrt (aspect);
		//aspect = 1.0f;
	}

	return aspect;
}

static void CalculateVertexMetrics (dgVertexCollapseVertexMetric table[], const dgBigVector* const pool, dgEdge* const edge)
{
	hacd::HaI32 i0 = edge->m_incidentVertex;

//	const dgBigVector& p0 = pool[i0];
	table[i0].Clear ();
	dgEdge* ptr = edge;
	do {

		if (ptr->m_incidentFace > 0) {
			hacd::HaI32 i1 = ptr->m_next->m_incidentVertex;
			hacd::HaI32 i2 = ptr->m_prev->m_incidentVertex;
			dgBigPlane constrainPlane (EdgePlane (i0, i1, i2, pool));
			table[i0].Accumulate (constrainPlane);

		} else {
			hacd::HaI32 i1 = ptr->m_twin->m_incidentVertex;
			hacd::HaI32 i2 = ptr->m_twin->m_prev->m_incidentVertex;
			dgBigPlane constrainPlane (UnboundedLoopPlane (i0, i1, i2, pool));
			table[i0].Accumulate (constrainPlane);

			i1 = ptr->m_prev->m_incidentVertex;
			i2 = ptr->m_prev->m_twin->m_prev->m_incidentVertex;
			constrainPlane = UnboundedLoopPlane (i0, i1, i2, pool);
			table[i0].Accumulate (constrainPlane);
		}

		ptr = ptr->m_twin->m_next;
	} while (ptr != edge);
}

static void RemoveHalfEdge (dgPolyhedra* const polyhedra, dgEdge* const edge)
{
	dgEdgeCollapseEdgeHandle* const handle = (dgEdgeCollapseEdgeHandle *) IntToPointer (edge->m_userData);
	if (handle) { 
		handle->m_edge = NULL;
	}

	dgPolyhedra::dgTreeNode* const node = polyhedra->GetNodeFromInfo(*edge);
	HACD_ASSERT (node);
	polyhedra->Remove (node);
}


static dgEdge* CollapseEdge(dgPolyhedra* const polyhedra, dgEdge* const edge)
{
	hacd::HaI32 v0 = edge->m_incidentVertex;
	hacd::HaI32 v1 = edge->m_twin->m_incidentVertex;

#ifdef __ENABLE_SANITY_CHECK 
	dgPolyhedra::dgPairKey TwinKey (v1, v0);
	dgPolyhedra::dgTreeNode* const node = polyhedra->Find (TwinKey.GetVal());
	dgEdge* const twin1 = node ? &node->GetInfo() : NULL;
	HACD_ASSERT (twin1);
	HACD_ASSERT (edge->m_twin == twin1);
	HACD_ASSERT (twin1->m_twin == edge);
	HACD_ASSERT (edge->m_incidentFace != 0);
	HACD_ASSERT (twin1->m_incidentFace != 0);
#endif


	dgEdge* retEdge = edge->m_twin->m_prev->m_twin;
	if (retEdge	== edge->m_twin->m_next) {
		return NULL;
	}
	if (retEdge	== edge->m_twin) {
		return NULL;
	}
	if (retEdge	== edge->m_next) {
		retEdge = edge->m_prev->m_twin;
		if (retEdge	== edge->m_twin->m_next) {
			return NULL;
		}
		if (retEdge	== edge->m_twin) {
			return NULL;
		}
	}

	dgEdge* lastEdge = NULL;
	dgEdge* firstEdge = NULL;
	if ((edge->m_incidentFace >= 0)	&& (edge->m_twin->m_incidentFace >= 0)) {	
		lastEdge = edge->m_prev->m_twin;
		firstEdge = edge->m_twin->m_next->m_twin->m_next;
	} else if (edge->m_twin->m_incidentFace >= 0) {
		firstEdge = edge->m_twin->m_next->m_twin->m_next;
		lastEdge = edge;
	} else {
		lastEdge = edge->m_prev->m_twin;
		firstEdge = edge->m_twin->m_next;
	}

	for (dgEdge* ptr = firstEdge; ptr != lastEdge; ptr = ptr->m_twin->m_next) {
		dgEdge* badEdge = polyhedra->FindEdge (edge->m_twin->m_incidentVertex, ptr->m_twin->m_incidentVertex);
		if (badEdge) {
			return NULL;
		}
	} 

	dgEdge* const twin = edge->m_twin;
	if (twin->m_next == twin->m_prev->m_prev) {
		twin->m_prev->m_twin->m_twin = twin->m_next->m_twin;
		twin->m_next->m_twin->m_twin = twin->m_prev->m_twin;

		RemoveHalfEdge (polyhedra, twin->m_prev);
		RemoveHalfEdge (polyhedra, twin->m_next);
	} else {
		twin->m_next->m_prev = twin->m_prev;
		twin->m_prev->m_next = twin->m_next;
	}

	if (edge->m_next == edge->m_prev->m_prev) {
		edge->m_next->m_twin->m_twin = edge->m_prev->m_twin;
		edge->m_prev->m_twin->m_twin = edge->m_next->m_twin;
		RemoveHalfEdge (polyhedra, edge->m_next);
		RemoveHalfEdge (polyhedra, edge->m_prev);
	} else {
		edge->m_next->m_prev = edge->m_prev;
		edge->m_prev->m_next = edge->m_next;
	}

	HACD_ASSERT (twin->m_twin->m_incidentVertex == v0);
	HACD_ASSERT (edge->m_twin->m_incidentVertex == v1);
	RemoveHalfEdge (polyhedra, twin);
	RemoveHalfEdge (polyhedra, edge);

	dgEdge* ptr = retEdge;
	do {
		dgPolyhedra::dgPairKey pairKey (v0, ptr->m_twin->m_incidentVertex);

		dgPolyhedra::dgTreeNode* node = polyhedra->Find (pairKey.GetVal());
		if (node) {
			if (&node->GetInfo() == ptr) {
				dgPolyhedra::dgPairKey key (v1, ptr->m_twin->m_incidentVertex);
				ptr->m_incidentVertex = v1;
				node = polyhedra->ReplaceKey (node, key.GetVal());
				HACD_ASSERT (node);
			} 
		}

		dgPolyhedra::dgPairKey TwinKey (ptr->m_twin->m_incidentVertex, v0);
		node = polyhedra->Find (TwinKey.GetVal());
		if (node) {
			if (&node->GetInfo() == ptr->m_twin) {
				dgPolyhedra::dgPairKey key (ptr->m_twin->m_incidentVertex, v1);
				node = polyhedra->ReplaceKey (node, key.GetVal());
				HACD_ASSERT (node);
			}
		}

		ptr = ptr->m_twin->m_next;
	} while (ptr != retEdge);

	return retEdge;
}



void dgPolyhedra::Optimize (const hacd::HaF64* const array, hacd::HaI32 strideInBytes, hacd::HaF64 tol)
{
	dgList <dgEdgeCollapseEdgeHandle>::dgListNode *handleNodePtr;

	hacd::HaI32 stride = hacd::HaI32 (strideInBytes / sizeof (hacd::HaF64));

#ifdef __ENABLE_SANITY_CHECK 
	HACD_ASSERT (SanityCheck ());
#endif

	hacd::HaI32 edgeCount = GetEdgeCount() * 4 + 1024 * 16;
	hacd::HaI32 maxVertexIndex = GetLastVertexIndex();

	dgStack<dgBigVector> vertexPool (maxVertexIndex); 
	dgStack<dgVertexCollapseVertexMetric> vertexMetrics (maxVertexIndex + 512); 

	dgList <dgEdgeCollapseEdgeHandle> edgeHandleList;
	dgStack<char> heapPool (2 * edgeCount * hacd::HaI32 (sizeof (hacd::HaF64) + sizeof (dgEdgeCollapseEdgeHandle*) + sizeof (hacd::HaI32))); 
	dgDownHeap<dgList <dgEdgeCollapseEdgeHandle>::dgListNode* , hacd::HaF64> bigHeapArray(&heapPool[0], heapPool.GetSizeInBytes());

	NormalizeVertex (maxVertexIndex, &vertexPool[0], array, stride);
	memset (&vertexMetrics[0], 0, maxVertexIndex * sizeof (dgVertexCollapseVertexMetric));
	CalculateAllMetrics (this, &vertexMetrics[0], &vertexPool[0]);


	hacd::HaF64 tol2 = tol * tol;
	Iterator iter (*this);
	for (iter.Begin(); iter; iter ++) {
		dgEdge* const edge = &(*iter);

		edge->m_userData = 0;
		hacd::HaI32 index0 = edge->m_incidentVertex;
		hacd::HaI32 index1 = edge->m_twin->m_incidentVertex;

		dgVertexCollapseVertexMetric &metric = vertexMetrics[index0];
		dgVector p	(&vertexPool[index1].m_x);
		hacd::HaF64 cost = metric.Evalue (p); 
		if (cost < tol2) {
			cost = EdgePenalty (&vertexPool[0], edge);

			if (cost > hacd::HaF64 (0.0f)) {
				dgEdgeCollapseEdgeHandle handle (edge);
				handleNodePtr = edgeHandleList.Addtop (handle);
				bigHeapArray.Push (handleNodePtr, cost);
			}
		}
	}


	while (bigHeapArray.GetCount()) {
		handleNodePtr = bigHeapArray[0];

		dgEdge* edge = handleNodePtr->GetInfo().m_edge;
		bigHeapArray.Pop();
		edgeHandleList.Remove (handleNodePtr);

		if (edge) {
			CalculateVertexMetrics (&vertexMetrics[0], &vertexPool[0], edge);

			hacd::HaI32 index0 = edge->m_incidentVertex;
			hacd::HaI32 index1 = edge->m_twin->m_incidentVertex;
			dgVertexCollapseVertexMetric &metric = vertexMetrics[index0];
			dgBigVector p (vertexPool[index1]);

			if ((metric.Evalue (p) < tol2) && (EdgePenalty (&vertexPool[0], edge)  > hacd::HaF64 (0.0f))) {

#ifdef __ENABLE_SANITY_CHECK 
				HACD_ASSERT (SanityCheck ());
#endif

				edge = CollapseEdge(this, edge);

#ifdef __ENABLE_SANITY_CHECK 
				HACD_ASSERT (SanityCheck ());
#endif
				if (edge) {
					// Update vertex metrics
					CalculateVertexMetrics (&vertexMetrics[0], &vertexPool[0], edge);

					// Update metrics for all surrounding vertex
					dgEdge* ptr = edge;
					do {
						CalculateVertexMetrics (&vertexMetrics[0], &vertexPool[0], ptr->m_twin);
						ptr = ptr->m_twin->m_next;
					} while (ptr != edge);

					// calculate edge cost of all incident edges
					hacd::HaI32 mark = IncLRU();
					ptr = edge;
					do {
						HACD_ASSERT (ptr->m_mark != mark);
						ptr->m_mark = mark;

						index0 = ptr->m_incidentVertex;
						index1 = ptr->m_twin->m_incidentVertex;

						dgVertexCollapseVertexMetric &metric = vertexMetrics[index0];
						dgBigVector p (vertexPool[index1]);

						hacd::HaF64 cost = hacd::HaF32 (-1.0f);
						if (metric.Evalue (p) < tol2) {
							cost = EdgePenalty (&vertexPool[0], ptr);
						}

						if (cost  > hacd::HaF64 (0.0f)) {
							dgEdgeCollapseEdgeHandle handle (ptr);
							handleNodePtr = edgeHandleList.Addtop (handle);
							bigHeapArray.Push (handleNodePtr, cost);
						} else {
							dgEdgeCollapseEdgeHandle* const handle = (dgEdgeCollapseEdgeHandle*)IntToPointer (ptr->m_userData);
							if (handle) {
								handle->m_edge = NULL;
							}
							ptr->m_userData = hacd::HaU32 (NULL);

						}

						ptr = ptr->m_twin->m_next;
					} while (ptr != edge);


					// calculate edge cost of all incident edges to a surrounding vertex
					ptr = edge;
					do {
						dgEdge* const incidentEdge = ptr->m_twin;		

						dgEdge* ptr1 = incidentEdge;
						do {
							index0 = ptr1->m_incidentVertex;
							index1 = ptr1->m_twin->m_incidentVertex;

							if (ptr1->m_mark != mark) {
								ptr1->m_mark = mark;
								dgVertexCollapseVertexMetric &metric = vertexMetrics[index0];
								dgBigVector p (vertexPool[index1]);

								hacd::HaF64 cost = hacd::HaF32 (-1.0f);
								if (metric.Evalue (p) < tol2) {
									cost = EdgePenalty (&vertexPool[0], ptr1);
								}

								if (cost  > hacd::HaF64 (0.0f)) {
									HACD_ASSERT (cost > hacd::HaF64(0.0f));
									dgEdgeCollapseEdgeHandle handle (ptr1);
									handleNodePtr = edgeHandleList.Addtop (handle);
									bigHeapArray.Push (handleNodePtr, cost);
								} else {
									dgEdgeCollapseEdgeHandle *handle;
									handle = (dgEdgeCollapseEdgeHandle*)IntToPointer (ptr1->m_userData);
									if (handle) {
										handle->m_edge = NULL;
									}
									ptr1->m_userData = hacd::HaU32 (NULL);

								}
							}

							if (ptr1->m_twin->m_mark != mark) {
								ptr1->m_twin->m_mark = mark;
								dgVertexCollapseVertexMetric &metric = vertexMetrics[index1];
								dgBigVector p (vertexPool[index0]);

								hacd::HaF64 cost = hacd::HaF32 (-1.0f);
								if (metric.Evalue (p) < tol2) {
									cost = EdgePenalty (&vertexPool[0], ptr1->m_twin);
								}

								if (cost  > hacd::HaF64 (0.0f)) {
									HACD_ASSERT (cost > hacd::HaF64(0.0f));
									dgEdgeCollapseEdgeHandle handle (ptr1->m_twin);
									handleNodePtr = edgeHandleList.Addtop (handle);
									bigHeapArray.Push (handleNodePtr, cost);
								} else {
									dgEdgeCollapseEdgeHandle *handle;
									handle = (dgEdgeCollapseEdgeHandle*) IntToPointer (ptr1->m_twin->m_userData);
									if (handle) {
										handle->m_edge = NULL;
									}
									ptr1->m_twin->m_userData = hacd::HaU32 (NULL);

								}
							}

							ptr1 = ptr1->m_twin->m_next;
						} while (ptr1 != incidentEdge);

						ptr = ptr->m_twin->m_next;
					} while (ptr != edge);
				}
			}
		}
	}
}


dgEdge* dgPolyhedra::FindEarTip (dgEdge* const face, const hacd::HaF64* const pool, hacd::HaI32 stride, dgDownHeap<dgEdge*, hacd::HaF64>& heap, const dgBigVector &normal) const
{
	dgEdge* ptr = face;
	dgBigVector p0 (&pool[ptr->m_prev->m_incidentVertex * stride]);
	dgBigVector p1 (&pool[ptr->m_incidentVertex * stride]);
	dgBigVector d0 (p1 - p0);
	hacd::HaF64 f = sqrt (d0 % d0);
	if (f < hacd::HaF64 (1.0e-10f)) {
		f = hacd::HaF64 (1.0e-10f);
	}
	d0 = d0.Scale (hacd::HaF64 (1.0f) / f);

	hacd::HaF64 minAngle = hacd::HaF32 (10.0f);
	do {
		dgBigVector p2 (&pool [ptr->m_next->m_incidentVertex * stride]);
		dgBigVector d1 (p2 - p1);
		hacd::HaF32 f = dgSqrt (d1 % d1);
		if (f < hacd::HaF32 (1.0e-10f)) {
			f = hacd::HaF32 (1.0e-10f);
		}
		d1 = d1.Scale (hacd::HaF32 (1.0f) / f);
		dgBigVector n (d0 * d1);

		hacd::HaF64 angle = normal %  n;
		if (angle >= hacd::HaF64 (0.0f)) {
			heap.Push (ptr, angle);
		}

		if (angle < minAngle) {
			minAngle = angle;
		}

		d0 = d1;
		p1 = p2;
		ptr = ptr->m_next;
	} while (ptr != face);

	if (minAngle > hacd::HaF32 (0.1f)) {
		return heap[0];
	}

	dgEdge* ear = NULL;
	while (heap.GetCount()) {
		ear = heap[0];
		heap.Pop();

		if (FindEdge (ear->m_prev->m_incidentVertex, ear->m_next->m_incidentVertex)) {
			continue;
		}

		dgBigVector p0 (&pool [ear->m_prev->m_incidentVertex * stride]);
		dgBigVector p1 (&pool [ear->m_incidentVertex * stride]);
		dgBigVector p2 (&pool [ear->m_next->m_incidentVertex * stride]);

		dgBigVector p10 (p1 - p0);
		dgBigVector p21 (p2 - p1);
		dgBigVector p02 (p0 - p2);

		for (ptr = ear->m_next->m_next; ptr != ear->m_prev; ptr = ptr->m_next) {
			dgBigVector p (&pool [ptr->m_incidentVertex * stride]);

			hacd::HaF64 side = ((p - p0) * p10) % normal;
			if (side < hacd::HaF64 (0.05f)) {
				side = ((p - p1) * p21) % normal;
				if (side < hacd::HaF64 (0.05f)) {
					side = ((p - p2) * p02) % normal;
					if (side < hacd::HaF32 (0.05f)) {
						break;
					}
				}
			}
		}

		if (ptr == ear->m_prev) {
			break;
		}
	}

	return ear;
}





//dgEdge* TriangulateFace (dgPolyhedra& polyhedra, dgEdge* face, const hacd::HaF32* const pool, hacd::HaI32 stride, dgDownHeap<dgEdge*, hacd::HaF32>& heap, dgVector* const faceNormalOut)
dgEdge* dgPolyhedra::TriangulateFace (dgEdge* face, const hacd::HaF64* const pool, hacd::HaI32 stride, dgDownHeap<dgEdge*, hacd::HaF64>& heap, dgBigVector* const faceNormalOut)
{
#ifdef _DEBUG
	dgEdge* perimeter [1024 * 16]; 
	dgEdge* ptr = face;
	hacd::HaI32 perimeterCount = 0;
	do {
		perimeter[perimeterCount] = ptr;
		perimeterCount ++;
		HACD_ASSERT (perimeterCount < hacd::HaI32 (sizeof (perimeter) / sizeof (perimeter[0])));
		ptr = ptr->m_next;
	} while (ptr != face);
	perimeter[perimeterCount] = face;
	HACD_ASSERT ((perimeterCount + 1) < hacd::HaI32 (sizeof (perimeter) / sizeof (perimeter[0])));
#endif
	dgBigVector normal (FaceNormal (face, pool, hacd::HaI32 (stride * sizeof (hacd::HaF64))));

	hacd::HaF64 dot = normal % normal;
	if (dot < hacd::HaF64 (1.0e-12f)) {
		if (faceNormalOut) {
			*faceNormalOut = dgBigVector (hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f)); 
		}
		return face;
	}
	normal = normal.Scale (hacd::HaF64 (1.0f) / sqrt (dot));
	if (faceNormalOut) {
		*faceNormalOut = normal;
	}


	while (face->m_next->m_next->m_next != face) {
		dgEdge* const ear = FindEarTip (face, pool, stride, heap, normal); 
		if (!ear) {
			return face;
		}
		if ((face == ear)	|| (face == ear->m_prev)) {
			face = ear->m_prev->m_prev;
		}
		dgEdge* const edge = AddHalfEdge (ear->m_next->m_incidentVertex, ear->m_prev->m_incidentVertex);
		if (!edge) {
			return face;
		}
		dgEdge* const twin = AddHalfEdge (ear->m_prev->m_incidentVertex, ear->m_next->m_incidentVertex);
		if (!twin) {
			return face;
		}
		HACD_ASSERT (twin);


		edge->m_mark = ear->m_mark;
		edge->m_userData = ear->m_next->m_userData;
		edge->m_incidentFace = ear->m_incidentFace;

		twin->m_mark = ear->m_mark;
		twin->m_userData = ear->m_prev->m_userData;
		twin->m_incidentFace = ear->m_incidentFace;

		edge->m_twin = twin;
		twin->m_twin = edge;

		twin->m_prev = ear->m_prev->m_prev;
		twin->m_next = ear->m_next;
		ear->m_prev->m_prev->m_next = twin;
		ear->m_next->m_prev = twin;

		edge->m_next = ear->m_prev;
		edge->m_prev = ear;
		ear->m_prev->m_prev = edge;
		ear->m_next = edge;

		heap.Flush ();
	}
	return NULL;
}


void dgPolyhedra::MarkAdjacentCoplanarFaces (dgPolyhedra& polyhedraOut, dgEdge* const face, const hacd::HaF64* const pool, hacd::HaI32 strideInBytes)
{
	const hacd::HaF64 normalDeviation = hacd::HaF64 (0.9999f);
	const hacd::HaF64 distanceFromPlane = hacd::HaF64 (1.0f / 128.0f);

	hacd::HaI32 faceIndex[1024 * 4];
	dgEdge* stack[1024 * 4];
	dgEdge* deleteEdge[1024 * 4];

	hacd::HaI32 deleteCount = 1;
	deleteEdge[0] = face;
	hacd::HaI32 stride = hacd::HaI32 (strideInBytes / sizeof (hacd::HaF64));

	HACD_ASSERT (face->m_incidentFace > 0);

	dgBigVector normalAverage (FaceNormal (face, pool, strideInBytes));
	hacd::HaF64 dot = normalAverage % normalAverage;
	if (dot > hacd::HaF64 (1.0e-12f)) {
		hacd::HaI32 testPointsCount = 1;
		dot = hacd::HaF64 (1.0f) / sqrt (dot);
		dgBigVector normal (normalAverage.Scale (dot));

		dgBigVector averageTestPoint (&pool[face->m_incidentVertex * stride]);
		dgBigPlane testPlane(normal, - (averageTestPoint % normal));

		polyhedraOut.BeginFace();

		IncLRU();
		hacd::HaI32 faceMark = IncLRU();

		hacd::HaI32 faceIndexCount = 0;
		dgEdge* ptr = face;
		do {
			ptr->m_mark = faceMark;
			faceIndex[faceIndexCount] = ptr->m_incidentVertex;
			faceIndexCount ++;
			HACD_ASSERT (faceIndexCount < hacd::HaI32 (sizeof (faceIndex) / sizeof (faceIndex[0])));
			ptr = ptr ->m_next;
		} while (ptr != face);
		polyhedraOut.AddFace(faceIndexCount, faceIndex);

		hacd::HaI32 index = 1;
		deleteCount = 0;
		stack[0] = face;
		while (index) 
		{
			index --;
			dgEdge* const face = stack[index];
			deleteEdge[deleteCount] = face;
			deleteCount ++;
			HACD_ASSERT (deleteCount < hacd::HaI32 (sizeof (deleteEdge) / sizeof (deleteEdge[0])));
// TODO:JWR Temporarily commented out...			HACD_ASSERT (face->m_next->m_next->m_next == face);

			dgEdge* edge = face;
			do 
			{
				dgEdge* const ptr = edge->m_twin;
				if (ptr->m_incidentFace > 0) 
				{
					if (ptr->m_mark != faceMark) 
					{
						dgEdge* ptr1 = ptr;
						faceIndexCount = 0;
						do 
						{
							ptr1->m_mark = faceMark;
							faceIndex[faceIndexCount] = ptr1->m_incidentVertex;
							HACD_ASSERT (faceIndexCount < hacd::HaI32 (sizeof (faceIndex) / sizeof (faceIndex[0])));
							faceIndexCount ++;
							ptr1 = ptr1 ->m_next;
						} while (ptr1 != ptr);

						dgBigVector normal1 (FaceNormal (ptr, pool, strideInBytes));
						dot = normal1 % normal1;
						if (dot < hacd::HaF64 (1.0e-12f)) {
							deleteEdge[deleteCount] = ptr;
							deleteCount ++;
							HACD_ASSERT (deleteCount < hacd::HaI32 (sizeof (deleteEdge) / sizeof (deleteEdge[0])));
						} else {
							//normal1 = normal1.Scale (hacd::HaF64 (1.0f) / sqrt (dot));
							dgBigVector testNormal (normal1.Scale (hacd::HaF64 (1.0f) / sqrt (dot)));
							dot = normal % testNormal;
							if (dot >= normalDeviation) {
								dgBigVector testPoint (&pool[ptr->m_prev->m_incidentVertex * stride]);
								hacd::HaF64 dist = fabs (testPlane.Evalue (testPoint));
								if (dist < distanceFromPlane) {
									testPointsCount ++;

									averageTestPoint += testPoint;
									testPoint = averageTestPoint.Scale (hacd::HaF64 (1.0f) / hacd::HaF64(testPointsCount));

									normalAverage += normal1;
									testNormal = normalAverage.Scale (hacd::HaF64 (1.0f) / sqrt (normalAverage % normalAverage));
									testPlane = dgBigPlane (testNormal, - (testPoint % testNormal));

									polyhedraOut.AddFace(faceIndexCount, faceIndex);;
									stack[index] = ptr;
									index ++;
									HACD_ASSERT (index < hacd::HaI32 (sizeof (stack) / sizeof (stack[0])));
								}
							}
						}
					}
				}

				edge = edge->m_next;
			} while (edge != face);
		}
		polyhedraOut.EndFace();
	}

	for (hacd::HaI32 index = 0; index < deleteCount; index ++) {
		DeleteFace (deleteEdge[index]);
	}
}


void dgPolyhedra::RefineTriangulation (const hacd::HaF64* const vertex, hacd::HaI32 stride, dgBigVector* const normal, hacd::HaI32 perimeterCount, dgEdge** const perimeter)
{
	dgList<dgDiagonalEdge> dignonals;

	for (hacd::HaI32 i = 1; i <= perimeterCount; i ++) {
		dgEdge* const last = perimeter[i - 1];
		for (dgEdge* ptr = perimeter[i]->m_prev; ptr != last; ptr = ptr->m_twin->m_prev) {
			dgList<dgDiagonalEdge>::dgListNode* node = dignonals.GetFirst();
			for (; node; node = node->GetNext()) {
				const dgDiagonalEdge& key = node->GetInfo();
				if (((key.m_i0 == ptr->m_incidentVertex) && (key.m_i1 == ptr->m_twin->m_incidentVertex)) ||
					((key.m_i1 == ptr->m_incidentVertex) && (key.m_i0 == ptr->m_twin->m_incidentVertex))) {
						break;
				}
			}
			if (!node) {
				dgDiagonalEdge key (ptr);
				dignonals.Append(key);
			}
		}
	}

	dgEdge* const face = perimeter[0];
	hacd::HaI32 i0 = face->m_incidentVertex * stride;
	hacd::HaI32 i1 = face->m_next->m_incidentVertex * stride;
	dgBigVector p0 (vertex[i0], vertex[i0 + 1], vertex[i0 + 2], hacd::HaF32 (0.0f));
	dgBigVector p1 (vertex[i1], vertex[i1 + 1], vertex[i1 + 2], hacd::HaF32 (0.0f));

	dgBigVector p1p0 (p1 - p0);
	hacd::HaF64 mag2 = p1p0 % p1p0;
	for (dgEdge* ptr = face->m_next->m_next; mag2 < hacd::HaF32 (1.0e-12f); ptr = ptr->m_next) {
		hacd::HaI32 i1 = ptr->m_incidentVertex * stride;
		dgBigVector p1 (vertex[i1], vertex[i1 + 1], vertex[i1 + 2], hacd::HaF32 (0.0f));
		p1p0 = p1 - p0;
		mag2 = p1p0 % p1p0;
	}

	dgMatrix matrix (dgGetIdentityMatrix());
	matrix.m_posit = p0;
	matrix.m_front = dgVector (p1p0.Scale (hacd::HaF64 (1.0f) / sqrt (mag2)));
	matrix.m_right = dgVector (normal->Scale (hacd::HaF64 (1.0f) / sqrt (*normal % *normal)));
	matrix.m_up = matrix.m_right * matrix.m_front;
	matrix = matrix.Inverse();
	matrix.m_posit.m_w = hacd::HaF32 (1.0f);

	hacd::HaI32 maxCount = dignonals.GetCount() * dignonals.GetCount();
	while (dignonals.GetCount() && maxCount) {
		maxCount --;
		dgList<dgDiagonalEdge>::dgListNode* const node = dignonals.GetFirst();
		dgDiagonalEdge key (node->GetInfo());
		dignonals.Remove(node);
		dgEdge* const edge = FindEdge(key.m_i0, key.m_i1);
		if (edge) {
			hacd::HaI32 i0 = edge->m_incidentVertex * stride;
			hacd::HaI32 i1 = edge->m_next->m_incidentVertex * stride;
			hacd::HaI32 i2 = edge->m_next->m_next->m_incidentVertex * stride;
			hacd::HaI32 i3 = edge->m_twin->m_prev->m_incidentVertex * stride;

			dgBigVector p0 (vertex[i0], vertex[i0 + 1], vertex[i0 + 2], hacd::HaF64 (0.0f));
			dgBigVector p1 (vertex[i1], vertex[i1 + 1], vertex[i1 + 2], hacd::HaF64 (0.0f));
			dgBigVector p2 (vertex[i2], vertex[i2 + 1], vertex[i2 + 2], hacd::HaF64 (0.0f));
			dgBigVector p3 (vertex[i3], vertex[i3 + 1], vertex[i3 + 2], hacd::HaF64 (0.0f));

			p0 = matrix.TransformVector(p0);
			p1 = matrix.TransformVector(p1);
			p2 = matrix.TransformVector(p2);
			p3 = matrix.TransformVector(p3);

			hacd::HaF64 circleTest[3][3];
			circleTest[0][0] = p0[0] - p3[0];
			circleTest[0][1] = p0[1] - p3[1];
			circleTest[0][2] = circleTest[0][0] * circleTest[0][0] + circleTest[0][1] * circleTest[0][1];

			circleTest[1][0] = p1[0] - p3[0];
			circleTest[1][1] = p1[1] - p3[1];
			circleTest[1][2] = circleTest[1][0] * circleTest[1][0] + circleTest[1][1] * circleTest[1][1];

			circleTest[2][0] = p2[0] - p3[0];
			circleTest[2][1] = p2[1] - p3[1];
			circleTest[2][2] = circleTest[2][0] * circleTest[2][0] + circleTest[2][1] * circleTest[2][1];

			hacd::HaF64 error;
			hacd::HaF64 det = Determinant3x3 (circleTest, &error);
			if (det < hacd::HaF32 (0.0f)) {
				dgEdge* frontFace0 = edge->m_prev;
				dgEdge* backFace0 = edge->m_twin->m_prev;

				FlipEdge(edge);

				if (perimeterCount > 4) {
					dgEdge* backFace1 = backFace0->m_next;
					dgEdge* frontFace1 = frontFace0->m_next;
					for (hacd::HaI32 i = 0; i < perimeterCount; i ++) {
						if (frontFace0 == perimeter[i]) {
							frontFace0 = NULL;
						}
						if (frontFace1 == perimeter[i]) {
							frontFace1 = NULL;
						}

						if (backFace0 == perimeter[i]) {
							backFace0 = NULL;
						}
						if (backFace1 == perimeter[i]) {
							backFace1 = NULL;
						}
					}

					if (backFace0 && (backFace0->m_incidentFace > 0) && (backFace0->m_twin->m_incidentFace > 0)) {
						dgDiagonalEdge key (backFace0);
						dignonals.Append(key);
					}
					if (backFace1 && (backFace1->m_incidentFace > 0) && (backFace1->m_twin->m_incidentFace > 0)) {
						dgDiagonalEdge key (backFace1);
						dignonals.Append(key);
					}

					if (frontFace0 && (frontFace0->m_incidentFace > 0) && (frontFace0->m_twin->m_incidentFace > 0)) {
						dgDiagonalEdge key (frontFace0);
						dignonals.Append(key);
					}

					if (frontFace1 && (frontFace1->m_incidentFace > 0) && (frontFace1->m_twin->m_incidentFace > 0)) {
						dgDiagonalEdge key (frontFace1);
						dignonals.Append(key);
					}
				}
			}
		}
	}
}


void dgPolyhedra::RefineTriangulation (const hacd::HaF64* const vertex, hacd::HaI32 stride)
{
	dgEdge* edgePerimeters[1024 * 16];
	hacd::HaI32 perimeterCount = 0;

	dgPolyhedra::Iterator iter (*this);
	for (iter.Begin(); iter; iter ++) {
		dgEdge* const edge = &(*iter);
		if (edge->m_incidentFace < 0) {
			dgEdge* ptr = edge;
			do {
				edgePerimeters[perimeterCount] = ptr->m_twin;
				perimeterCount ++;
				HACD_ASSERT (perimeterCount < hacd::HaI32 (sizeof (edgePerimeters) / sizeof (edgePerimeters[0])));
				ptr = ptr->m_prev;
			} while (ptr != edge);
			break;
		}
	}
	HACD_ASSERT (perimeterCount);
	HACD_ASSERT (perimeterCount < hacd::HaI32 (sizeof (edgePerimeters) / sizeof (edgePerimeters[0])));
	edgePerimeters[perimeterCount] = edgePerimeters[0];

	dgBigVector normal (FaceNormal(edgePerimeters[0], vertex, hacd::HaI32 (stride * sizeof (hacd::HaF64))));
	if ((normal % normal) > hacd::HaF32 (1.0e-12f)) {
		RefineTriangulation (vertex, stride, &normal, perimeterCount, edgePerimeters);
	}
}


void dgPolyhedra::OptimizeTriangulation (const hacd::HaF64* const vertex, hacd::HaI32 strideInBytes)
{
	hacd::HaI32 polygon[1024 * 8];
	hacd::HaI32 stride = hacd::HaI32 (strideInBytes / sizeof (hacd::HaF64));

	dgPolyhedra leftOver;
	dgPolyhedra buildConvex;

	buildConvex.BeginFace();
	dgPolyhedra::Iterator iter (*this);

	for (iter.Begin(); iter; ) {
		dgEdge* const edge = &(*iter);
		iter++;

		if (edge->m_incidentFace > 0) {
			dgPolyhedra flatFace;
			MarkAdjacentCoplanarFaces (flatFace, edge, vertex, strideInBytes);
			//HACD_ASSERT (flatFace.GetCount());

			if (flatFace.GetCount()) {
				//flatFace.Triangulate (vertex, strideInBytes, &leftOver);
				//HACD_ASSERT (!leftOver.GetCount());
				flatFace.RefineTriangulation (vertex, stride);

				hacd::HaI32 mark = flatFace.IncLRU();
				dgPolyhedra::Iterator iter (flatFace);
				for (iter.Begin(); iter; iter ++) {
					dgEdge* const edge = &(*iter);
					if (edge->m_mark != mark) {
						if (edge->m_incidentFace > 0) {
							dgEdge* ptr = edge;
							hacd::HaI32 vertexCount = 0;
							do {
								polygon[vertexCount] = ptr->m_incidentVertex;				
								vertexCount ++;
								HACD_ASSERT (vertexCount < hacd::HaI32 (sizeof (polygon) / sizeof (polygon[0])));
								ptr->m_mark = mark;
								ptr = ptr->m_next;
							} while (ptr != edge);
							if (vertexCount >= 3) {
								buildConvex.AddFace (vertexCount, polygon);
							}
						}
					}
				}
			}
			iter.Begin();
		}
	}
	buildConvex.EndFace();
	HACD_ASSERT (GetCount() == 0);
	SwapInfo(buildConvex);
}


void dgPolyhedra::Triangulate (const hacd::HaF64* const vertex, hacd::HaI32 strideInBytes, dgPolyhedra* const leftOver)
{
	hacd::HaI32 count = GetCount() / 2;
	dgStack<char> memPool (hacd::HaI32 ((count + 512) * (2 * sizeof (hacd::HaF64)))); 
	dgDownHeap<dgEdge*, hacd::HaF64> heap(&memPool[0], memPool.GetSizeInBytes());
#if 0
	hacd::HaI32 stride = hacd::HaI32 (strideInBytes / sizeof (hacd::HaF64));
	hacd::HaI32 mark = IncLRU();
	Iterator iter (*this);
	for (iter.Begin(); iter; ) 
	{
		dgEdge* const thisEdge = &(*iter);
		iter ++;

		if (thisEdge->m_mark == mark) 
		{
			continue;
		}
		if (thisEdge->m_incidentFace < 0) 
		{
			continue;
		}

		count = 0;
		dgEdge* ptr = thisEdge;
		do 
		{
			count ++;
			ptr->m_mark = mark;
			ptr = ptr->m_next;
		} while (ptr != thisEdge);

		if (count > 3) 
		{
			dgEdge* const edge = TriangulateFace (thisEdge, vertex, stride, heap, NULL);
			heap.Flush ();

			if (edge) 
			{
				HACD_ASSERT (edge->m_incidentFace > 0);

				if (leftOver) 
				{
					hacd::HaI32* const index = (hacd::HaI32 *) &heap[0];
					hacd::HaI64* const data = (hacd::HaI64 *)&index[count];
					hacd::HaI32 i = 0;
					dgEdge* ptr = edge;
					do {
						index[i] = ptr->m_incidentVertex;
						data[i] = hacd::HaI64 (ptr->m_userData);
						i ++;
						ptr = ptr->m_next;
					} while (ptr != edge);
					leftOver->AddFace(i, index, data);
				} 
				DeleteFace (edge);
				iter.Begin();
			}
		}
	}
	OptimizeTriangulation (vertex, strideInBytes);
	mark = IncLRU();
	m_faceSecuence = 1;
	for (iter.Begin(); iter; iter ++) 
	{
		dgEdge* edge = &(*iter);
		if (edge->m_mark == mark) 
		{
			continue;
		}
		if (edge->m_incidentFace < 0) 
		{
			continue;
		}
		HACD_ASSERT (edge == edge->m_next->m_next->m_next);

		for (hacd::HaI32 i = 0; i < 3; i ++) 
		{
			edge->m_incidentFace = m_faceSecuence; 
			edge->m_mark = mark;
			edge = edge->m_next;
		}
		m_faceSecuence ++;
	}
#endif
}


static void RemoveColinearVertices (dgPolyhedra& flatFace, const hacd::HaF64* const vertex, hacd::HaI32 stride)
{
	dgEdge* edgePerimeters[1024];

	hacd::HaI32 perimeterCount = 0;
	hacd::HaI32 mark = flatFace.IncLRU();
	dgPolyhedra::Iterator iter (flatFace);
	for (iter.Begin(); iter; iter ++) {
		dgEdge* const edge = &(*iter);
		if ((edge->m_incidentFace < 0) && (edge->m_mark != mark)) {
			dgEdge* ptr = edge;
			do {
				ptr->m_mark = mark;
				ptr = ptr->m_next;
			} while (ptr != edge);
			edgePerimeters[perimeterCount] = edge;
			perimeterCount ++;
			HACD_ASSERT (perimeterCount < hacd::HaI32 (sizeof (edgePerimeters) / sizeof (edgePerimeters[0])));
		}
	}

	for (hacd::HaI32 i = 0; i < perimeterCount; i ++) {
		dgEdge* edge = edgePerimeters[i];
		dgEdge* ptr = edge;
		dgVector p0 (&vertex[ptr->m_incidentVertex * stride]);
		dgVector p1 (&vertex[ptr->m_next->m_incidentVertex * stride]);
		dgVector e0 (p1 - p0) ;
		e0 = e0.Scale (hacd::HaF32 (1.0f) / (dgSqrt (e0 % e0) + hacd::HaF32 (1.0e-12f)));
		hacd::HaI32 ignoreTest = 1;
		do {
			ignoreTest = 0;
			dgVector p2 (&vertex[ptr->m_next->m_next->m_incidentVertex * stride]);
			dgVector e1 (p2 - p1);
			e1 = e1.Scale (hacd::HaF32 (1.0f) / (dgSqrt (e1 % e1) + hacd::HaF32 (1.0e-12f)));
			hacd::HaF32 dot = e1 % e0;
			if (dot > hacd::HaF32 (hacd::HaF32 (0.9999f))) {

				for (dgEdge* interiorEdge = ptr->m_next->m_twin->m_next; interiorEdge != ptr->m_twin; interiorEdge = ptr->m_next->m_twin->m_next) {
					flatFace.DeleteEdge (interiorEdge);
				} 

				if (ptr->m_twin->m_next->m_next->m_next == ptr->m_twin) {
					HACD_ASSERT (ptr->m_twin->m_next->m_incidentFace > 0);
					flatFace.DeleteEdge (ptr->m_twin->m_next);
				}

				HACD_ASSERT (ptr->m_next->m_twin->m_next->m_twin == ptr);
				edge = ptr->m_next;

				if (!flatFace.FindEdge (ptr->m_incidentVertex, edge->m_twin->m_incidentVertex) && 
					!flatFace.FindEdge (edge->m_twin->m_incidentVertex, ptr->m_incidentVertex)) {
						ptr->m_twin->m_prev = edge->m_twin->m_prev;
						edge->m_twin->m_prev->m_next = ptr->m_twin;

						edge->m_next->m_prev = ptr;
						ptr->m_next = edge->m_next;

						edge->m_next = edge->m_twin;
						edge->m_prev = edge->m_twin;
						edge->m_twin->m_next = edge;
						edge->m_twin->m_prev = edge;
						flatFace.DeleteEdge (edge);								
						flatFace.ChangeEdgeIncidentVertex (ptr->m_twin, ptr->m_next->m_incidentVertex);

						e1 = e0;
						p1 = p2;
						edge = ptr;
						ignoreTest = 1;
						continue;
				}
			}

			e0 = e1;
			p1 = p2;
			ptr = ptr->m_next;
		} while ((ptr != edge) || ignoreTest);
	}
}


static hacd::HaI32 GetInteriorDiagonals (dgPolyhedra& polyhedra, dgEdge** const diagonals, hacd::HaI32 maxCount)
{
	hacd::HaI32 count = 0;
	hacd::HaI32 mark = polyhedra.IncLRU();
	dgPolyhedra::Iterator iter (polyhedra);
	for (iter.Begin(); iter; iter++) {
		dgEdge* const edge = &(*iter);
		if (edge->m_mark != mark) { 
			if (edge->m_incidentFace > 0) {
				if (edge->m_twin->m_incidentFace > 0) {
					edge->m_twin->m_mark = mark;
					if (count < maxCount){
						diagonals[count] = edge;
						count ++;
					}
					HACD_ASSERT (count <= maxCount);
				}
			}
		}
		edge->m_mark = mark;
	}

	return count;
}

static bool IsEssensialPointDiagonal (dgEdge* const diagonal, const dgBigVector& normal, const hacd::HaF64* const pool, hacd::HaI32 stride)
{
	hacd::HaF64 dot;
	dgBigVector p0 (&pool[diagonal->m_incidentVertex * stride]);
	dgBigVector p1 (&pool[diagonal->m_twin->m_next->m_twin->m_incidentVertex * stride]);
	dgBigVector p2 (&pool[diagonal->m_prev->m_incidentVertex * stride]);

	dgBigVector e1 (p1 - p0);
	dot = e1 % e1;
	if (dot < hacd::HaF64 (1.0e-12f)) {
		return false;
	}
	e1 = e1.Scale (hacd::HaF64 (1.0f) / sqrt(dot));

	dgBigVector e2 (p2 - p0);
	dot = e2 % e2;
	if (dot < hacd::HaF64 (1.0e-12f)) {
		return false;
	}
	e2 = e2.Scale (hacd::HaF64 (1.0f) / sqrt(dot));

	dgBigVector n1 (e1 * e2); 

	dot = normal % n1;
	//if (dot > hacd::HaF64 (hacd::HaF32 (0.1f)f)) {
	//if (dot >= hacd::HaF64 (-1.0e-6f)) {
	if (dot >= hacd::HaF64 (0.0f)) {
		return false;
	}
	return true;
}


static bool IsEssensialDiagonal (dgEdge* const diagonal, const dgBigVector& normal, const hacd::HaF64* const pool,  hacd::HaI32 stride)
{
	return IsEssensialPointDiagonal (diagonal, normal, pool, stride) || IsEssensialPointDiagonal (diagonal->m_twin, normal, pool, stride); 
}


void dgPolyhedra::ConvexPartition (const hacd::HaF64* const vertex, hacd::HaI32 strideInBytes, dgPolyhedra* const leftOversOut)
{
	if (GetCount()) {
		Triangulate (vertex, strideInBytes, leftOversOut);
		DeleteDegenerateFaces (vertex, strideInBytes, hacd::HaF32 (1.0e-5f));
		Optimize (vertex, strideInBytes, hacd::HaF32 (1.0e-4f));
		DeleteDegenerateFaces (vertex, strideInBytes, hacd::HaF32 (1.0e-5f));

		if (GetCount()) {
			hacd::HaI32 removeCount = 0;
			hacd::HaI32 stride = hacd::HaI32 (strideInBytes / sizeof (hacd::HaF64));

			hacd::HaI32 polygon[1024 * 8];
			dgEdge* diagonalsPool[1024 * 8];
			dgPolyhedra buildConvex;

			buildConvex.BeginFace();
			dgPolyhedra::Iterator iter (*this);
			for (iter.Begin(); iter; ) {
				dgEdge* edge = &(*iter);
				iter++;
				if (edge->m_incidentFace > 0) {

					dgPolyhedra flatFace;
					MarkAdjacentCoplanarFaces (flatFace, edge, vertex, strideInBytes);

					if (flatFace.GetCount()) {
						flatFace.RefineTriangulation (vertex, stride);
						RemoveColinearVertices (flatFace, vertex, stride);

						hacd::HaI32 diagonalCount = GetInteriorDiagonals (flatFace, diagonalsPool, sizeof (diagonalsPool) / sizeof (diagonalsPool[0]));
						if (diagonalCount) {
							edge = &flatFace.GetRoot()->GetInfo();
							if (edge->m_incidentFace < 0) {
								edge = edge->m_twin;
							}
							HACD_ASSERT (edge->m_incidentFace > 0);

							dgBigVector normal (FaceNormal (edge, vertex, strideInBytes));
							normal = normal.Scale (hacd::HaF64 (1.0f) / sqrt (normal % normal));

							edge = NULL;
							dgPolyhedra::Iterator iter (flatFace);
							for (iter.Begin(); iter; iter ++) {
								edge = &(*iter);
								if (edge->m_incidentFace < 0) {
									break;
								}
							}
							HACD_ASSERT (edge);

							hacd::HaI32 isConvex = 1;
							dgEdge* ptr = edge;
							hacd::HaI32 mark = flatFace.IncLRU();

							dgBigVector normal2 (normal);
							dgBigVector p0 (&vertex[ptr->m_prev->m_incidentVertex * stride]);
							dgBigVector p1 (&vertex[ptr->m_incidentVertex * stride]);
							dgBigVector e0 (p1 - p0);
							e0 = e0.Scale (hacd::HaF32 (1.0f) / (dgSqrt (e0 % e0) + hacd::HaF32 (1.0e-14f)));
							do {
								dgBigVector p2 (&vertex[ptr->m_next->m_incidentVertex * stride]);
								dgBigVector e1 (p2 - p1);
								e1 = e1.Scale (hacd::HaF32 (1.0f) / (sqrt (e1 % e1) + hacd::HaF32 (1.0e-14f)));
								hacd::HaF64 dot = (e0 * e1) % normal2;
								//if (dot > hacd::HaF32 (0.0f)) {
								if (dot > hacd::HaF32 (5.0e-3f)) {
									isConvex = 0;
									break;
								}
								ptr->m_mark = mark;
								e0 = e1;
								p1 = p2;
								ptr = ptr->m_next;
							} while (ptr != edge);

							if (isConvex) {
								dgPolyhedra::Iterator iter (flatFace);
								for (iter.Begin(); iter; iter ++) {
									ptr = &(*iter);
									if (ptr->m_incidentFace < 0) {
										if (ptr->m_mark < mark) {
											isConvex = 0;
											break;
										}
									}
								}
							}

							if (isConvex) {
								if (diagonalCount > 2) {
									hacd::HaI32 count = 0;
									ptr = edge;
									do {
										polygon[count] = ptr->m_incidentVertex;				
										count ++;
										HACD_ASSERT (count < hacd::HaI32 (sizeof (polygon) / sizeof (polygon[0])));
										ptr = ptr->m_next;
									} while (ptr != edge);

									for (hacd::HaI32 i = 0; i < count - 1; i ++) {
										for (hacd::HaI32 j = i + 1; j < count; j ++) {
											if (polygon[i] == polygon[j]) {
												i = count;
												isConvex = 0;
												break ;
											}
										}
									}
								}
							}

							if (isConvex) {
								for (hacd::HaI32 j = 0; j < diagonalCount; j ++) {
									dgEdge* const diagonal = diagonalsPool[j];
									removeCount ++;
									flatFace.DeleteEdge (diagonal);
								}
							} else {
								for (hacd::HaI32 j = 0; j < diagonalCount; j ++) {
									dgEdge* const diagonal = diagonalsPool[j];
									if (!IsEssensialDiagonal(diagonal, normal, vertex, stride)) {
										removeCount ++;
										flatFace.DeleteEdge (diagonal);
									}
								}
							}
						}

						hacd::HaI32 mark = flatFace.IncLRU();
						dgPolyhedra::Iterator iter (flatFace);
						for (iter.Begin(); iter; iter ++) {
							dgEdge* const edge = &(*iter);
							if (edge->m_mark != mark) {
								if (edge->m_incidentFace > 0) {
									dgEdge* ptr = edge;
									hacd::HaI32 diagonalCount = 0;
									do {
										polygon[diagonalCount] = ptr->m_incidentVertex;				
										diagonalCount ++;
										HACD_ASSERT (diagonalCount < hacd::HaI32 (sizeof (polygon) / sizeof (polygon[0])));
										ptr->m_mark = mark;
										ptr = ptr->m_next;
									} while (ptr != edge);
									if (diagonalCount >= 3) {
										buildConvex.AddFace (diagonalCount, polygon);
									}
								}
							}
						}
					}
					iter.Begin();
				}
			}

			buildConvex.EndFace();
			HACD_ASSERT (GetCount() == 0);
			SwapInfo(buildConvex);
		}
	}
}


dgSphere dgPolyhedra::CalculateSphere (const hacd::HaF64* const vertex, hacd::HaI32 strideInBytes, const dgMatrix* const basis) const
{
/*
		// this si a degenerate mesh of a flat plane calculate OOBB by discrete rotations
		dgStack<hacd::HaI32> pool (GetCount() * 3 + 6); 
		hacd::HaI32* const indexList = &pool[0]; 

		dgMatrix axis (dgGetIdentityMatrix());
		dgBigVector p0 (hacd::HaF32 ( 1.0e10f), hacd::HaF32 ( 1.0e10f), hacd::HaF32 ( 1.0e10f), hacd::HaF32 (0.0f));
		dgBigVector p1 (hacd::HaF32 (-1.0e10f), hacd::HaF32 (-1.0e10f), hacd::HaF32 (-1.0e10f), hacd::HaF32 (0.0f));

		hacd::HaI32 stride = hacd::HaI32 (strideInBytes / sizeof (hacd::HaF64));
		hacd::HaI32 indexCount = 0;
		hacd::HaI32 mark = IncLRU();
		dgPolyhedra::Iterator iter(*this);
		for (iter.Begin(); iter; iter ++) {
			dgEdge* const edge = &(*iter);
			if (edge->m_mark != mark) {
				dgEdge *ptr = edge;
				do {
					ptr->m_mark = mark;
					ptr = ptr->m_twin->m_next;
				} while (ptr != edge);
				hacd::HaI32 index = edge->m_incidentVertex;
				indexList[indexCount + 6] = edge->m_incidentVertex;
				dgBigVector point (vertex[index * stride + 0], vertex[index * stride + 1], vertex[index * stride + 2], hacd::HaF32 (0.0f));
				for (hacd::HaI32 i = 0; i < 3; i ++) {
					if (point[i] < p0[i]) {
						p0[i] = point[i];
						indexList[i * 2 + 0] = index;
					}
					if (point[i] > p1[i]) {
						p1[i] = point[i];
						indexList[i * 2 + 1] = index;
					}
				}
				indexCount ++;
			}
		}
		indexCount += 6;


		dgBigVector size (p1 - p0);
		hacd::HaF64 volume = size.m_x * size.m_y * size.m_z;


		for (hacd::HaF32 pitch = hacd::HaF32 (0.0f); pitch < hacd::HaF32 (90.0f); pitch += hacd::HaF32 (10.0f)) {
			dgMatrix pitchMatrix (dgPitchMatrix(pitch * hacd::HaF32 (3.1416f) / hacd::HaF32 (180.0f)));
			for (hacd::HaF32 yaw = hacd::HaF32 (0.0f); yaw  < hacd::HaF32 (90.0f); yaw  += hacd::HaF32 (10.0f)) {
				dgMatrix yawMatrix (dgYawMatrix(yaw * hacd::HaF32 (3.1416f) / hacd::HaF32 (180.0f)));
				for (hacd::HaF32 roll = hacd::HaF32 (0.0f); roll < hacd::HaF32 (90.0f); roll += hacd::HaF32 (10.0f)) {
					hacd::HaI32 tmpIndex[6];
					dgMatrix rollMatrix (dgRollMatrix(roll * hacd::HaF32 (3.1416f) / hacd::HaF32 (180.0f)));
					dgMatrix tmp (pitchMatrix * yawMatrix * rollMatrix);
					dgBigVector q0 (hacd::HaF32 ( 1.0e10f), hacd::HaF32 ( 1.0e10f), hacd::HaF32 ( 1.0e10f), hacd::HaF32 (0.0f));
					dgBigVector q1 (hacd::HaF32 (-1.0e10f), hacd::HaF32 (-1.0e10f), hacd::HaF32 (-1.0e10f), hacd::HaF32 (0.0f));

					hacd::HaF32 volume1 = hacd::HaF32 (1.0e10f);
					for (hacd::HaI32 i = 0; i < indexCount; i ++) {
						hacd::HaI32 index = indexList[i];
						dgBigVector point (vertex[index * stride + 0], vertex[index * stride + 1], vertex[index * stride + 2], hacd::HaF32 (0.0f));
						point = tmp.UnrotateVector(point);

						for (hacd::HaI32 j = 0; j < 3; j ++) {
							if (point[j] < q0[j]) {
								q0[j] = point[j];
								tmpIndex[j * 2 + 0] = index;
							}
							if (point[j] > q1[j]) {
								q1[j] = point[j];
								tmpIndex[j * 2 + 1] = index;
							}
						}


						dgVector size1 (q1 - q0);
						volume1 = size1.m_x * size1.m_y * size1.m_z;
						if (volume1 >= volume) {
							break;
						}
					}

					if (volume1 < volume) {
						p0 = q0;
						p1 = q1;
						axis = tmp;
						volume = volume1;
						memcpy (indexList, tmpIndex, sizeof (tmpIndex));
					}
				}
			}
		}

		HACD_ASSERT (0);
		dgSphere sphere (axis);
		dgVector q0 (p0);
		dgVector q1 (p1);
		sphere.m_posit = axis.RotateVector((q1 + q0).Scale (hacd::HaF32 (0.5f)));
		sphere.m_size = (q1 - q0).Scale (hacd::HaF32 (0.5f));
		return sphere;
*/

	hacd::HaI32 stride = hacd::HaI32 (strideInBytes / sizeof (hacd::HaF64));	

	hacd::HaI32 vertexCount = 0;
	hacd::HaI32 mark = IncLRU();
	dgPolyhedra::Iterator iter(*this);
	for (iter.Begin(); iter; iter ++) {
		dgEdge* const edge = &(*iter);
		if (edge->m_mark != mark) {
			dgEdge* ptr = edge;
			do {
				ptr->m_mark = mark;
				ptr = ptr->m_twin->m_next;
			} while (ptr != edge);
			vertexCount ++;
		}
	}
	HACD_ASSERT (vertexCount);

	mark = IncLRU();
	hacd::HaI32 vertexCountIndex = 0;
	dgStack<dgBigVector> pool (vertexCount);
	for (iter.Begin(); iter; iter ++) {
		dgEdge* const edge = &(*iter);
		if (edge->m_mark != mark) {
			dgEdge* ptr = edge;
			do {
				ptr->m_mark = mark;
				ptr = ptr->m_twin->m_next;
			} while (ptr != edge);
			hacd::HaI32 incidentVertex = edge->m_incidentVertex * stride;
			pool[vertexCountIndex] = dgBigVector (vertex[incidentVertex + 0], vertex[incidentVertex + 1], vertex[incidentVertex + 2], hacd::HaF32 (0.0f));
			vertexCountIndex ++;
		}
	}
	HACD_ASSERT (vertexCountIndex <= vertexCount);

	dgMatrix axis (dgGetIdentityMatrix());
	dgSphere sphere (axis);
	dgConvexHull3d convexHull (&pool[0].m_x, sizeof (dgBigVector), vertexCountIndex, 0.0f);
	if (convexHull.GetCount()) {
		dgStack<hacd::HaI32> triangleList (convexHull.GetCount() * 3); 				
		hacd::HaI32 trianglesCount = 0;
		for (dgConvexHull3d::dgListNode* node = convexHull.GetFirst(); node; node = node->GetNext()) {
			dgConvexHull3DFace* const face = &node->GetInfo();
			triangleList[trianglesCount * 3 + 0] = face->m_index[0];
			triangleList[trianglesCount * 3 + 1] = face->m_index[1];
			triangleList[trianglesCount * 3 + 2] = face->m_index[2];
			trianglesCount ++;
			HACD_ASSERT ((trianglesCount * 3) <= triangleList.GetElementsCount());
		}

		dgVector* const dst = (dgVector*) &pool[0].m_x;
		for (hacd::HaI32 i = 0; i < convexHull.GetVertexCount(); i ++) {
			dst[i] = convexHull.GetVertex(i);
		}
		sphere.SetDimensions (&dst[0].m_x, sizeof (dgVector), &triangleList[0], trianglesCount * 3, NULL);

	} else if (vertexCountIndex >= 3) {
		dgStack<hacd::HaI32> triangleList (GetCount() * 3 * 2); 
		hacd::HaI32 mark = IncLRU();
		hacd::HaI32 trianglesCount = 0;
		for (iter.Begin(); iter; iter ++) {
			dgEdge* const edge = &(*iter);
			if (edge->m_mark != mark) {
				dgEdge* ptr = edge;
				do {
					ptr->m_mark = mark;
					ptr = ptr->m_twin->m_next;
				} while (ptr != edge);

				ptr = edge->m_next->m_next;
				do {
					triangleList[trianglesCount * 3 + 0] = edge->m_incidentVertex;
					triangleList[trianglesCount * 3 + 1] = ptr->m_prev->m_incidentVertex;
					triangleList[trianglesCount * 3 + 2] = ptr->m_incidentVertex;
					trianglesCount ++;
					HACD_ASSERT ((trianglesCount * 3) <= triangleList.GetElementsCount());
					ptr = ptr->m_twin->m_next;
				} while (ptr != edge);

				dgVector* const dst = (dgVector*) &pool[0].m_x;
				for (hacd::HaI32 i = 0; i < vertexCountIndex; i ++) {
					dst[i] = pool[i];
				}
				sphere.SetDimensions (&dst[0].m_x, sizeof (dgVector), &triangleList[0], trianglesCount * 3, NULL);
			}
		}
	}
	return sphere;

}
