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

#ifndef NX_USER_RENDER_RESOURCE_H
#define NX_USER_RENDER_RESOURCE_H

/*!
\file
\brief class NxUserRenderResource
*/

#include "NxApexUsingNamespace.h"

namespace physx
{
namespace apex
{

class NxUserRenderVertexBuffer;
class NxUserRenderIndexBuffer;
class NxUserRenderBoneBuffer;
class NxUserRenderInstanceBuffer;
class NxUserRenderSpriteBuffer;


PX_PUSH_PACK_DEFAULT

/**
\brief An abstract interface to a renderable resource
*/
class NxUserRenderResource
{
public:
	virtual ~NxUserRenderResource() {}

	/** \brief Set vertex buffer range */
	virtual void setVertexBufferRange(physx::PxU32 firstVertex, physx::PxU32 numVerts) = 0;
	/** \brief Set index buffer range */
	virtual void setIndexBufferRange(physx::PxU32 firstIndex, physx::PxU32 numIndices) = 0;
	/** \brief Set bone buffer range */
	virtual void setBoneBufferRange(physx::PxU32 firstBone, physx::PxU32 numBones) = 0;
	/** \brief Set instance buffer range */
	virtual void setInstanceBufferRange(physx::PxU32 firstInstance, physx::PxU32 numInstances) = 0;
	/** \brief Set sprite buffer range */
	virtual void setSpriteBufferRange(physx::PxU32 firstSprite, physx::PxU32 numSprites) = 0;
	/** \brief Set sprite visible count */
	virtual void setSpriteVisibleCount(physx::PxU32 visibleCount) { PX_UNUSED(visibleCount); }
	/** \brief Set material */
	virtual void setMaterial(void* material) = 0;

	/** \brief Get number of vertex buffers */
	virtual physx::PxU32				getNbVertexBuffers() const = 0;
	/** \brief Get vertex buffer */
	virtual NxUserRenderVertexBuffer*	getVertexBuffer(physx::PxU32 index) const = 0;
	/** \brief Get index buffer */
	virtual NxUserRenderIndexBuffer*	getIndexBuffer() const = 0;
	/** \brief Get bone buffer */
	virtual NxUserRenderBoneBuffer*		getBoneBuffer() const = 0;
	/** \brief Get instance buffer */
	virtual NxUserRenderInstanceBuffer*	getInstanceBuffer() const = 0;
	/** \brief Get sprite buffer */
	virtual NxUserRenderSpriteBuffer*	getSpriteBuffer() const = 0;
};

PX_POP_PACK

}
} // end namespace physx::apex

#endif
