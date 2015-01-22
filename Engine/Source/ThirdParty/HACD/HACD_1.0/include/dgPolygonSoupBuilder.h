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
*  Visual C++ 6.0 created by: Julio Jerez
*
****************************************************************************/
#ifndef __dgPolygonSoupDatabaseBuilder0x23413452233__
#define __dgPolygonSoupDatabaseBuilder0x23413452233__


#include "dgTypes.h"
#include "dgArray.h"
#include "dgIntersections.h"


class AdjacentdFaces
{
	public:
	hacd::HaI32 m_count;
	hacd::HaI32 *m_index;
	dgPlane m_normal;
	hacd::HaI64 m_edgeMap[256];
};

class dgPolygonSoupDatabaseBuilder : public UANS::UserAllocated
{
	public:
	dgPolygonSoupDatabaseBuilder (void);
	~dgPolygonSoupDatabaseBuilder ();

	void Begin();
	void End(bool optimize);
	void AddMesh (const hacd::HaF32* const vertex, hacd::HaI32 vertexCount, hacd::HaI32 strideInBytes, hacd::HaI32 faceCount, 
		          const hacd::HaI32* const faceArray, const hacd::HaI32* const indexArray, const hacd::HaI32* const faceTagsData, const dgMatrix& worldMatrix); 

	void SingleFaceFixup();

	private:

	void Optimize(bool optimize);
	void EndAndOptimize(bool optimize);
	void OptimizeByGroupID();
	void OptimizeByIndividualFaces();
	hacd::HaI32 FilterFace (hacd::HaI32 count, hacd::HaI32* const indexArray);
	hacd::HaI32 AddConvexFace (hacd::HaI32 count, hacd::HaI32* const indexArray, hacd::HaI32* const  facesArray);
	void OptimizeByGroupID (dgPolygonSoupDatabaseBuilder& source, hacd::HaI32 faceNumber, hacd::HaI32 faceIndexNumber, dgPolygonSoupDatabaseBuilder& leftOver); 

	void PackArray();

//	void WriteDebugOutput (const char* name);

	public:
	struct VertexArray: public dgArray<dgBigVector>
	{
		VertexArray(void)
			:dgArray<dgBigVector>(1024 * 256)
		{
		}
	};

	struct IndexArray: public dgArray<hacd::HaI32>
	{
		IndexArray(void)
			:dgArray<hacd::HaI32>(1024 * 256)
		{
		}
	};

	hacd::HaI32 m_run;
	hacd::HaI32 m_faceCount;
	hacd::HaI32 m_indexCount;
	hacd::HaI32 m_vertexCount;
	hacd::HaI32 m_normalCount;
	IndexArray m_faceVertexCount;
	IndexArray m_vertexIndex;
	IndexArray m_normalIndex;
	VertexArray	m_vertexPoints;
	VertexArray	m_normalPoints;
};





#endif

