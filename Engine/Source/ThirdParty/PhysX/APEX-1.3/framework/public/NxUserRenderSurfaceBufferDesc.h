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

#ifndef NX_USER_RENDER_SURFACE_BUFFER_DESC_H
#define NX_USER_RENDER_SURFACE_BUFFER_DESC_H

/*!
\file
\brief class NxUserRenderSurfaceBufferDesc, structs NxRenderDataFormat and NxRenderSurfaceSemantic
*/

#include "NxApexUsingNamespace.h"
#include "NxUserRenderResourceManager.h"
#include "NxApexRenderDataFormat.h"
#include "NxApexSDK.h"

namespace physx
{
namespace apex
{

PX_PUSH_PACK_DEFAULT

/**
\brief Describes the semantics and layout of a Surface buffer
*/
class NxUserRenderSurfaceBufferDesc
{
public:
	NxUserRenderSurfaceBufferDesc(void)
	{
		setDefaults();
	}

	/**
	\brief Default values
	*/
	void setDefaults()
	{
//		hint   = NxRenderBufferHint::STATIC;		
		format = NxRenderDataFormat::UNSPECIFIED;
		
		width = 0;
		height = 0;
		depth = 1;

		//moduleIdentifier = 0;
		
		registerInCUDA = false;
		interopContext = 0;
//		stride = 0;
	}

	/**
	\brief Checks if the surface buffer descriptor is valid
	*/
	bool isValid(void) const
	{
		physx::PxU32 numFailed = 0;
		numFailed += (format == NxRenderDataFormat::UNSPECIFIED);
		numFailed += (width == 0) && (height == 0) && (depth == 0);
		numFailed += registerInCUDA && (interopContext == 0);
//		numFailed += registerInCUDA && (stride == 0);
		
		return (numFailed == 0);
	}

public:
	/**
	\brief The size of U-dimension.
	*/
	physx::PxU32				width;
	
	/**
	\brief The size of V-dimension.
	*/
	physx::PxU32				height;

	/**
	\brief The size of W-dimension.
	*/
	physx::PxU32				depth;

	/**
	\brief A hint about the update frequency of this buffer
	*/
//	NxRenderBufferHint::Enum	hint;

	/**
	\brief Data format of suface buffer.
	*/
	NxRenderDataFormat::Enum	format;

	/**
	\brief Identifier of module generating this request
	*/
	//NxAuthObjTypeID				moduleIdentifier;

	/**
	\brief Buffer can be shared by multiple render resources
	*/
	//bool							canBeShared;

//	physx::PxU32					stride; //!< The stride between sprites of this buffer. Required when CUDA interop is used!

	bool							registerInCUDA;  //!< Declare if the resource must be registered in CUDA upon creation

	/**
	This context must be used to register and unregister the resource every time the
	device is lost and recreated.
	*/
	physx::PxCudaContextManager*	interopContext;
};

PX_POP_PACK

}
} // end namespace physx::apex

#endif // NX_USER_RENDER_SURFACE_BUFFER_DESC_H
