// This code contains NVIDIA Confidential Information and is disclosed to you
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and
// any modifications thereto. Any use, reproduction, disclosure, or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2008-2014 NVIDIA Corporation. All rights reserved.

#ifndef NX_CLOTHING_PHYSICAL_MESH_H
#define NX_CLOTHING_PHYSICAL_MESH_H

#include "NxApexInterface.h"
#include "NxApexUserProgress.h"

namespace physx
{
namespace apex
{


/**
\brief replica of the 2.8.4 NxClothConstrainCoefficients.
*/
struct NxClothingConstrainCoefficients
{
	PxF32 maxDistance; ///< \brief Maximum distance a vertex is allowed to move
	PxF32 maxDistanceBias; ///< \brief Turns max distance sphere into a capsule or disc
	PxF32 collisionSphereRadius; ///< \brief Backstop sphere radius
	PxF32 collisionSphereDistance; ///< \brief Backstop sphere distance
};


PX_PUSH_PACK_DEFAULT

/**
\brief Contains the data for handing out statistics about a NxClothingPhysicalMesh
*/
struct NxClothingPhysicalMeshStats
{
	/// the number of bytes allocated for the physical mesh
	physx::PxU32	totalBytes;
	/// the number of vertices
	physx::PxU32	numVertices;
	/// the number of indices
	physx::PxU32	numIndices;
};


/**
\brief Holder for a physical mesh, this can be generated through various ways (see single- and multi-layered clothing) or hand crafted
*/
class NxClothingPhysicalMesh : public NxApexInterface
{
public:
	/**
	\brief returns the number of vertices
	*/
	virtual physx::PxU32 getNumVertices() const = 0;

	/**
	\brief returns the number of incides
	*/
	virtual physx::PxU32 getNumIndices() const = 0;

	/**
	\brief writes the indices to a destination buffer

	\param [out] indexDestination	destination buffer where to write the indices
	\param [in] byteStride			stride of the destination buffer
	\param [in] numIndices			number of indices the buffer can hold. This can be smaller than getNumIndices()
	*/
	virtual void getIndices(void* indexDestination, physx::PxU32 byteStride, physx::PxU32 numIndices) const = 0;

	/**
	\brief returns whether the mesh is built out of tetrahedra or triangles
	*/
	virtual bool isTetrahedralMesh() const = 0;

	/**
	\brief This will simplify the current mesh.

	\param [in] subdivisions	used to derive the maximal length a new edge can get.<br>
								Divide the bounding box diagonal by this value to get the maximal edge length for newly created edges<br>
								Use 0 to not restrict the maximal edge length
	\param [in] maxSteps		The maximum number of edges to be considered for simplification.<br>
								Use 0 to turn off
	\param [in] maxError		The maximal quadric error an edge can cause to be considered simplifyable.<br>
								Use any value < 0 to turn off
	\param [in] progress		Callback class that will be fired every now and then to update a progress bar in the gui.
	\return The number of edges collapsed
	*/
	virtual void simplify(physx::PxU32 subdivisions, physx::PxI32 maxSteps, physx::PxF32 maxError, IProgressListener* progress) = 0;

	/**
	\brief Create a physical mesh from scratch

	Overwrites all vertices/indices, and invalidates all misc vertex buffers. vertices must be physx::PxVec3 and indices physx::PxU32.
	*/
	virtual void setGeometry(bool tetraMesh, physx::PxU32 numVertices, physx::PxU32 vertexByteStride, const void* vertices, physx::PxU32 numIndices, physx::PxU32 indexByteStride, const void* indices) = 0;

	// direct access to specific buffers
	/**
	\brief writes the indices into a user specified buffer.

	Returns false if the buffer doesn't exist.
	The buffer must be bigger than sizeof(physx::PxU32) * getNumIndices()
	*/
	virtual bool getIndices(physx::PxU32* indices, physx::PxU32 byteStride) const = 0;

	/**
	\brief Writes the vertex positions into a user specified buffer.

	Returns false if the buffer doesn't exist.
	The buffer must be bigger than sizeof(physx::PxVec3) * getNumVertices()
	*/
	virtual bool getVertices(physx::PxVec3* vertices, physx::PxU32 byteStride) const = 0;

	/**
	\brief Writes the normals into a user specified buffer.

	Returns false if the buffer doesn't exist.
	The buffer must be bigger than sizeof(physx::PxVec3) * getNumVertices()
	*/
	virtual bool getNormals(physx::PxVec3* normals, physx::PxU32 byteStride) const = 0;

	/**
	\brief Returns the number of bones per vertex.
	*/
	virtual physx::PxU32 getNumBonesPerVertex() const = 0;

	/**
	\brief Writes the bone indices into a user specified buffer.

	Returns false if the buffer doesn't exist.
	The buffer must be bigger than sizeof(physx::PxU16) * getNumVertices() * getNumBonesPerVertex().
	(numBonesPerVertex is the same as in the graphical mesh for LOD 0)

	The bytestride is applied only after writing numBonesPerVertex and thus must be >= sizoef(physx::PxU16) * numBonesPerVertex
	*/
	virtual bool getBoneIndices(physx::PxU16* boneIndices, physx::PxU32 byteStride) const = 0;

	/**
	\brief Writes the bone weights into a user specified buffer.

	Returns false if the buffer doesn't exist.
	The buffer must be bigger than sizeof(physx::PxF32) * getNumVertices() * getNumBonesPerVertex().
	(numBonesPerVertex is the same as in the graphical mesh for LOD 0)
	The bytestride is applied only after writing numBonesPerVertex and thus must be >= sizoef(physx::PxF32) * numBonesPerVertex
	*/
	virtual bool getBoneWeights(physx::PxF32* boneWeights, physx::PxU32 byteStride) const = 0;

	/**
	\brief Writes the cloth constrain coefficients into a user specified buffer.

	Returns false if the buffer doesn't exist. The buffer must be bigger than sizeof(ClothingConstrainCoefficients) * getNumVertices().
	*/
	virtual bool getConstrainCoefficients(NxClothingConstrainCoefficients* coeffs, physx::PxU32 byteStride) const = 0;

	/**
	\brief Returns stats (sizes, counts) for the asset.  See NxClothingPhysicalMeshStats.
	*/
	virtual void getStats(NxClothingPhysicalMeshStats& stats) const = 0;

};

PX_POP_PACK

}
} // namespace physx::apex

#endif // NX_CLOTHING_PHYSICAL_MESH_H
