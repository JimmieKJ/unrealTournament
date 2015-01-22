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

#ifndef NX_APEX_DESC_H
#define NX_APEX_DESC_H

/*!
\file
\brief class NxApexDesc
*/

#include "NxApexUsingNamespace.h"

namespace physx
{
namespace apex
{

PX_PUSH_PACK_DEFAULT

/**
\brief Base class for all APEX Descriptor classes

A descriptor class of type NxXDesc is typically passed to a createX() function.  Descriptors have
several advantages over simply passing a number of explicit parameters to create():

- all parameters can have default values so the user only needs to know about the ones he needs to change
- new parameters can be added without changing the user code, along with defaults
- the user and the SDK can validate the parameter's correctness using isValid()
- if creation fails, the user can look at the code of isValid() to see what exactly is not being accepted by the SDK
- some object types can save out their state into descriptors again for serialization

Care should be taken that derived descriptor classes do not initialize their base class members multiple times,
once in the constructor, and once in setToDefault()!
*/
class NxApexDesc
{
public:
	/**
	\brief for standard init of user data member
	*/
	void* userData;

	/**
	\brief constructor sets to default.
	*/
	PX_INLINE NxApexDesc()
	{
		setToDefault();
	}
	/**
	\brief (re)sets the structure to the default.
	*/
	PX_INLINE void setToDefault()
	{
		userData = 0;
	}
	/**
	\brief Returns true if the descriptor is valid.
	\return true if the current settings are valid.
	*/
	PX_INLINE bool isValid() const
	{
		return true;
	}
};

PX_POP_PACK

}
} // end namespace physx::apex

#endif // NX_APEX_DESC_H
