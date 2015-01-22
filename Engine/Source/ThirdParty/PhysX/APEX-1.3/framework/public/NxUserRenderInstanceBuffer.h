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

#ifndef NX_USER_RENDER_INSTANCE_BUFFER_H
#define NX_USER_RENDER_INSTANCE_BUFFER_H

/*!
\file
\brief class NxUserRenderInstanceBuffer
*/

#include "NxApexRenderBufferData.h"
#include "NxUserRenderInstanceBufferDesc.h"

/**
\brief Cuda graphics resource
*/
typedef struct CUgraphicsResource_st* CUgraphicsResource;

namespace physx
{
namespace apex
{

PX_PUSH_PACK_DEFAULT
/**
\brief The instance buffer type (deprecated)
*/
class PX_DEPRECATED NxApexRenderInstanceBufferData : public NxApexRenderBufferData<NxRenderInstanceSemantic, NxRenderInstanceSemantic::Enum> {};

/**
\brief Used for storing per-instance data for rendering.
*/
class NxUserRenderInstanceBuffer
{
public:
	virtual		~NxUserRenderInstanceBuffer() {}

	///Get the low-level handle of the buffer resource
	///\return true if succeeded, false otherwise
	virtual bool getInteropResourceHandle(CUgraphicsResource& handle)
#if NX_APEX_DEFAULT_NO_INTEROP_IMPLEMENTATION
	{
		PX_UNUSED(&handle);
		return false;
	}
#else
	= 0;
#endif
	/**
	\brief Called when APEX wants to update the contents of the instance buffer.

	The source data type is assumed to be the same as what was defined in the descriptor.
	APEX should call this function and supply data for ALL semantics that were originally
	requested during creation every time its called.

	\param [in] data				Contains the source data for the instance buffer.
	\param [in] firstInstance		first instance to start writing to.
	\param [in] numInstances		number of instance to write.
	*/
	virtual void writeBuffer(const void* data, physx::PxU32 firstInstance, physx::PxU32 numInstances)
	{
		PX_UNUSED(data);
		PX_UNUSED(firstInstance);
		PX_UNUSED(numInstances);
	}
};

PX_POP_PACK

}
} // end namespace physx::apex

#endif // NX_USER_RENDER_INSTANCE_BUFFER_H
