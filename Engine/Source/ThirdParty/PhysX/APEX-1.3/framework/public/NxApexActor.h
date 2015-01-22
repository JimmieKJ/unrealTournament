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

#ifndef NX_APEX_ACTOR_H
#define NX_APEX_ACTOR_H

/*!
\file
\brief classes NxApexActor, NxApexActorSource
*/

#include "NxApexInterface.h"

// PhysX SDK class declarations
#if NX_SDK_VERSION_MAJOR == 2
class NxActorDesc;
class NxActorDescBase;
class NxShapeDesc;
class NxBodyDesc;
#elif NX_SDK_VERSION_MAJOR == 3
namespace physx { namespace apex
{
class PhysX3DescTemplate;
}}
#endif

namespace physx
{
namespace apex
{

PX_PUSH_PACK_DEFAULT

class NxApexAsset;

/**
\brief Base class for APEX module objects.
*/
class NxApexActor : public NxApexInterface
{
public:
	/**
	\brief Returns the owning asset
	*/
	virtual NxApexAsset* getOwner() const = 0;

	/**
	\brief Returns the range of possible values for physical Lod overwrite

	\param [out] min		The minimum lod value
	\param [out] max		The maximum lod value
	\param [out] intOnly	Only integers are allowed if this is true, gets rounded to nearest

	\note The max value can change with different graphical Lods
	\see NxApexActor::forcePhysicalLod()
	*/
	virtual void getPhysicalLodRange(physx::PxF32& min, physx::PxF32& max, bool& intOnly) = 0;

	/**
	\brief Get current physical lod.
	*/
	virtual physx::PxF32 getActivePhysicalLod() = 0;

	/**
	\brief Force an APEX Actor to use a certian physical Lod

	\param [in] lod	Overwrite the Lod system to use this Lod.

	\note Setting the lod value to a negative number will turn off the overwrite and proceed with regular Lod computations
	\see NxApexActor::getPhysicalLodRange()
	*/
	virtual void forcePhysicalLod(physx::PxF32 lod) = 0;

	/**
	\brief Ensure that all module-cached data is cached.
	*/
	virtual void cacheModuleData() const {}

	/**
	\brief Selectively enables/disables debug visualization of a specific APEX actor.  Default value it true.
	*/
	virtual void setEnableDebugVisualization(bool state) = 0;
	

protected:
	virtual ~NxApexActor() {} // use release() method instead!
};

/**
\brief Base class for APEX classes that spawn PhysX SDK Actors
*/
class NxApexActorSource
{
public:
	/**
	\brief Set the current actor template

	User can specify a descriptor template for actors that this object may create.  APEX may customize these suggested settings.
	Does not include shape array, as we only want base properties.  Specify base shape properties using setShapeBase().

	Already created / existing actors will not be changed if the actor template is changed!  The actor template will only be used for new
	actors created after this is called!

	members that are ignored:
	globalPose
	body
	type

	These fields should be left at their default values as set by the desc constructor.

	Because it is not possible to instance the type NxActorDescBase directly, it is simplest to pass a pointer to a NxActorDesc.

	You can pass NULL to not have a template.
	*/

#if NX_SDK_VERSION_MAJOR == 2
	virtual void setActorTemplate(const NxActorDescBase*) = 0;

	/**
	\brief Retrieve the current actor template

	If the template is NULL this will return false; otherwise it will fill in dest and return true.
	*/
	virtual bool getActorTemplate(NxActorDescBase& dest) const = 0;

	/**
	\brief Sets the current shape template

	User can specify a descriptor template for shapes that this object may create.  APEX may customize these suggested settings.
	Already created / existing shapes will not be changed if the shape template is changed!  The shape template will only be used for new
	shapes created after this is called!

	members that are ignored:
	type
	localPose
	ccdSkeleton

	These fields should be left at their default values as set by the desc constructor.

	Because it is not possible to instance the type NxShapeDesc directly, it is simplest to pass a pointer to a NxSphereShapeDesc.
	*/
	virtual void setShapeTemplate(const NxShapeDesc*) = 0;

	/**
	\brief Retrieve the current shape template

	If the template is NULL this will return false; otherwise it will fill in dest and return true.
	*/
	virtual bool getShapeTemplate(NxShapeDesc& dest) const = 0;

	/**
	\brief Sets the current body template

	User can specify a descriptor template for bodies that this object may create.  APEX may customize these suggested settings.
	Already created / existing bodies will not be changed if the body template is changed!  The body template will only be used for
	new bodies created after this is called!

	members that are ignored:
	massLocalPose
	massSpaceInertia
	mass
	linearVelocity
	angularVelocity

	These fields should be left at their default values as set by the desc constructor.

	Because it is not possible to instance the type NxShapeDesc directly, it is simplest to pass a pointer to a NxSphereShapeDesc.
	*/
	virtual void setBodyTemplate(const NxBodyDesc*) = 0;

	/**
	\brief Retrieve the current body template

	If the template is NULL this will return false; otherwise it will fill in dest and return true.
	*/
	virtual bool getBodyTemplate(NxBodyDesc& dest) const = 0;
#elif NX_SDK_VERSION_MAJOR == 3
	/**
	\brief Sets the current body template

	User can specify a descriptor template for bodies that this object may create.  APEX may customize these suggested settings.
	Already created / existing bodies will not be changed if the body template is changed!  The body template will only be used for
	new bodies created after this is called!

	members that are ignored:
	massLocalPose
	massSpaceInertia
	mass
	linearVelocity
	angularVelocity

	These fields should be left at their default values as set by the desc constructor.

	Because it is not possible to instance the type NxShapeDesc directly, it is simplest to pass a pointer to a NxSphereShapeDesc.
	*/
	virtual void setPhysX3Template(const PhysX3DescTemplate*) = 0;

	/**
	\brief Retrieve the current body template

	If the template is NULL this will return false; otherwise it will fill in dest and return true.
	*/
	virtual bool getPhysX3Template(PhysX3DescTemplate& dest) const = 0;
#endif
};

PX_POP_PACK

}
} // end namespace physx::apex

#endif // NX_APEX_ACTOR_H
