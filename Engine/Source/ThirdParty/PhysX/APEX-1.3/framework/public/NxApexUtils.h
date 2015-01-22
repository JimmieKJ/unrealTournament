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

#ifndef NX_APEX_UTILS_H
#define NX_APEX_UTILS_H

/*!
\file
\brief Misc utility classes
*/

#include "NxModule.h"
#include "foundation/PxMath.h"

namespace physx
{
namespace apex
{

PX_PUSH_PACK_DEFAULT

/**
\brief Linear interpolation
*/
class NxLerp : public NxInterpolator
{
public:
	virtual physx::PxF32 interpolate(physx::PxF32 inCurrent, physx::PxF32 inMin, physx::PxF32 inMax, physx::PxF32 outMin, physx::PxF32 outMax)
	{
		/*
		our return value is to be to the range outMin:outMax as
		inCurrent is to the range inMin:inMax -- and it is legal for it to lie outside the range!

		first convert everything to floats.
		then figure out the slope of the line via a division:
		*/

		if (inMin == inMax)  //special case to avoid divide by zero.
		{
			return outMin;
		}

		return ((inCurrent - inMin) / (inMax - inMin)) * (outMax - outMin) + outMin;
	}
};


/**
\brief Method by which chunk mesh collision hulls are generated.
*/
struct NxConvexHullMethod
{
	/**
	\brief Enum of methods by which chunk mesh collision hulls are generated.
	*/
	enum Enum
	{
		USE_6_DOP,
		USE_10_DOP_X,
		USE_10_DOP_Y,
		USE_10_DOP_Z,
		USE_14_DOP_XY,
		USE_14_DOP_YZ,
		USE_14_DOP_ZX,
		USE_18_DOP,
		USE_26_DOP,
		WRAP_GRAPHICS_MESH,
		CONVEX_DECOMPOSITION,

		COUNT
	};
};


/**
\brief Simple struct to hold a pair of integers, commonly used (for reporting overlaps, for example)
*/
struct NxIntPair
{
	///integer
	physx::PxI32	mI0;
	///integer
	physx::PxI32	mI1;
};


PX_POP_PACK

}
} // end namespace physx::apex

#endif // NX_APEX_UTILS_H
