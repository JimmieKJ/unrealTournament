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

#ifndef NX_INSTANCED_OBJECT_SIMULATION_ASSET_H
#define NX_INSTANCED_OBJECT_SIMULATION_ASSET_H

/*!
\file
\brief class NxIosAsset
*/

#include "NxApexAsset.h"

namespace physx
{
namespace apex
{

class NxApexScene;
class NxApexActor;

class NxIofxAsset;

PX_PUSH_PACK_DEFAULT

/**
\brief The base class of all Instanced Object Simulation classes
*/
class NxIosAsset : public NxApexAsset
{
public:
	//! \brief create a generic IOS NxApexActor in a specific NxApexSene
	virtual NxApexActor*		        createIosActor(NxApexScene& scene, NxIofxAsset* iofxAsset) = 0;
	//! \brief release a generic IOS NxApexActor
	virtual void						releaseIosActor(NxApexActor& actor) = 0;
	//! \brief get supports density
	virtual bool						getSupportsDensity() const = 0;
};

PX_POP_PACK

}
} // end namespace physx::apex

#endif // NX_INSTANCED_OBJECT_SIMULATION_ASSET_H
