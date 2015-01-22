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

#ifndef NX_USER_RENDER_INDEX_BUFFER_DESC_H
#define NX_USER_RENDER_INDEX_BUFFER_DESC_H

/*!
\file
\brief class NxUserRenderIndexBufferDesc, structs NxRenderDataFormat and NxUserRenderIndexBufferDesc
*/

#include "NxApexRenderDataFormat.h"
#include "NxUserRenderResourceManager.h"

namespace physx
{
class PxCudaContextManager;
};

namespace physx
{
namespace apex
{

PX_PUSH_PACK_DEFAULT

#if !defined(PX_PS4)
	#pragma warning(push)
	#pragma warning(disable:4121)
#endif	//!PX_PS4

/**
\brief describes the semantics and layout of an index buffer
*/
class NxUserRenderIndexBufferDesc
{
public:
	NxUserRenderIndexBufferDesc(void)
	{
		registerInCUDA = false;
		interopContext = 0;
		maxIndices = 0;
		hint       = NxRenderBufferHint::STATIC;
		format     = NxRenderDataFormat::UNSPECIFIED;
		primitives = NxRenderPrimitiveType::TRIANGLES;
	}

	/**
	\brief Check if parameter's values are correct
	*/
	bool isValid(void) const
	{
		physx::PxU32 numFailed = 0;
		numFailed += (registerInCUDA && !interopContext) ? 1 : 0;
		return (numFailed == 0);
	}

public:

	/**
	\brief The maximum amount of indices this buffer will ever hold.
	*/
	physx::PxU32				maxIndices;

	/**
	\brief Hint on how often this buffer is updated
	*/
	NxRenderBufferHint::Enum	hint;

	/**
	\brief The format of this buffer (only one implied semantic)
	*/
	NxRenderDataFormat::Enum	format;

	/**
	\brief Rendering primitive type (triangle, line strip, etc)
	*/
	NxRenderPrimitiveType::Enum	primitives;

	/**
	\brief Declare if the resource must be registered in CUDA upon creation
	*/
	bool						registerInCUDA;

	/**
	\brief The CUDA context

	This context must be used to register and unregister the resource every time the
	device is lost and recreated.
	*/
	physx::PxCudaContextManager*	interopContext;
};

#if !defined(PX_PS4)
	#pragma warning(pop)
#endif	//!PX_PS4

PX_POP_PACK

}
} // end namespace physx::apex

#endif // NX_USER_RENDER_INDEX_BUFFER_DESC_H
