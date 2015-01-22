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

#ifndef NX_APEX_CUSTOM_BUFFER_ITERATOR_H
#define NX_APEX_CUSTOM_BUFFER_ITERATOR_H

/*!
\file
\brief class NxApexCustomBufferIterator
*/

#include "NxRenderMesh.h"

namespace physx
{
namespace apex
{

PX_PUSH_PACK_DEFAULT

/**
\brief This class is used to access specific elements in an untyped chunk of memory
*/
class NxApexCustomBufferIterator
{
public:
	/**
	\brief Returns the memory start of a specific vertex.

	All custom buffers are stored interleaved, so this is also the memory start of the first attribute of this vertex.
	*/
	virtual void*		getVertex(physx::PxU32 triangleIndex, physx::PxU32 vertexIndex) const = 0;

	/**
	\brief Returns the index of a certain custom buffer.

	\note This is constant throughout the existence of this class.
	*/
	virtual physx::PxI32		getAttributeIndex(const char* attributeName) const = 0;

	/**
	\brief Returns a pointer to a certain attribute of the specified vertex/triangle.

	\param [in] triangleIndex Which triangle
	\param [in] vertexIndex Which of the vertices of this triangle (must be either 0, 1 or 2)
	\param [in] attributeName The name of the attribute you wish the data for
	\param [out] outFormat The format of the attribute, reinterpret_cast the void pointer accordingly.
	*/
	virtual void*		getVertexAttribute(physx::PxU32 triangleIndex, physx::PxU32 vertexIndex, const char* attributeName, NxRenderDataFormat::Enum& outFormat) const = 0;

	/**
	\brief Returns a pointer to a certain attribute of the specified vertex/triangle.

	\note This is the faster method than the one above since it won't do any string comparisons

	\param [in] triangleIndex Which triangle
	\param [in] vertexIndex Which of the vertices of this triangle (must be either 0, 1 or 2)
	\param [in] attributeIndex The indexof the attribute you wish the data for (use NxApexCustomBufferIterator::getAttributeIndex to find the index to a certain attribute name
	\param [out] outFormat The format of the attribute, reinterpret_cast the void pointer accordingly.
	\param [out] outName The name associated with the attribute
	*/
	virtual void*		getVertexAttribute(physx::PxU32 triangleIndex, physx::PxU32 vertexIndex, physx::PxU32 attributeIndex, NxRenderDataFormat::Enum& outFormat, const char*& outName) const = 0;

protected:
	NxApexCustomBufferIterator() {}
	virtual				~NxApexCustomBufferIterator() {}
};

PX_POP_PACK

}
} // end namespace physx::apex

#endif // NX_APEX_CUSTOM_BUFFER_ITERATOR_H
