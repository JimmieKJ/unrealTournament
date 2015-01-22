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

#ifndef NX_USER_RENDER_INSTANCE_BUFFER_DESC_H
#define NX_USER_RENDER_INSTANCE_BUFFER_DESC_H

/*!
\file
\brief class NxUserRenderInstanceBufferDesc, structs NxRenderDataFormat and NxRenderInstanceSemantic
*/

#include "NxUserRenderResourceManager.h"
#include "NxApexRenderDataFormat.h"

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
\brief Enumerates the potential instance buffer semantics
*/
struct NxRenderInstanceSemantic
{
	/**
	\brief Enume of the potential instance buffer semantics
	*/
	enum Enum
	{
		POSITION = 0,	//!< position of instance
		ROTATION_SCALE,	//!< rotation matrix and scale baked together
		VELOCITY_LIFE,	//!< velocity and life remain (1.0=new .. 0.0=dead) baked together 
		DENSITY,		//!< particle density at instance location
		COLOR,			//!< color of instance
		UV_OFFSET,		//!< an offset to apply to all U,V coordinates
		LOCAL_OFFSET,	//!< the static initial position offset of the instance

		USER_DATA,		//!< User data - 32 bits

		NUM_SEMANTICS	//!< Count of semantics, not a valid semantic.
	};

	/**
	\brief Get semantic format
	*/
	static PX_INLINE NxRenderDataFormat::Enum getSemanticFormat(Enum semantic)
	{
		switch (semantic)
		{
		case POSITION:
			return NxRenderDataFormat::FLOAT3;
		case ROTATION_SCALE:
			return NxRenderDataFormat::FLOAT3x3;
		case VELOCITY_LIFE:
			return NxRenderDataFormat::FLOAT4;
		case DENSITY:
			return NxRenderDataFormat::FLOAT1;
		case COLOR:
			return NxRenderDataFormat::B8G8R8A8;
		case UV_OFFSET:
			return NxRenderDataFormat::FLOAT2;
		case LOCAL_OFFSET:
			return NxRenderDataFormat::FLOAT3;
		case USER_DATA:
			return NxRenderDataFormat::UINT1;
		default:
			PX_ALWAYS_ASSERT();
			return NxRenderDataFormat::NUM_FORMATS;
		}
	}
};

/**
\brief potential semantics of a sprite buffer
*/
struct NxRenderInstanceLayoutElement
{
	/**
	\brief Enum of sprite buffer semantics types
	*/
	enum Enum
	{
		POSITION_FLOAT3,
		ROTATION_SCALE_FLOAT3x3,
		VELOCITY_LIFE_FLOAT4,
		DENSITY_FLOAT1,
		COLOR_RGBA8,
		COLOR_BGRA8,
		COLOR_FLOAT4,
		UV_OFFSET_FLOAT2,
		LOCAL_OFFSET_FLOAT3,
		USER_DATA_UINT1,

		NUM_SEMANTICS
	};

	/**
	\brief Get semantic format
	*/
	static PX_INLINE NxRenderDataFormat::Enum getSemanticFormat(Enum semantic)
	{
		switch (semantic)
		{
		case POSITION_FLOAT3:
			return NxRenderDataFormat::FLOAT3;		
		case ROTATION_SCALE_FLOAT3x3:
			return NxRenderDataFormat::FLOAT3x3;
		case VELOCITY_LIFE_FLOAT4:
			return NxRenderDataFormat::FLOAT4;
		case DENSITY_FLOAT1:
			return NxRenderDataFormat::FLOAT1;
		case COLOR_RGBA8:
			return NxRenderDataFormat::R8G8B8A8;
		case COLOR_BGRA8:
			return NxRenderDataFormat::B8G8R8A8;
		case COLOR_FLOAT4:
			return NxRenderDataFormat::FLOAT4;
		case UV_OFFSET_FLOAT2:
			return NxRenderDataFormat::FLOAT2;
		case LOCAL_OFFSET_FLOAT3:
			return NxRenderDataFormat::FLOAT3;
		case USER_DATA_UINT1:
			return NxRenderDataFormat::UINT1;
		default:
			PX_ALWAYS_ASSERT();
			return NxRenderDataFormat::NUM_FORMATS;
		}
	}
/**
	\brief Get semantic from layout element format
	*/
	static PX_INLINE NxRenderInstanceSemantic::Enum getSemantic(Enum semantic)
	{
		switch (semantic)
		{
		case POSITION_FLOAT3:
			return NxRenderInstanceSemantic::POSITION;		
		case ROTATION_SCALE_FLOAT3x3:
			return NxRenderInstanceSemantic::ROTATION_SCALE;
		case VELOCITY_LIFE_FLOAT4:
			return NxRenderInstanceSemantic::VELOCITY_LIFE;
		case DENSITY_FLOAT1:
			return NxRenderInstanceSemantic::DENSITY;
		case COLOR_RGBA8:
		case COLOR_BGRA8:
		case COLOR_FLOAT4:
			return NxRenderInstanceSemantic::COLOR;
		case UV_OFFSET_FLOAT2:
			return NxRenderInstanceSemantic::UV_OFFSET;
		case LOCAL_OFFSET_FLOAT3:
			return NxRenderInstanceSemantic::LOCAL_OFFSET;
		case USER_DATA_UINT1:
			return NxRenderInstanceSemantic::USER_DATA;
		default:
			PX_ALWAYS_ASSERT();
			return NxRenderInstanceSemantic::NUM_SEMANTICS;
		}
	}
};

/**
\brief Describes the data and layout of an instance buffer
*/
class NxUserRenderInstanceBufferDesc
{
public:
	NxUserRenderInstanceBufferDesc(void)
	{
		setDefaults();
	}

	/**
	\brief Default values
	*/
	void setDefaults()
	{
		registerInCUDA = false;
		interopContext = 0;
		maxInstances = 0;
		hint         = NxRenderBufferHint::STATIC;
		for (physx::PxU32 i = 0; i < NxRenderInstanceSemantic::NUM_SEMANTICS; i++)
		{
			semanticFormats[i] = NxRenderDataFormat::UNSPECIFIED;
		}
		for (physx::PxU32 i = 0; i < NxRenderInstanceLayoutElement::NUM_SEMANTICS; i++)
		{
			semanticOffsets[i] = physx::PxU32(-1);
		}
		stride = 0;
	}

	/**
	\brief Check if parameter's values are correct
	*/
	bool isValid(void) const
	{
		physx::PxU32 numFailed = 0;

		numFailed += (maxInstances == 0);
		numFailed += (stride == 0);
		numFailed += (semanticOffsets[NxRenderInstanceSemantic::POSITION] == physx::PxU32(-1));
		numFailed += registerInCUDA && (interopContext == 0);

		return (numFailed == 0);
	}

public:
	physx::PxU32					maxInstances; //!< The maximum amount of instances this buffer will ever hold.
	NxRenderBufferHint::Enum		hint; //!< Hint on how often this buffer is updated.
	
	/**
	\brief Array of the corresponding formats for each semantic.

	NxRenderInstanceSemantic::UNSPECIFIED is used for semantics that are disabled
	*/
	NxRenderDataFormat::Enum		semanticFormats[NxRenderInstanceSemantic::NUM_SEMANTICS];
	/**
	\brief Array of the corresponding offsets (in bytes) for each semantic. Required when CUDA interop is used!
	*/
	physx::PxU32					semanticOffsets[NxRenderInstanceLayoutElement::NUM_SEMANTICS];

	physx::PxU32					stride; //!< The stride between instances of this buffer. Required when CUDA interop is used!

	bool							registerInCUDA; //!< Declare if the resource must be registered in CUDA upon creation

	/**
	This context must be used to register and unregister the resource every time the
	device is lost and recreated.
	*/
	physx::PxCudaContextManager*   interopContext;
};

#if !defined(PX_PS4)
	#pragma warning(pop)
#endif	//!PX_PS4

PX_POP_PACK

}
} // end namespace physx::apex

#endif // NX_USER_RENDER_INSTANCE_BUFFER_DESC_H
