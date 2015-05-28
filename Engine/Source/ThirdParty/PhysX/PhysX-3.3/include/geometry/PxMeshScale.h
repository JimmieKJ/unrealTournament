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


#ifndef PX_PHYSICS_NX_MESHSCALE
#define PX_PHYSICS_NX_MESHSCALE
/** \addtogroup geomutils
@{
*/

#include "common/PxPhysXCommonConfig.h"
#include "foundation/PxMat33.h"

/** \brief Minimum allowed absolute magnitude for each of mesh scale's components (x,y,z).
	\note Only positive scale values are allowed for convex meshes. */
#define PX_MIN_ABS_MESH_SCALE 1e-6f

/** \brief Maximum allowed absolute magnitude for each of mesh scale's components (x,y,z).
	\note Only positive scale values are allowed for convex meshes. */
#define PX_MAX_ABS_MESH_SCALE 1e6f

#ifndef PX_DOXYGEN
namespace physx
{
#endif

/**
\brief A class expressing a nonuniform scaling transformation.

The scaling is along arbitrary axes that are specified by PxMeshScale::rotation.

\note	Negative scale values are supported for PxTriangleMeshGeometry
		with absolute values for each component within [PX_MIN_ABS_MESH_SCALE, PX_MAX_ABS_MESH_SCALE] range.
		Negative scale causes a reflection around the specified axis, in addition PhysX will flip the normals
		for mesh triangles when scale.x*scale.y*scale.z < 0.
\note	Only positive scale values are supported for PxConvexMeshGeometry
		with values for each component within [PX_MIN_ABS_MESH_SCALE, PX_MAX_ABS_MESH_SCALE] range).

@see PxConvexMeshGeometry PxTriangleMeshGeometry
*/
class PxMeshScale
{
//= ATTENTION! =====================================================================================
// Changing the data layout of this class breaks the binary serialization format.  See comments for 
// PX_BINARY_SERIAL_VERSION.  If a modification is required, please adjust the getBinaryMetaData 
// function.  If the modification is made on a custom branch, please change PX_BINARY_SERIAL_VERSION
// accordingly.
//==================================================================================================
public:
	/**
	\brief Constructor initializes to identity scale.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxMeshScale(): scale(1.0f), rotation(PxIdentity) 
	{
	}

	/**
	\brief Constructor from scalar.
	*/
	explicit PX_CUDA_CALLABLE PX_FORCE_INLINE PxMeshScale(PxReal r): scale(r), rotation(PxIdentity) 
	{
	}

	/**
	\brief Constructor to initialize to arbitrary scale and identity scale rotation.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxMeshScale(const PxVec3& s)
	{
		scale = s;
		rotation = PxQuat(PxIdentity);
	}

	/**
	\brief Constructor to initialize to arbitrary scaling.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxMeshScale(const PxVec3& s, const PxQuat& r)
	{
		PX_ASSERT(r.isUnit());
		scale = s;
		rotation = r;
	}


	/**
	\brief Returns true if the scaling is an identity transformation.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE bool isIdentity()	const
	{
		return (scale.x == 1.0f && scale.y == 1.0f && scale.z == 1.0f);
	}

	/**
	\brief Returns the inverse of this scaling transformation.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE  PxMeshScale getInverse() const 
	{
		return PxMeshScale(PxVec3(1.0f/scale.x, 1.0f/scale.y, 1.0f/scale.z), rotation);
	}

	/**
	\deprecated
	\brief Returns the identity scaling transformation.
	*/
	PX_DEPRECATED static PX_CUDA_CALLABLE PX_FORCE_INLINE PxMeshScale createIdentity()
	{
		return PxMeshScale(1.0f);
	}

	/**
	\brief Converts this transformation to a 3x3 matrix representation.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxMat33 toMat33() const 
	{
		PxMat33 rot(rotation);
		PxMat33 trans = rot.getTranspose();
		trans.column0 *= scale[0];
		trans.column1 *= scale[1];
		trans.column2 *= scale[2];
		return trans * rot;
	}

	/**
	\brief Returns true if combination of negative scale components will cause the triangle normal to flip. The SDK will flip the normals internally.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE bool flipsNormal() const
	{
		return (scale.x * scale.y * scale.z < 0.0f);
	}

	PxVec3		transform(const PxVec3& v) const
	{
		return rotation.rotateInv(scale.multiply(rotation.rotate(v)));
	}

	bool		isValidForMesh() const
	{
		PxVec3 absXYZ = scale.abs();
		return (absXYZ.maxElement() <= PX_MAX_ABS_MESH_SCALE) && (absXYZ.minElement() >= PX_MIN_ABS_MESH_SCALE);
	}

	bool		isValidForConvex() const
	{
		return (scale.maxElement() <= PX_MAX_ABS_MESH_SCALE) && (scale.minElement() >= PX_MIN_ABS_MESH_SCALE);
	}

	PxVec3		scale;		//!< A nonuniform scaling
	PxQuat		rotation;	//!< The orientation of the scaling axes


};

#ifndef PX_DOXYGEN
} // namespace physx
#endif

/** @} */
#endif
