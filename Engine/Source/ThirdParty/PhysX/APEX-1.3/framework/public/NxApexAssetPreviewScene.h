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

#ifndef NX_APEX_ASSET_PREVIEW_SCENE_H
#define NX_APEX_ASSET_PREVIEW_SCENE_H

/*!
\file
\brief classes NxApexScene, NxApexSceneStats, NxApexSceneDesc
*/

#include "NxApexDesc.h"
#include "NxApexRenderable.h"
#include "NxApexContext.h"
#include "foundation/PxVec3.h"
#include <NxApexDefs.h>

namespace physx
{
class PxCpuDispatcher;
class PxGpuDispatcher;
class PxTaskManager;
class PxBaseTask;
}

#if NX_SDK_VERSION_MAJOR == 2
class NxScene;
class NxDebugRenderable;
#elif NX_SDK_VERSION_MAJOR == 3
#include "PxFiltering.h"
namespace physx
{
class PxActor;
class PxScene;
class PxRenderBuffer;
}
#endif

namespace NxParameterized
{
class Interface;
}

namespace physx
{
namespace apex
{

PX_PUSH_PACK_DEFAULT


/**
\brief An APEX class for 
*/
class NxApexAssetPreviewScene : public NxApexInterface
{
public:
	/**
	\brief Sets the view matrix. Should be called whenever the view matrix needs to be updated.
	*/
	virtual void					setCameraMatrix(const physx::PxMat44& viewTransform) = 0;

	/**
	\brief Returns the view matrix set by the user for the given viewID.
	*/
	virtual physx::PxMat44			getCameraMatrix() const = 0;

	/**
	\brief Sets whether the asset preview should simply show asset names or many other parameter values
	*/
	virtual void					setShowFullInfo(bool showFullInfo) = 0;

	/**
	\brief Get the bool which determines whether the asset preview shows just asset names or parameter values
	*/
	virtual bool					getShowFullInfo() const = 0;
};


PX_POP_PACK
}
} // end namespace physx::apex

#endif // NX_APEX_SCENE_H
