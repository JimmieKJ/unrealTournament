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

#ifndef NX_APEX_PHYS_XOBJECT_DESC_H
#define NX_APEX_PHYS_XOBJECT_DESC_H

/*!
\file
\brief class NxApexPhysXObjectDesc
*/

#include "NxApexUsingNamespace.h"

namespace physx
{
namespace apex
{

class NxApexActor;

PX_PUSH_PACK_DEFAULT

/**
 * \brief PhysX object descriptor
 *
 * Class which describes how a PhysX object is being used by APEX.  Data access is
 * non virtual for performance reasons.
 */
class NxApexPhysXObjectDesc
{
protected:
	/**
	\brief Object interaction flags

	These flags determine how this PhysX object should interact with user callbacks.  For
	instance whether contact callbacks with the object should be ignored.
	*/
	physx::PxU32				mFlags;

	enum
	{
		TRANSFORM   = (1U << 31),	//!< If set, ignore this object's xform in active actor callbacks
		RAYCASTS    = (1U << 30), //!< If set, ignore this object in raycasts
		CONTACTS    = (1U << 29) //!< If set, ignore this object in contact callbacks
	};


public:
	/** \brief Returns the number of NxApexActors associated with the PhysX object */
	virtual physx::PxU32				getApexActorCount() const = 0;
	/** \brief Returns the indexed NxApexActor pointer */
	virtual const NxApexActor*	getApexActor(physx::PxU32 i) const = 0;

	/** \brief Returns whether this object's xform should be ignored */
	bool ignoreTransform() const
	{
		return (mFlags & (physx::PxU32)TRANSFORM) ? true : false;
	}
	/** \brief Returns whether this object should be ignored by raycasts */
	bool ignoreRaycasts() const
	{
		return (mFlags & (physx::PxU32)RAYCASTS) ? true : false;
	}
	/** \brief Returns whether this object should be ignored by contact report callbacks */
	bool ignoreContacts() const
	{
		return (mFlags & (physx::PxU32)CONTACTS) ? true : false;
	}
	/** \brief Returns a user defined status bit */
	bool getUserDefinedFlag(physx::PxU32 index) const
	{
		return (mFlags & (physx::PxU32)(1 << index)) ? true : false;
	}

	/**
	\brief User data, for use by APEX

	For internal use by APEX.  Please do not modify this field.  You may use the PhysX object
	userData or NxApexActor userData field.
	*/
	void* 				userData;
};

// To get owning NxApexActor's authorable object type name:
//		getActor()->getOwner()->getAuthObjName();
// To get owning NxApexActor's authorable object type ID:
//		getActor()->getOwner()->getAuthObjType();

PX_POP_PACK

}
} // end namespace physx::apex

#endif // NX_APEX_PHYS_XOBJECT_DESC_H
