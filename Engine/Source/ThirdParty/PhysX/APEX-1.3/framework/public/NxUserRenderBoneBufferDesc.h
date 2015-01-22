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

#ifndef NX_USER_RENDER_BONE_BUFFER_DESC_H
#define NX_USER_RENDER_BONE_BUFFER_DESC_H

/*!
\file
\brief class NxUserRenderBoneBufferDesc, structs NxRenderDataFormat and NxRenderBoneSemantic
*/

#include "NxApexRenderDataFormat.h"
#include "NxUserRenderResourceManager.h"

namespace physx
{
namespace apex
{

PX_PUSH_PACK_DEFAULT

/**
\brief The semantics available for bone buffers
*/
struct NxRenderBoneSemantic
{
	/**
	\brief Enum of the semantics available for bone buffers
	*/
	enum Enum
	{
		POSE = 0,		//!< A matrix that transforms from object space into animated object space or even world space
		PREVIOUS_POSE,	//!< The corresponding poses from the last frame
		NUM_SEMANTICS	//!< Count of semantics, not a valid semantic.
	};
};



/**
\brief Descriptor to generate a bone buffer

This descriptor is filled out by APEX and helps as a guide how the
bone buffer should be generated.
*/
class NxUserRenderBoneBufferDesc
{
public:
	NxUserRenderBoneBufferDesc(void)
	{
		maxBones = 0;
		hint     = NxRenderBufferHint::STATIC;
		for (physx::PxU32 i = 0; i < NxRenderBoneSemantic::NUM_SEMANTICS; i++)
		{
			buffersRequest[i] = NxRenderDataFormat::UNSPECIFIED;
		}
	}

	/**
	\brief Check if parameter's values are correct
	*/
	bool isValid(void) const
	{
		physx::PxU32 numFailed = 0;
		return (numFailed == 0);
	}

public:
	/**
	\brief The maximum amount of bones this buffer will ever hold.
	*/
	physx::PxU32				maxBones;

	/**
	\brief Hint on how often this buffer is updated.
	*/
	NxRenderBufferHint::Enum	hint;

	/**
	\brief Array of semantics with the corresponding format.

	NxRenderDataFormat::UNSPECIFIED is used for semantics that are disabled
	*/
	NxRenderDataFormat::Enum	buffersRequest[NxRenderBoneSemantic::NUM_SEMANTICS];
};

PX_POP_PACK

}
} // end namespace physx::apex

#endif
