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

#ifndef NX_USER_RENDER_BONE_BUFFER_H
#define NX_USER_RENDER_BONE_BUFFER_H

/*!
\file
\brief class NxUserRenderBoneBuffer
*/

#include "NxApexRenderBufferData.h"
#include "NxUserRenderBoneBufferDesc.h"

namespace physx
{
namespace apex
{

PX_PUSH_PACK_DEFAULT

/**
\brief type of the bone buffer data
*/
class NxApexRenderBoneBufferData : public NxApexRenderBufferData<NxRenderBoneSemantic, NxRenderBoneSemantic::Enum> {};

/**
\brief Used for storing skeletal bone information used during skinning.

Separated out into its own interface as we don't know how the user is going to implement this
it could be in another vertex buffer, a texture or just an array and skinned on CPU depending
on engine and hardware support.
*/
class NxUserRenderBoneBuffer
{
public:
	virtual		~NxUserRenderBoneBuffer() {}

	/**
	\brief Called when APEX wants to update the contents of the bone buffer.

	The source data type is assumed to be the same as what was defined in the descriptor.
	APEX should call this function and supply data for ALL semantics that were originally
	requested during creation every time its called.

	\param [in] data				Contains the source data for the bone buffer.
	\param [in] firstBone			first bone to start writing to.
	\param [in] numBones			number of bones to write.
	*/
	virtual void writeBuffer(const physx::NxApexRenderBoneBufferData& data, physx::PxU32 firstBone, physx::PxU32 numBones)	= 0;
};

PX_POP_PACK

}
} // end namespace physx::apex

#endif
