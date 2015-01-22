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

/****************************************************************************
*
*  File Name  : Bitmap.C
*  Visual C++ 4.0 base by Julio Jerez
*
****************************************************************************/
#ifndef __dgAABBCollisionGeometry0x2341753767__
#define __dgAABBCollisionGeometry0x2341753767__

#include "dgTypes.h"
#include "dgPolygonSoupDatabase.h"


class dgPolygonSoupDatabaseBuilder;

class dgAABBPolygonSoup: public dgPolygonSoupDatabase
{
	public:
	hacd::HaI32 GetIndexCount() const
	{
		return m_indexCount;
	}

	hacd::HaI32* GetIndexPool() const
	{
		return m_indices;
	}

	virtual void GetAABB (dgVector& p0, dgVector& p1) const;

	protected:
	dgAABBPolygonSoup ();
	~dgAABBPolygonSoup ();

	void* GetRootNode() const;
	void* GetBackNode(const void* const root) const;
	void* GetFrontNode(const void* const root) const;
	void GetNodeAABB(const void* const root, dgVector& p0, dgVector& p1) const;

	void CalculateAdjacendy ();
	void Create (const dgPolygonSoupDatabaseBuilder& builder, bool optimizedBuild);
	virtual void ForAllSectors (const dgVector& min, const dgVector& max, dgAABBIntersectCallback callback, void* const context) const;
	virtual void ForAllSectorsRayHit (const dgFastRayTest& ray, dgRayIntersectCallback callback, void* const context) const;
	virtual dgVector ForAllSectorsSupportVectex (const dgVector& dir) const;	

	hacd::HaF32 CalculateFaceMaxSize (dgTriplex* const vertex, hacd::HaI32 indexCount, const hacd::HaI32* const indexArray) const;

	private:
	static dgIntersectStatus CalculateThisFaceEdgeNormals (void *context, const hacd::HaF32* const polygon, hacd::HaI32 strideInBytes, const hacd::HaI32* const indexArray, hacd::HaI32 indexCount);
	static dgIntersectStatus CalculateAllFaceEdgeNormals (void *context, const hacd::HaF32* const polygon, hacd::HaI32 strideInBytes, const hacd::HaI32* const indexArray, hacd::HaI32 indexCount);


	hacd::HaI32 m_nodesCount;
	hacd::HaI32 m_indexCount;
	hacd::HaI32 *m_indices;
	void* m_aabb;

};



#endif


